#include "character_rina_shirt.hpp"
#include "character_rina_vest.hpp"
#include "character_sculpt.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/rina_anatomy.hpp"
#include "fury/generated/rina_hands.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>
namespace fury {

namespace {
using namespace character_detail;
// Original integration of the documented CC0 anatomy data. No upstream
// program logic is included. Surface queries fit Fury's own cosmetics/hair.
float anatomy_front(float x, float y) {
  float best = -1.f;
  for (std::size_t i = 0; i < rina_anatomy::indices.size(); i += 3) {
    const Vec3 a = rina_anatomy::positions[rina_anatomy::indices[i]];
    const Vec3 b = rina_anatomy::positions[rina_anatomy::indices[i + 1]];
    const Vec3 c = rina_anatomy::positions[rina_anatomy::indices[i + 2]];
    const float denominator =
        (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (std::fabs(denominator) < 1e-12f)
      continue;
    const float u =
        ((x - a.x) * (c.y - a.y) - (y - a.y) * (c.x - a.x)) / denominator;
    const float v =
        ((b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x)) / denominator;
    if (u >= -1e-5f && v >= -1e-5f && u + v <= 1.00001f)
      best = std::max(best, a.z + u * (b.z - a.z) + v * (c.z - a.z));
  }
  return best;
}
float anatomy_radius(float y, float angle, float center_z) {
  const Vec3 origin{0, y, center_z},
      direction{std::cos(angle), 0, std::sin(angle)};
  float best = 0.f;
  for (std::size_t i = 0; i < rina_anatomy::indices.size(); i += 3) {
    const Vec3 a = rina_anatomy::positions[rina_anatomy::indices[i]];
    const Vec3 b = rina_anatomy::positions[rina_anatomy::indices[i + 1]];
    const Vec3 c = rina_anatomy::positions[rina_anatomy::indices[i + 2]];
    if (y < std::min({a.y, b.y, c.y}) - 1e-6f ||
        y > std::max({a.y, b.y, c.y}) + 1e-6f)
      continue;
    const Vec3 e1 = b - a, e2 = c - a, p = cross(direction, e2);
    const float det = dot(e1, p);
    if (std::fabs(det) < 1e-12f)
      continue;
    const Vec3 t = origin - a;
    const float u = dot(t, p) / det;
    if (u < 0.f || u > 1.f)
      continue;
    const Vec3 q = cross(t, e1);
    const float v = dot(direction, q) / det;
    if (v < 0.f || u + v > 1.f)
      continue;
    const float d = dot(e2, q) / det;
    if (d > best)
      best = d;
  }
  return best;
}
CharacterInfluence rina_neck_weight(float y) {
  return y < .350f ? wt(B::Neck, B::Chest, smooth((y - .323f) / .027f))
                   : wt(B::Head, B::Neck, smooth((y - .350f) / .028f));
}
void make_head(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> corners;
  std::vector<std::vector<std::uint32_t>> groups(
      rina_anatomy::positions.size());
  for (std::size_t triangle = 0; triangle < rina_anatomy::indices.size();
       triangle += 3) {
    std::uint32_t out[3];
    for (int corner = 0; corner < 3; ++corner) {
      const auto index = rina_anatomy::indices[triangle + corner];
      const auto uv_index = rina_anatomy::uv_indices[triangle + corner];
      const auto key = std::make_pair(index, uv_index);
      auto found = corners.find(key);
      if (found == corners.end()) {
        const Vec3 position = rina_anatomy::positions[index];
        const Vec2 uv = rina_anatomy::uv_coordinates[uv_index];
        const float lip = g(position.x, 0, .015f) *
                          g(position.y, .39109f, .0035f) *
                          smooth((position.z - .078f) / .010f);
        // Small anatomical pigment zones retain the stable base complexion.
        const float cheeks = g(std::fabs(position.x), .030f, .015f) *
                             g(position.y, .404f, .015f) *
                             smooth((position.z - .067f) / .02f);
        const float nose = g(position.x, 0, .009f) *
                           g(position.y, .408f, .012f) *
                           smooth((position.z - .081f) / .013f);
        const float front = smooth((position.z - .054f) / .025f);
        const float vascular =
            .5f + .25f * std::sin(position.x * 360 + position.y * 97) +
            .25f * std::sin(position.y * 285 - position.z * 190);
        const float lid = g(std::fabs(position.x), .0178f, .008f) *
                          g(position.y, .42866f, .006f) * front;
        Vec3 color = skin * (1 - .025f * vascular * front);
        color.x *= 1 - .015f * lid;
        color.y *= 1 - .055f * lid;
        color.z *= 1 - .037f * lid;
        color.y *= 1 - .052f * cheeks - .043f * nose;
        color.z *= 1 - .077f * cheeks - .054f * nose;
        color.y *= 1 - .22f * lip;
        color.z *= 1 - .19f * lip;
        const auto vertex =
            b.vertex({position, color, rina_neck_weight(position.y),
                      character_surface_uv(S::Face, uv.x, uv.y)});
        groups[index].push_back(vertex);
        found = corners.emplace(key, vertex).first;
      }
      out[corner] = found->second;
    }
    b.tri(out[0], out[1], out[2]);
  }
  // Continue the complete original attachment loop into the shirt. The source
  // Face data remains untouched; this Skin underlay removes a visible cut edge
  // without forcing the collar onto the anatomical neck. Its lower ring stays
  // hidden inside the closed garment, as a deliberate body attachment boundary.
  auto boundary = rina_anatomy::attachment_boundary_indices;
  float area = 0;
  for (std::size_t i = 0; i < boundary.size(); ++i) {
    const Vec3 a = rina_anatomy::positions[boundary[i]];
    const Vec3 c = rina_anatomy::positions[boundary[(i + 1) % boundary.size()]];
    area += a.x * c.z - c.x * a.z;
  }
  if (area < 0)
    std::reverse(boundary.begin(), boundary.end());
  const auto underlay = std::uint32_t(b.m.bind_mesh.vertices.size());
  const int columns = int(boundary.size());
  b.grid(
      columns, 1,
      [&](float u, float v) {
        const auto i = std::size_t(std::lround(u * columns)) % boundary.size();
        Vec3 point = rina_anatomy::positions[boundary[i]];
        point.y -= .015f * (1 - v);
        return P{point, skin, rina_neck_weight(point.y),
                 character_surface_uv(S::Skin, u, v)};
      },
      true, false);
  // The source correspondence group performs the upper U seam average once.
  b.smooth_seams.pop_back();
  for (int i = 0; i <= columns; ++i)
    groups[boundary[std::size_t(i) % boundary.size()]].push_back(
        underlay + std::uint32_t(columns + 1 + i));
  for (auto &group : groups)
    if (group.size() > 1)
      b.normal_groups.push_back(std::move(group));
  for (std::size_t ei = 0; ei < rina_anatomy::eye_centers.size(); ++ei) {
    const Vec3 eye = rina_anatomy::eye_centers[ei] +
                     Vec3{ei == 0 ? .00065f : -.00065f, -.00046f, -.0015f};
    const Vec3 radius = rina_anatomy::eye_helper_half_extents[ei];
    b.ellipsoid(eye, radius, {.49f, .51f, .48f}, S::Eyes, B::Head,
                b.near ? 24 : 12, b.near ? 8 : 6);
    if (b.near) {
      std::vector<Vec3> upper_lid, lower_lid;
      const auto &rim = ei == 0 ? rina_anatomy::left_eye_opening
                                : rina_anatomy::right_eye_opening;
      for (Vec3 p : rim) {
        if (std::fabs(p.x - eye.x) > .0055f)
          continue;
        p.z += .00010f;
        (p.y >= .42866f ? upper_lid : lower_lid).push_back(p);
      }
      for (auto *line : {&upper_lid, &lower_lid})
        std::sort(line->begin(), line->end(),
                  [](Vec3 a, Vec3 c) { return a.x < c.x; });
      const Vec3 rim_color{skin.x * .85f, skin.y * .74f, skin.z * .72f};
      if (lower_lid.size() > 1)
        b.curve(lower_lid, .00010f, rim_color, S::Eyes, B::Head, 6, .6f);
      if (upper_lid.size() > 1)
        b.curve(upper_lid, .00009f, rim_color * .80f, S::Eyes, B::Head, 6, .6f);
    }
    const int iris_columns = b.near ? 24 : 8, iris_rows = b.near ? 5 : 3;
    const auto iris_base = std::uint32_t(b.m.bind_mesh.vertices.size());
    b.grid(
        iris_columns, iris_rows,
        [&](float u, float v) {
          const float near_r[]{.00005f, .00122f, .00140f,
                               .00225f, .00310f, .00355f};
          const float far_r[]{.00005f, .00122f, .00140f, .00355f};
          int row = std::min(iris_rows, int(std::lround(v * iris_rows)));
          float r = b.near ? near_r[row] : far_r[row], a = 2 * pi * u,
                x = r * std::cos(a), y = r * std::sin(a);
          float z = radius.z * std::sqrt(std::max(0.f, 1 - sq(x / radius.x) -
                                                           sq(y / radius.y))) +
                    .00015f;
          Vec3 color{.005f, .006f, .005f};
          if (r > .00130f) {
            float fiber =
                .88f + .16f * std::sin(a * 37 + .4f * std::sin(a * 11));
            color = b.m.appearance.eyes * fiber;
            if (r > .0031f)
              color = color * .38f;
          }
          return P{eye + Vec3{x, y, z}, color, wt(B::Head),
                   character_surface_uv(S::Eyes, u, v)};
        },
        true, false);
    auto center = b.vertex({eye + Vec3{0, 0, radius.z + .00015f},
                            {.005f, .006f, .005f},
                            wt(B::Head),
                            character_surface_uv(S::Eyes, .5f, .5f)});
    for (int i = 0; i < iris_columns; ++i)
      b.tri(center, iris_base + std::uint32_t(i),
            iris_base + std::uint32_t(i + 1));
    std::vector<Vec3> brow;
    std::vector<float> brow_radius;
    const int count = b.near ? 24 : 6;
    for (int i = 0; i <= count; ++i) {
      float t = float(i) / count, x = eye.x + lerp(-.0118f, .0118f, t),
            outward = eye.x > 0 ? t : 1 - t,
            y = eye.y + .0122f + .0025f * std::sin(pi * outward) -
                .0011f * outward;
      float z = anatomy_front(x, y);
      brow.push_back({x, y, z + .0004f});
      brow_radius.push_back(
          .00012f + .00088f * std::pow(std::max(0.f, std::sin(pi * t)), .6f));
    }
    b.tube(
        brow, brow_radius, .35f, b.m.appearance.hair * .74f, S::Hair,
        [](float) { return wt(B::Head); }, b.near ? 6 : 4);
  }
}

Vec3 fit_hand_point(Vec3 p, float side) {
  const float angle = side * (58.f * pi / 180.f);
  p.x *= side;
  return {p.x * std::cos(angle) + p.z * std::sin(angle), p.y,
          -p.x * std::sin(angle) + p.z * std::cos(angle)};
}
void make_hands_and_forearms(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  for (float side : {-1.f, 1.f}) {
    const B fore = side > 0 ? B::RightForearm : B::LeftForearm;
    const B upper = side > 0 ? B::RightUpperArm : B::LeftUpperArm;
    const B hand = side > 0 ? B::RightHand : B::LeftHand;
    const Vec3 wrist{side * .164f, -.039f, .008f};
    std::map<std::pair<unsigned, unsigned>, unsigned> corners;
    std::vector<std::vector<unsigned>> groups(rina_hands::positions.size());
    const auto emit = [&](auto const &indices, auto const &uv_indices) {
      for (std::size_t t = 0; t < indices.size(); t += 3) {
        unsigned out[3];
        for (int k = 0; k < 3; ++k) {
          auto index = indices[t + k], uv = uv_indices[t + k];
          const bool hard =
              b.near &&
              std::find(rina_hands::hard_normal_triangle_indices.begin(),
                        rina_hands::hard_normal_triangle_indices.end(),
                        t / 3) !=
                  rina_hands::hard_normal_triangle_indices.end();
          auto key = std::make_pair(
              index,
              uv + (hard ? unsigned((t + 1) * rina_hands::uv_coordinates.size())
                         : 0));
          auto found = corners.find(key);
          if (found == corners.end()) {
            Vec3 p = fit_hand_point(rina_hands::positions[index], side);
            p += wrist;
            const Vec2 tex = rina_hands::uv_coordinates[uv];
            auto i = b.vertex({p, skin, wt(hand),
                               character_surface_uv(S::Skin, tex.x, tex.y)});
            if (!hard)
              groups[index].push_back(i);
            found = corners.emplace(key, i).first;
          }
          out[k] = found->second;
        }
        if (side > 0)
          b.tri(out[0], out[1], out[2]);
        else
          b.tri(out[0], out[2], out[1]);
      }
    };
    if (b.near)
      emit(rina_hands::near_indices, rina_hands::near_uv_indices);
    else
      emit(rina_hands::far_indices, rina_hands::far_uv_indices);
    // Exactly reuse every anatomical wrist point at the terminal forearm ring.
    // The opposite terminal ring tucks inside the open rolled cuff, not a cap.
    auto boundary = rina_hands::wrist_boundary_indices;
    float area = 0;
    for (std::size_t i = 0; i < boundary.size(); ++i) {
      auto a = rina_hands::positions[boundary[i]],
           c = rina_hands::positions[boundary[(i + 1) % boundary.size()]];
      area += a.x * c.z - c.x * a.z;
    }
    if (area * side < 0)
      std::reverse(boundary.begin(), boundary.end());
    Vec3 center{};
    for (auto i : boundary)
      center += rina_hands::positions[i];
    center = center * (1.f / boundary.size());
    const unsigned start = b.m.bind_mesh.vertices.size();
    const int columns = boundary.size(), rows = b.near ? 16 : 6;
    b.grid(
        columns, rows,
        [&](float u, float v) {
          const auto index =
              boundary[std::size_t(std::lround(u * columns)) % boundary.size()];
          Vec3 source = fit_hand_point(rina_hands::positions[index], side);
          source += wrist;
          const Vec3 local =
              fit_hand_point(rina_hands::positions[index] - center, side);
          const float a = std::atan2(local.z, local.x);
          const float y = lerp(source.y, .144f, v),
                      radius = .014f + .0105f * g(y, .078f, .071f);
          const float x = side * lerp(.164f, .145f, smooth((y + .039f) / .18f));
          Vec3 target{x + radius * std::cos(a), y,
                      .006f + radius * .83f * std::sin(a)};
          Vec3 p = source * (1 - smooth(v / .28f)) + target * smooth(v / .28f);
          CharacterInfluence w =
              y < -.015f ? wt(hand, fore, 1 - smooth((y + .039f) / .04f))
                         : wt(fore, upper, smooth((.163f - y) / .081f));
          if (v == 0)
            w = wt(hand);
          return P{p, skin, w, character_surface_uv(S::Skin, u, v)};
        },
        true, false);
    // Suppress the duplicate periodic seam average at the wrist row.
    b.smooth_seams.erase(b.smooth_seams.end() - (rows + 1));
    for (int i = 0; i <= columns; ++i)
      groups[boundary[i % columns]].push_back(start + i);
    for (auto &g : groups)
      if (g.size() > 1)
        b.normal_groups.push_back(std::move(g));
  }
}
rina_vest_detail::LegRanges make_trousers_and_shoes(Sculpt &b) {
  rina_vest_detail::LegRanges support;
  const Vec3 cloth = b.m.appearance.trousers;
  const int columns = b.near ? 28 : 12, rows = b.near ? 24 : 13;
  constexpr float near_y[]{-.459f, -.440f, -.420f, -.390f, -.355f,
                           -.325f, -.300f, -.280f, -.265f, -.250f,
                           -.235f, -.225f, -.215f, -.200f, -.185f,
                           -.170f, -.150f, -.130f, -.110f, -.090f};
  constexpr float far_y[]{-.459f, -.420f, -.370f, -.320f, -.280f, -.255f,
                          -.225f, -.200f, -.175f, -.135f, -.090f};
  std::array<std::vector<unsigned>, 2> seams;
  for (float side : {-1.f, 1.f}) {
    const B thigh = side > 0 ? B::RightThigh : B::LeftThigh,
            shin = side > 0 ? B::RightShin : B::LeftShin,
            foot = side > 0 ? B::RightFoot : B::LeftFoot;
    const auto base = unsigned(b.m.bind_mesh.vertices.size());
    const auto leg_first_triangle = b.m.bind_mesh.indices.size();
    b.grid(
        columns, rows,
        [&](float u, float v) {
          float a = u == 1.f ? 0.f : 2 * pi * u, cs = std::cos(a),
                sn = std::sin(a);
          const int row = int(std::lround(v * rows)),
                    leg_rows = b.near ? 19 : 10;
          const float y = b.near ? near_y[std::min(row, leg_rows)]
                                 : far_y[std::min(row, leg_rows)];
          float radius = .0255f + .0095f * g(y, -.34f, .073f) +
                         .0205f * smooth((y + .24f) / .19f);
          const float fold =
              .0018f * g(y, -.22f + .012f * cs, .025f) * sn +
              .0012f * g(y, -.446f, .021f) * std::sin(2 * a + y * 210);
          Vec3 p{side * (.054f + (radius + fold) * cs), y,
                 (radius * 1.10f + fold) * sn - .002f +
                     .013f * g(y, -.225f, .19f) * std::max(0.f, -sn)};
          const float join =
              smooth(float(row - leg_rows) / float(rows - leg_rows));
          const Vec3 top{side * .098f * std::max(0.f, cs),
                         .057f - .092f * std::max(0.f, -cs),
                         .063f * sn - .002f};
          p = p * (1 - join) + top * join;
          // A local anterior bend and broader posterior blend distribute
          // compression around the knee; whole calf/thigh slabs must not
          // rotate into one another in the deep-crouch diagnostic.
          const float knee_span = .100f + .120f * std::max(0.f, -sn);
          const float knee_low = -.225f - .5f * knee_span;
          const float knee_high = -.225f + .5f * knee_span;
          CharacterInfluence w =
              y < knee_low ? wt(shin)
              : y < knee_high
                  ? wt(shin, thigh, 1 - smooth((y - knee_low) / knee_span))
                  : wt(thigh, B::Pelvis, 1 - join);
          if (v == 1)
            w = wt(B::Pelvis);
          return P{p, cloth, w, character_surface_uv(S::Pants, u, v)};
        },
        true, false);
    // Mirroring reverses winding while preserving exact shared crotch edges.
    if (side < 0)
      for (std::size_t i = b.m.bind_mesh.indices.size() - columns * rows * 6;
           i < b.m.bind_mesh.indices.size(); i += 3)
        std::swap(b.m.bind_mesh.indices[i + 1], b.m.bind_mesh.indices[i + 2]);
    support[side > 0 ? 1 : 0] = {leg_first_triangle,
                                 b.m.bind_mesh.indices.size()};
    auto &seam = seams[side > 0 ? 1 : 0];
    for (int i = columns / 4; i <= 3 * columns / 4; ++i)
      seam.push_back(base + rows * (columns + 1) + i);
    const float x = side * .054f;
    b.grid(b.near ? 28 : 10, 3, [&](float u, float v) {
      float a = 2 * pi * u;
      return P{{x + .032f * std::cos(a), lerp(-.5f, -.481f, v),
                .029f + .067f * std::sin(a)},
               {.061f, .065f, .069f},
               wt(foot),
               character_surface_uv(S::Rubber, u, v)};
    });
    const auto boot_first_triangle = b.m.bind_mesh.indices.size();
    b.grid(b.near ? 32 : 10, b.near ? 10 : 3, [&](float u, float v) {
      float a = 2 * pi * u, yy = lerp(-.481f, -.420f, v);
      return P{{x + lerp(.032f, .024f, smooth(v)) * std::cos(a), yy,
                lerp(.029f, .004f, smooth(v)) +
                    lerp(.066f, .027f, smooth(v)) * std::sin(a)},
               b.m.appearance.shoes,
               wt(foot),
               character_surface_uv(S::Leather, u, v)};
    });
    const auto boot_last_triangle = b.m.bind_mesh.indices.size();
    const auto boot_top = [&](float px, float pz) {
      float highest = -1.f;
      const auto &mesh = b.m.bind_mesh;
      for (std::size_t t = boot_first_triangle; t < boot_last_triangle;
           t += 3) {
        Vec3 a = mesh.vertices[mesh.indices[t]].position * (1 / b.h),
             c = mesh.vertices[mesh.indices[t + 1]].position * (1 / b.h),
             d = mesh.vertices[mesh.indices[t + 2]].position * (1 / b.h);
        const Vec3 e = c - a, f = d - a;
        float det = e.x * f.z - e.z * f.x;
        if (std::fabs(det) < 1e-12f)
          continue;
        float u = ((px - a.x) * f.z - (pz - a.z) * f.x) / det,
              v = (e.x * (pz - a.z) - e.z * (px - a.x)) / det;
        if (u >= -1e-6f && v >= -1e-6f && u + v <= 1.000001f)
          highest = std::max(highest, a.y + u * e.y + v * f.y);
      }
      if (highest < -.6f)
        throw std::runtime_error("Rina boot detail lacks upper support");
      return highest;
    };
    // A narrow welt separates the resilient sole from the sewn upper. Its
    // volume follows the real toe/heel outline, rather than a flat trim box.
    b.grid(
        b.near ? 24 : 10, 1,
        [&](float u, float v) {
          float a = 2 * pi * u;
          return P{{x + .0325f * std::cos(a), lerp(-.482f, -.4785f, v),
                    .029f + .0672f * std::sin(a)},
                   b.m.appearance.shoes * .65f,
                   wt(foot),
                   character_surface_uv(S::Leather, u, v)};
        },
        true, false);
    b.grid(
        b.near ? 4 : 2, b.near ? 6 : 2,
        [&](float u, float v) {
          float px = x + lerp(-.0115f, .0115f, u), z = lerp(.023f, .056f, v);
          return P{{px, boot_top(px, z) + .00045f, z},
                   b.m.appearance.shoes * .87f,
                   wt(foot),
                   character_surface_uv(S::Leather, u, v)};
        },
        false, false);
    // A shaped ankle underlay bridges the trouser opening to the moving shoe.
    // Its weights continue the shin-to-foot bend, avoiding an empty cuff gap.
    b.grid(
        b.near ? 20 : 12, b.near ? 6 : 3,
        [&](float u, float v) {
          float a = 2 * pi * u, y = lerp(-.477f, -.432f, v),
                rx = lerp(.024f, .0228f, v);
          return P{{x + rx * std::cos(a), y, .005f + rx * .93f * std::sin(a)},
                   Vec3{.071f, .078f, .083f},
                   wt(foot, shin, 1 - smooth((y + .469f) / .030f)),
                   character_surface_uv(S::Shirt, u, v)};
        },
        true, false);
    // Individually fitted laces lie over the actual LOD upper and tongue.
    // Their prior fixed coordinates had disappeared inside the raised boot.
    for (int lace = 0; lace < (b.near ? 4 : 1); ++lace) {
      float z = .030f + lace * .007f;
      std::vector<Vec3> path;
      const int segments = b.near ? 4 : 2;
      for (int i = 0; i <= segments; ++i) {
        float t = float(i) / segments, px = x + lerp(-.011f, .011f, t),
              pz = z + .0014f * std::sin(pi * t);
        path.push_back({px, boot_top(px, pz) + .0010f, pz});
      }
      b.curve(path, .00050f, {.10f, .09f, .075f}, S::Shirt, foot,
              b.near ? 6 : 4);
    }
  }
  for (std::size_t i = 0; i < seams[0].size(); ++i)
    b.normal_groups.push_back({seams[0][i], seams[1][i]});
  b.grid(
      b.near ? 48 : 20, 3,
      [&](float u, float v) {
        float a = 2 * pi * u;
        return P{{.0985f * std::cos(a), lerp(.0565f, .071f, v),
                  .064f * std::sin(a) - .002f},
                 cloth * .94f,
                 wt(B::Pelvis),
                 character_surface_uv(S::Pants, u, v)};
      },
      true, false);
  return support;
}
float rina_hairline(float angle) {
  const float front = std::max(0.f, std::sin(angle));
  const float rear = std::max(0.f, -std::sin(angle));
  // Short sideburns and a low, complete nape; an irregular boundary follows
  // the skull rather than exposing the generic model's rectangular rear gap.
  return .437f + .021f * front - .033f * rear +
         .006f * std::pow(std::fabs(std::cos(angle)), 8.f) +
         .0007f * std::sin(angle * 9 + .8f) + .0004f * std::sin(angle * 17);
}
Vec3 rina_crop(float angle, float t, float lift = 0.f) {
  const float y = lerp(rina_hairline(angle), .500f,
                       std::sin(std::clamp(t, 0.f, 1.f) * pi * .5f));
  const float cz =
      lerp(rina_anatomy::head_center.z, rina_anatomy::crown_joint.z,
           smooth((y - .455f) / .038f));
  // Map the whole upper skull continuously into the reserved short-hair
  // envelope. Clamping one source section produced a visible crown step.
  const float root_ease = smooth(t / .20f);
  const float source_y =
      y - .007f * smooth((y - .446f) / .054f) * smooth(t / .30f);
  const float fade =
      std::sqrt(std::max(0.f, 1 - sq(std::max(0.f, (y - .478f) / .022f))));
  float radius =
      anatomy_radius(std::min(source_y, .49295f), angle, cz) +
      ((.0019f + .0006f * smooth(t / .30f)) * (.38f + .62f * root_ease) +
       lift) *
          fade;
  radius += .00045f * std::sin(angle * 3 + t * 3) * std::sin(pi * t);
  if (y > .497f)
    radius *= std::sqrt(std::max(0.f, (.500f - y) / .003f));
  radius = std::max(.00022f, radius);
  return {radius * std::cos(angle), y, cz + radius * std::sin(angle)};
}
void make_rina_hair(Sculpt &b) {
  const Vec3 dark_hair = b.m.appearance.hair;
  const int sectors = 8, columns = b.near ? 8 : 3, rows = b.near ? 16 : 5;
  // The coarse Far chords otherwise cut through the convex source scalp,
  // including the lower rear seams. Clearance stays radial and tapers with
  // the existing crown profile, preserving the exact height and topology.
  const float cap_clearance = b.near ? 0.f : .003f;
  const auto cap_first_triangle = b.m.bind_mesh.indices.size();
  std::array<std::uint32_t, sectors> starts{};
  for (int sector = 0; sector < sectors; ++sector) {
    starts[sector] = std::uint32_t(b.m.bind_mesh.vertices.size());
    b.grid(
        columns, rows,
        [&](float u, float v) {
          const float a = 2 * pi * (float(sector) + u) / sectors;
          const float t = .038f + .962f * v;
          const float root = .69f + .11f * smooth(t / .35f);
          return P{rina_crop(a, t, cap_clearance), dark_hair * root,
                   wt(B::Head), character_surface_uv(S::HairCrop, u, v)};
        },
        false, false);
  }
  for (int sector = 0; sector < sectors; ++sector)
    for (int row = 0; row <= rows; ++row) {
      const auto first = starts[sector] + std::uint32_t(row * (columns + 1));
      const auto last = starts[(sector + sectors - 1) % sectors] +
                        std::uint32_t(row * (columns + 1) + columns);
      b.m.bind_mesh.vertices[last].position =
          b.m.bind_mesh.vertices[first].position;
      b.normal_groups.push_back({first, last});
    }
  const auto pole = b.vertex({{0, .500f, rina_anatomy::crown_joint.z},
                              dark_hair * .80f,
                              wt(B::Head),
                              character_surface_uv(S::HairCrop, .5f, 1.f)});
  for (int sector = 0; sector < sectors; ++sector)
    for (int col = 0; col < columns; ++col) {
      const auto a = starts[sector] + std::uint32_t(rows * (columns + 1) + col);
      b.tri(a, pole, a + 1);
    }
  const auto cap_last_triangle = b.m.bind_mesh.indices.size();
  // Fit the cards to the polygonal cap actually sent to every renderer.
  // Analytic scalp fitting alone leaves broad path-traced shadow bars above
  // coarse triangle chords. Edge roots nest inside the cap; tips alone extend
  // onto the original anatomical scalp with a fraction-millimetre clearance.
  const auto fit_card = [&](Vec3 p, float angle, bool edge) {
    const float cz =
        lerp(rina_anatomy::head_center.z, rina_anatomy::crown_joint.z,
             smooth((p.y - .455f) / .038f));
    const Vec3 origin{0, p.y, cz},
        direction{std::cos(angle), 0, std::sin(angle)};
    float best = 0;
    for (std::size_t i = cap_first_triangle; i < cap_last_triangle; i += 3) {
      const auto &mesh = b.m.bind_mesh;
      Vec3 a = mesh.vertices[mesh.indices[i]].position * (1 / b.h),
           c = mesh.vertices[mesh.indices[i + 1]].position * (1 / b.h),
           d = mesh.vertices[mesh.indices[i + 2]].position * (1 / b.h);
      if (p.y < std::min({a.y, c.y, d.y}) || p.y > std::max({a.y, c.y, d.y}))
        continue;
      Vec3 e = c - a, f = d - a, q = cross(direction, f);
      float det = dot(e, q);
      if (std::fabs(det) < 1e-12f)
        continue;
      Vec3 v = origin - a, z = cross(v, e);
      float u = dot(v, q) / det, w = dot(direction, z) / det;
      if (u >= -1e-6f && w >= -1e-6f && u + w <= 1.000001f)
        best = std::max(best, dot(f, z) / det);
    }
    if (best > 0)
      return origin + direction * (best + (edge ? -.00008f : .00010f));
    const float skin = anatomy_radius(p.y, angle, cz);
    if (skin <= 0)
      throw std::runtime_error("Rina hair tip lacks source scalp support");
    return origin + direction * (skin + .00008f);
  };
  // Short growth follows a restrained offset whorl. Low-amplitude overlapping
  // cards soften silhouette and temple/nape transitions without bald openings.
  const int strands = b.near ? 112 : 12;
  for (int k = 0; k < strands; ++k) {
    const float fraction = std::fmod(float(k) * .6180339887f, 1.f);
    const float a0 = float(k) * 2.39996323f;
    const float start =
        b.near ? (k < 64 ? .095f : .38f + .55f * fraction) : .11f;
    const float finish =
        start < .20f ? -.012f - .018f * (.5f + .5f * std::sin(float(k) * 1.91f))
                     : start - .23f;
    const float width =
        (b.near ? .074f : .21f) * (.87f + .12f * std::sin(float(k) * 1.37f));
    b.grid(
        2, b.near ? 5 : 3,
        [&](float u, float v) {
          const float t = lerp(start, finish, v);
          const float swirl = .085f * std::sin(pi * v) * smooth(start / .65f);
          const float a = a0 + swirl + (.5f - u) * width;
          const float lift = .000025f + .00014f * std::sin(pi * v);
          Vec3 p = rina_crop(a, std::clamp(t, 0.f, 1.f), lift);
          if (t < 0)
            p.y += .065f * t;
          p = fit_card(p, a, start < .20f);
          const float root = .68f + .16f * smooth(v / .4f);
          const float variation = .96f + .045f * std::sin(float(k) * 2.11f);
          return P{p, dark_hair * (root * variation), wt(B::Head),
                   character_surface_uv(S::HairShortStrands, u, v)};
        },
        false, false);
  }
}


// Fit the emitted lower shirt after the vest has consumed its original support.
// The finite waistband triangles are authoritative at each LOD. Only the first
// two body rows move; the narrow turned return follows its original outer row.
void fit_rina_lower_shirt(Sculpt &b, std::size_t shirt_end,
                          std::size_t waistband_begin,
                          std::size_t waistband_end) {
  auto &mesh = b.m.bind_mesh;
  const auto displacement = [&](Vec3 p) {
    const Vec3 origin{0, std::clamp(p.y, .0566f, .0709f), -.002f};
    const Vec3 radial{p.x, 0, p.z - origin.z};
    const float radius = length(radial);
    const Vec3 direction = radial * (1 / radius);
    float boundary = 0;
    for (std::size_t t = waistband_begin; t < waistband_end; t += 3) {
      const Vec3 a = mesh.vertices[mesh.indices[t]].position * (1 / b.h),
                 c = mesh.vertices[mesh.indices[t + 1]].position * (1 / b.h),
                 d = mesh.vertices[mesh.indices[t + 2]].position * (1 / b.h);
      const Vec3 e = c - a, f = d - a, q = cross(direction, f);
      const float det = dot(e, q);
      if (std::fabs(det) < 1e-12f)
        continue;
      const Vec3 v = origin - a, z = cross(v, e);
      const float u = dot(v, q) / det, w = dot(direction, z) / det;
      if (u >= -1e-6f && w >= -1e-6f && u + w <= 1.000001f)
        boundary = std::max(boundary, dot(f, z) / det);
    }
    if (boundary <= 0)
      throw std::runtime_error("Rina lower shirt lacks finite waistband support");
    const float excess = radius - (boundary - .003f);
    // C1 angular onset avoids an abrupt clamp where the original fit is clear.
    const float tuck = excess <= -.001f ? 0.f
                      : excess >= .001f ? excess
                                        : sq(excess + .001f) / .004f;
    const float transition = 1 - smooth((p.y - .071f) / .024f);
    return direction * (-tuck * transition);
  };
  struct HemAnchor { Vec3 original, delta; };
  std::vector<HemAnchor> anchors;
  for (std::size_t i = 0; i < shirt_end; ++i) {
    const Vec3 p = mesh.vertices[i].position * (1 / b.h);
    if (std::fabs(p.y - .057f) < 1e-6f)
      anchors.push_back({p, displacement(p)});
  }
  if (anchors.empty())
    throw std::runtime_error("Rina lower shirt lacks its original hem ring");
  const float top = b.near ? .071801f : .081668f;
  for (std::size_t i = 0; i < shirt_end; ++i) {
    const Vec3 p = mesh.vertices[i].position * (1 / b.h);
    if (p.y > top)
      continue;
    Vec3 delta;
    if (std::fabs(p.y - .0567f) < 1e-6f ||
        std::fabs(p.y - .064f) < 1e-6f) {
      // The return was authored along a ray from (0,y,0). Choose its exact
      // outer angular neighbor, then translate all three rings identically.
      const Vec3 direction = normalize(Vec3{p.x, 0, p.z});
      float best = -2;
      for (const auto &anchor : anchors) {
        const float alignment = dot(
            direction, normalize(Vec3{anchor.original.x, 0, anchor.original.z}));
        if (alignment > best) {
          best = alignment;
          delta = anchor.delta;
        }
      }
    } else {
      delta = displacement(p);
    }
    if (dot(delta, delta) > 0)
      mesh.vertices[i].position += delta * b.h;
  }
}

CharacterRig rina_rig(float height) {
  CharacterRig r;
  r.height = height;
  auto joint = [&](B bone, int parent, Vec3 p) {
    r.joints[bone_index(bone)] = {parent, p * height};
  };
  auto child = [&](B bone, B parent, Vec3 p) {
    joint(bone, int(bone_index(parent)), p);
  };
  joint(B::Pelvis, -1, {0, 0, 0});
  child(B::Spine, B::Pelvis, {0, .13f, 0});
  child(B::Chest, B::Spine, {0, .28f, 0});
  child(B::Neck, B::Chest, rina_anatomy::neck_center);
  child(B::Head, B::Neck, rina_anatomy::head_center);
  for (float sign : {-1.f, 1.f}) {
    bool q = sign > 0;
    B upper = q ? B::RightUpperArm : B::LeftUpperArm,
      fore = q ? B::RightForearm : B::LeftForearm,
      hand = q ? B::RightHand : B::LeftHand,
      thigh = q ? B::RightThigh : B::LeftThigh,
      shin = q ? B::RightShin : B::LeftShin,
      foot = q ? B::RightFoot : B::LeftFoot;
    child(upper, B::Chest, {sign * .114f, .322f, 0});
    child(fore, upper, {sign * .145f, .12f, .006f});
    child(hand, fore, {sign * .164f, -.039f, .008f});
    child(thigh, B::Pelvis, {sign * .054f, 0, 0});
    child(shin, thigh, {sign * .054f, -.225f, 0});
    child(foot, shin, {sign * .054f, -.46f, 0});
  }
  return r;
}
} // namespace
CharacterProfile make_rina_character_profile(float height, CharacterRole role,
                                             std::string_view identity,
                                             CharacterLod lod) {
  if (!std::isfinite(height) || height <= 0 ||
      (lod != CharacterLod::Near && lod != CharacterLod::Far))
    throw std::invalid_argument("Invalid individual character request");
  CharacterProfile p;
  auto &m = p.model;
  m.role = role;
  m.lod = lod;
  m.seed = character_seed(identity);
  m.appearance = character_appearance(m.seed);
  m.rig = rina_rig(height);
  m.dual_quaternion_skinning = true;
  m.bind_mesh.vertices.reserve(lod == CharacterLod::Near ? 34000 : 17000);
  m.bind_mesh.indices.reserve(lod == CharacterLod::Near ? 120000 : 48000);
  m.influences.reserve(m.bind_mesh.vertices.capacity());
  Sculpt b{m, height, lod == CharacterLod::Near, {}, {}};
  const auto shirt =
      make_rina_shirt(b, anatomy_radius, rina_anatomy::neck_center);
  const auto shirt_vertex_end = m.bind_mesh.vertices.size();
  make_hands_and_forearms(b);
  const auto trousers = make_trousers_and_shoes(b);
  const auto waistband_end = m.bind_mesh.indices.size();
  const auto waistband_begin = waistband_end - (b.near ? 48 : 20) * 18;
  make_head(b);
  make_rina_hair(b);
  make_rina_vest(b, shirt, trousers);
  fit_rina_lower_shirt(b, shirt_vertex_end, waistband_begin, waistband_end);
  b.finish();
  p.material = make_character_surface_material();
  p.material.alpha_cutoff = .4f;
  p.lod_distance = 12;
  p.individualized = true;
  return p;
}
} // namespace fury
