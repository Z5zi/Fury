#include "character_nell_shirt.hpp"
#include "character_sculpt.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/nell_anatomy.hpp"
#include "fury/generated/nell_hands.hpp"
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
  for (std::size_t i = 0; i < nell_anatomy::indices.size(); i += 3) {
    const Vec3 a = nell_anatomy::positions[nell_anatomy::indices[i]];
    const Vec3 b = nell_anatomy::positions[nell_anatomy::indices[i + 1]];
    const Vec3 c = nell_anatomy::positions[nell_anatomy::indices[i + 2]];
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
  for (std::size_t i = 0; i < nell_anatomy::indices.size(); i += 3) {
    const Vec3 a = nell_anatomy::positions[nell_anatomy::indices[i]];
    const Vec3 b = nell_anatomy::positions[nell_anatomy::indices[i + 1]];
    const Vec3 c = nell_anatomy::positions[nell_anatomy::indices[i + 2]];
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
CharacterInfluence nell_neck_weight(float y) {
  return y < .349f ? wt(B::Neck, B::Chest, smooth((y - .324f) / .025f))
                   : wt(B::Head, B::Neck, smooth((y - .349f) / .027f));
}
void make_head(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> corners;
  std::vector<std::vector<std::uint32_t>> groups(
      nell_anatomy::positions.size());
  for (std::size_t triangle = 0; triangle < nell_anatomy::indices.size();
       triangle += 3) {
    std::uint32_t out[3];
    for (int corner = 0; corner < 3; ++corner) {
      const auto index = nell_anatomy::indices[triangle + corner];
      const auto uv_index = nell_anatomy::uv_indices[triangle + corner];
      const auto key = std::make_pair(index, uv_index);
      auto found = corners.find(key);
      if (found == corners.end()) {
        const Vec3 position = nell_anatomy::positions[index];
        const Vec2 uv = nell_anatomy::uv_coordinates[uv_index];
        const float lip = g(position.x, 0, .015f) *
                          g(position.y, .38386f, .0035f) *
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
                          g(position.y, .423f, .006f) * front;
        Vec3 color = skin * (1 - .025f * vascular * front);
        color.x *= 1 - .015f * lid;
        color.y *= 1 - .055f * lid;
        color.z *= 1 - .037f * lid;
        color.y *= 1 - .052f * cheeks - .043f * nose;
        color.z *= 1 - .077f * cheeks - .054f * nose;
        color.y *= 1 - .22f * lip;
        color.z *= 1 - .19f * lip;
        const auto vertex =
            b.vertex({position, color, nell_neck_weight(position.y),
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
  auto boundary = nell_anatomy::attachment_boundary_indices;
  float area = 0;
  for (std::size_t i = 0; i < boundary.size(); ++i) {
    const Vec3 a = nell_anatomy::positions[boundary[i]];
    const Vec3 c = nell_anatomy::positions[boundary[(i + 1) % boundary.size()]];
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
        Vec3 point = nell_anatomy::positions[boundary[i]];
        point.y -= .015f * (1 - v);
        return P{point, skin, nell_neck_weight(point.y),
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
  for (std::size_t ei = 0; ei < nell_anatomy::eye_centers.size(); ++ei) {
    const Vec3 eye = nell_anatomy::eye_centers[ei] +
                     Vec3{ei == 0 ? .00065f : -.00065f, -.00065f, -.00115f};
    const Vec3 radius = nell_anatomy::eye_helper_half_extents[ei];
    b.ellipsoid(eye, radius, {.79f, .80f, .77f}, S::Eyes, B::Head,
                b.near ? 24 : 12, b.near ? 8 : 6);
    if (b.near) {
      std::vector<Vec3> upper_lid, lower_lid;
      const auto &rim = ei == 0 ? nell_anatomy::left_eye_opening
                                : nell_anatomy::right_eye_opening;
      for (Vec3 p : rim) {
        if (std::fabs(p.x - eye.x) > .0055f)
          continue;
        p.z += .00010f;
        (p.y >= .42382f ? upper_lid : lower_lid).push_back(p);
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
    const int count = b.near ? 24 : 8;
    for (int i = 0; i <= count; ++i) {
      float t = float(i) / count, x = eye.x + lerp(-.0118f, .0118f, t),
            outward = eye.x > 0 ? t : 1 - t,
            y = eye.y + .0130f + .0032f * std::sin(pi * outward) -
                .0017f * outward;
      float z = anatomy_front(x, y);
      brow.push_back({x, y, z + .0004f});
      brow_radius.push_back(
          .00012f + .00112f * std::pow(std::max(0.f, std::sin(pi * t)), .6f));
    }
    b.tube(
        brow, brow_radius, .35f, b.m.appearance.hair * .74f, S::Hair,
        [](float) { return wt(B::Head); }, b.near ? 6 : 4);
  }
}

void make_hands_and_forearms(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  for (float side : {-1.f, 1.f}) {
    const B fore = side > 0 ? B::RightForearm : B::LeftForearm;
    const B upper = side > 0 ? B::RightUpperArm : B::LeftUpperArm;
    const B hand = side > 0 ? B::RightHand : B::LeftHand;
    const Vec3 wrist{side * .151f, -.043f, .008f};
    std::map<std::pair<unsigned, unsigned>, unsigned> corners;
    std::vector<std::vector<unsigned>> groups(nell_hands::positions.size());
    const auto emit = [&](auto const &indices, auto const &uv_indices) {
      for (std::size_t t = 0; t < indices.size(); t += 3) {
        unsigned out[3];
        for (int k = 0; k < 3; ++k) {
          auto index = indices[t + k], uv = uv_indices[t + k];
          const bool hard =
              b.near &&
              std::find(nell_hands::hard_normal_triangle_indices.begin(),
                        nell_hands::hard_normal_triangle_indices.end(),
                        t / 3) !=
                  nell_hands::hard_normal_triangle_indices.end();
          auto key = std::make_pair(
              index,
              uv + (hard ? unsigned((t + 1) * nell_hands::uv_coordinates.size())
                         : 0));
          auto found = corners.find(key);
          if (found == corners.end()) {
            Vec3 p = nell_hands::positions[index];
            p.x *= side;
            p += wrist;
            const Vec2 tex = nell_hands::uv_coordinates[uv];
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
      emit(nell_hands::near_indices, nell_hands::near_uv_indices);
    else
      emit(nell_hands::far_indices, nell_hands::far_uv_indices);
    // Exactly reuse every anatomical wrist point at the terminal forearm ring.
    // The opposite terminal ring tucks inside the open rolled cuff, not a cap.
    auto boundary = nell_hands::wrist_boundary_indices;
    float area = 0;
    for (std::size_t i = 0; i < boundary.size(); ++i) {
      auto a = nell_hands::positions[boundary[i]],
           c = nell_hands::positions[boundary[(i + 1) % boundary.size()]];
      area += a.x * c.z - c.x * a.z;
    }
    if (area * side < 0)
      std::reverse(boundary.begin(), boundary.end());
    Vec3 center{};
    for (auto i : boundary)
      center += nell_hands::positions[i];
    center = center * (1.f / boundary.size());
    const unsigned start = b.m.bind_mesh.vertices.size();
    const int columns = boundary.size(), rows = b.near ? 16 : 3;
    b.grid(
        columns, rows,
        [&](float u, float v) {
          const auto index =
              boundary[std::size_t(std::lround(u * columns)) % boundary.size()];
          Vec3 source = nell_hands::positions[index];
          source.x *= side;
          source += wrist;
          const Vec3 local = nell_hands::positions[index] - center;
          const float a = std::atan2(local.z, side * local.x);
          const float y = lerp(source.y, .142f, v),
                      radius = .014f + .0105f * g(y, .078f, .071f);
          const float x = side * lerp(.151f, .135f, smooth((y + .043f) / .18f));
          Vec3 target{x + radius * std::cos(a), y,
                      .006f + radius * .83f * std::sin(a)};
          Vec3 p = source * (1 - smooth(v / .28f)) + target * smooth(v / .28f);
          CharacterInfluence w =
              y < -.015f ? wt(hand, fore, 1 - smooth((y + .043f) / .04f))
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
void make_trousers_and_shoes(Sculpt &b) {
  const Vec3 cloth = b.m.appearance.trousers;
  // Quarter-ring vertices are required for the exact center seat/waist join.
  const int columns = b.near ? 28 : 12, rows = b.near ? 24 : 8;
  std::array<std::vector<unsigned>, 2> seams;
  for (float side : {-1.f, 1.f}) {
    const B thigh = side > 0 ? B::RightThigh : B::LeftThigh,
            shin = side > 0 ? B::RightShin : B::LeftShin,
            foot = side > 0 ? B::RightFoot : B::LeftFoot;
    const auto base = unsigned(b.m.bind_mesh.vertices.size());
    b.grid(
        columns, rows,
        [&](float u, float v) {
          float a = 2 * pi * u, cs = std::cos(a), sn = std::sin(a),
                y = lerp(-.459f, -.09f, std::min(v / .78f, 1.f));
          float radius = .024f + .009f * g(y, -.34f, .073f) +
                         .019f * smooth((y + .24f) / .19f);
          const float fold =
              .0018f * g(y, -.22f + .012f * cs, .025f) * sn +
              .0012f * g(y, -.446f, .021f) * std::sin(2 * a + y * 210);
          Vec3 p{side * (.051f + (radius + fold) * cs), y,
                 (radius * 1.10f + fold) * sn - .002f};
          const float join = smooth((v - .78f) / .22f);
          const Vec3 top{side * .094f * std::max(0.f, cs),
                         .057f - .127f * std::max(0.f, -cs),
                         .060f * sn - .002f};
          p = p * (1 - join) + top * join;
          CharacterInfluence w =
              y < -.285f  ? wt(shin)
              : y < -.16f ? wt(shin, thigh, 1 - smooth((y + .285f) / .125f))
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
    auto &seam = seams[side > 0 ? 1 : 0];
    for (int i = columns / 4; i <= 3 * columns / 4; ++i)
      seam.push_back(base + rows * (columns + 1) + i);
    const float x = side * .051f;
    b.grid(b.near ? 28 : 10, 3, [&](float u, float v) {
      float a = 2 * pi * u;
      return P{{x + .030f * std::cos(a), lerp(-.5f, -.484f, v),
                .032f + .066f * std::sin(a)},
               {.061f, .065f, .069f},
               wt(foot),
               character_surface_uv(S::Rubber, u, v)};
    });
    b.grid(b.near ? 32 : 10, b.near ? 10 : 3, [&](float u, float v) {
      float a = 2 * pi * u, yy = lerp(-.484f, -.429f, v);
      return P{{x + lerp(.030f, .022f, smooth(v)) * std::cos(a), yy,
                lerp(.032f, .004f, smooth(v)) +
                    lerp(.065f, .026f, smooth(v)) * std::sin(a)},
               b.m.appearance.shoes,
               wt(foot),
               character_surface_uv(S::Leather, u, v)};
    });
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
    // Low-profile plain work shoes with a separate folded tongue and quiet
    // laces.
    for (int lace = 0; lace < (b.near ? 4 : 0); ++lace) {
      float z = .032f + lace * .007f, y = -.442f - lace * .0047f;
      b.curve(
          {{x - .011f, y, z}, {x, y + .0018f, z + .0018f}, {x + .011f, y, z}},
          .00065f, {.15f, .16f, .16f}, S::Shirt, foot, b.near ? 6 : 4);
    }
  }
  for (std::size_t i = 0; i < seams[0].size(); ++i)
    b.normal_groups.push_back({seams[0][i], seams[1][i]});
  b.grid(
      b.near ? 48 : 20, 3,
      [&](float u, float v) {
        float a = 2 * pi * u;
        return P{{.0945f * std::cos(a), lerp(.0565f, .071f, v),
                  .061f * std::sin(a) - .002f},
                 cloth * .94f,
                 wt(B::Pelvis),
                 character_surface_uv(S::Pants, u, v)};
      },
      true, false);
}
float nell_hairline(float a) {
  const float front = std::max(0.f, std::sin(a)),
              rear = std::max(0.f, -std::sin(a));
  return .437f + .027f * front - .042f * rear +
         .006f * std::pow(std::fabs(std::cos(a)), 10.f) +
         .0008f * std::sin(5 * a + .7f);
}
Vec3 nell_scalp(float a, float t, float lift = 0) {
  const float y =
      lerp(nell_hairline(a), .5f, std::sin(std::clamp(t, 0.f, 1.f) * pi * .5f));
  const float cz =
      lerp(nell_anatomy::head_center.z, nell_anatomy::crown_joint.z,
           smooth((y - .456f) / .037f));
  const float source_y = y - .007f * smooth((y - .448f) / .052f);
  float r =
      anatomy_radius(std::min(source_y, .49295f), a, cz) +
      (.0028f + lift) *
          std::sqrt(std::max(0.f, 1 - sq(std::max(0.f, (y - .478f) / .022f))));
  r += .0007f * std::sin(a * 3 + t * 2) * std::sin(pi * t);
  return {std::max(.00022f, r) * std::cos(a), y,
          cz + std::max(.00022f, r) * std::sin(a)};
}
void make_nell_hair(Sculpt &b) {
  const Vec3 hair = b.m.appearance.hair;
  const int columns = b.near ? 48 : 24, rows = b.near ? 14 : 7;
  const auto cap_begin = b.m.bind_mesh.indices.size();
  const auto start = unsigned(b.m.bind_mesh.vertices.size());
  b.grid(
      columns, rows,
      [&](float u, float v) {
        float a = 2 * pi * u;
        return P{nell_scalp(a, .018f + .982f * v, b.near ? 0 : .0028f),
                 hair * .82f, wt(B::Head),
                 character_surface_uv(S::HairCrop, u, v)};
      },
      true, false);
  const auto pole = b.vertex({{0, .5f, nell_anatomy::crown_joint.z},
                              hair * .82f,
                              wt(B::Head),
                              character_surface_uv(S::HairCrop, .5f, 1.f)});
  for (int i = 0; i < columns; ++i)
    b.tri(start + rows * (columns + 1) + i, pole,
          start + rows * (columns + 1) + i + 1);
  const auto cap_end = b.m.bind_mesh.indices.size();
  const auto fit_gathered_card = [&](Vec3 p, bool skin_fallback = false) {
    const Vec3 origin{0, p.y, nell_anatomy::crown_joint.z};
    const Vec3 direction = normalize(p - origin);
    float hit = -1;
    Vec3 normal{};
    const auto &mesh = b.m.bind_mesh;
    for (std::size_t t = cap_begin; t < cap_end; t += 3) {
      const Vec3 a = mesh.vertices[mesh.indices[t]].position * (1 / b.h),
                 c = mesh.vertices[mesh.indices[t + 1]].position * (1 / b.h),
                 d = mesh.vertices[mesh.indices[t + 2]].position * (1 / b.h);
      if (p.y < std::min({a.y, c.y, d.y}) - 1e-6f ||
          p.y > std::max({a.y, c.y, d.y}) + 1e-6f)
        continue;
      const Vec3 e1 = c - a, e2 = d - a, q = cross(direction, e2);
      const float det = dot(e1, q);
      if (std::fabs(det) < 1e-12f)
        continue;
      const Vec3 off = origin - a, r = cross(off, e1);
      const float u = dot(off, q) / det, v = dot(direction, r) / det,
                  dist = dot(e2, r) / det;
      if (u >= -1e-5f && v >= -1e-5f && u + v <= 1.00001f && dist > hit) {
        hit = dist;
        normal = normalize(cross(e1, e2));
        if (dot(normal, direction) < 0)
          normal = -normal;
      }
    }
    // Only the transparent root corners may be outside the cap's open edge.
    // Opaque gathered fibers sit just above actual cap polygons, not above an
    // analytic envelope that can float millimeters above coarse chords.
    // Boundary-card roots nest into the opaque shell; only their short tips
    // extend onto the scalp. A raised root row casts a dotted seam in path
    // light.
    if (hit > 0)
      return origin + direction * hit +
             normal * (skin_fallback ? -.00010f : (b.near ? .00010f : .00015f));
    if (skin_fallback) {
      const float angle = std::atan2(direction.z, direction.x);
      const float radius = anatomy_radius(p.y, angle, origin.z);
      if (radius > 0)
        return origin + direction * (radius + .00008f);
    }
    return p;
  };
  // A compact, low wrapped knot. The asymmetric coiled envelope constricts at
  // its root and follows the occiput, rather than perching above the crown.
  const Vec3 knot{.004f, .420f, -.042f};
  b.ellipsoid({.002f, .421f, -.025f}, {.018f, .017f, .020f}, hair * .83f,
              S::Hair, B::Head, b.near ? 24 : 10, b.near ? 10 : 4);
  b.grid(b.near ? 28 : 12, b.near ? 14 : 5, [&](float u, float v) {
    float a = 2 * pi * u, e = lerp(-pi * .495f, pi * .495f, v);
    float coil =
        1 + .060f * std::sin(3 * a + e * 4) + .028f * std::sin(7 * a - e * 3);
    return P{knot + Vec3{.029f * std::cos(a) * std::cos(e) * coil,
                         .022f * std::sin(e),
                         .020f * std::sin(a) * std::cos(e) * coil},
             hair * (.84f + .045f * std::sin(a * 3 + e * 2)), wt(B::Head),
             character_surface_uv(S::Hair, u, v)};
  });
  // The sheet's width is perpendicular to the local growth tangent. Pure
  // azimuth width folds at the apex where the growth changes vertical sign.
  for (int k = 0; k < (b.near ? 18 : 4); ++k) {
    const float side = k % 2 ? 1.f : -1.f,
                offset = float(k / 2) / (b.near ? 9.f : 2.f);
    const auto center = [&](float v) {
      const float start = pi * .5f + side * (.12f + offset * 1.12f);
      const float angle = start + side * (pi - .12f - offset * 1.12f) * v;
      const float y = lerp(nell_hairline(start) + .005f, .418f, v) +
                      (.030f - .015f * offset) * std::sin(pi * v);
      const float t = std::asin(std::clamp((y - nell_hairline(angle)) /
                                               (.5f - nell_hairline(angle)),
                                           0.f, 1.f)) /
                      (pi * .5f);
      return nell_scalp(angle, t,
                        (b.near ? .0008f : .0038f) + .0005f * std::sin(pi * v));
    };
    b.grid(
        2, b.near ? 10 : 6,
        [&](float u, float v) {
          Vec3 p = center(v),
               tangent = normalize(center(std::min(1.f, v + .001f)) -
                                   center(std::max(0.f, v - .001f)));
          Vec3 normal = normalize(Vec3{p.x, p.y - .437f, p.z - .023f});
          Vec3 width = normalize(cross(tangent, normal));
          p += width * ((.5f - u) * (.0090f - .0020f * v));
          p = fit_gathered_card(p);
          return P{p, hair * .83f, wt(B::Head),
                   character_surface_uv(S::HairShortStrands, u, v)};
        },
        false, false);
  }
  // Wrapped, masked locks cover the coiled knot with changing growth direction
  // and a broken outline. They are fitted outside its real opaque envelope.
  for (int lock = 0; lock < (b.near ? 8 : 2); ++lock) {
    const float root = 2 * pi * float(lock) / (b.near ? 8.f : 2.f);
    const auto center = [&](float t) {
      const float a = root + 1.4f * t, e = lerp(-1.10f, 1.10f, t),
                  coil = 1 + .060f * std::sin(3 * a + e * 4) +
                         .028f * std::sin(7 * a - e * 3);
      return knot + Vec3{(.029f * coil + .0008f) * std::cos(a) * std::cos(e),
                         .0228f * std::sin(e),
                         (.020f * coil + .0008f) * std::sin(a) * std::cos(e)};
    };
    b.grid(
        2, b.near ? 7 : 5,
        [&](float u, float v) {
          Vec3 p = center(v),
               tangent = normalize(center(std::min(1.f, v + .001f)) -
                                   center(std::max(0.f, v - .001f)));
          Vec3 normal =
              normalize(Vec3{(p.x - knot.x) / .029f, (p.y - knot.y) / .022f,
                             (p.z - knot.z) / .020f});
          p += normalize(cross(tangent, normal)) * ((.5f - u) * .0065f);
          return P{p, hair * .81f, wt(B::Head),
                   character_surface_uv(S::HairShortStrands, u, v)};
        },
        false, false);
  }
  std::vector<Vec3> tie;
  for (int i = 0; i <= 24; ++i) {
    float a = 2 * pi * i / 24;
    tie.push_back(
        {.002f + .016f * std::cos(a), .421f + .014f * std::sin(a), -.029f});
  }
  b.curve(tie, .0015f, {.023f, .024f, .025f}, S::Rubber, B::Head,
          b.near ? 6 : 4, .60f);
  const int cards = b.near ? 44 : 12;
  for (int k = 0; k < cards; ++k) {
    float a0 = 2 * pi * float(k) / cards;
    bool nape = std::sin(a0) < -.3f;
    float width = b.near ? .11f : .20f;
    b.grid(
        2, b.near ? 6 : 3,
        [&](float u, float v) {
          float a = a0 + (.5f - u) * width + .027f * std::sin(pi * v),
                t = lerp(.21f, nape ? -.040f : -.027f, v);
          Vec3 p = nell_scalp(a, std::max(0.f, t),
                              .0004f + .0005f * std::sin(pi * v));
          if (t < 0)
            p.y += .10f * t;
          p = fit_gathered_card(p, true);
          return P{p, hair * .84f, wt(B::Head),
                   character_surface_uv(S::HairShortStrands, u, v)};
        },
        false, false);
  }
}
CharacterInfluence nell_mix_weights(CharacterInfluence a, CharacterInfluence c,
                                    CharacterInfluence d, float u, float v);
CharacterInfluence nell_apron_weight(float x, float y) {
  if (y > .072f)
    return nell_jacket_detail::body_weight(y);
  const float follow = smooth((.071f - y) / .105f),
              right = smooth((x + .006f) / .012f);
  return {std::uint8_t(B::Pelvis), std::uint8_t(B::LeftThigh), 1 - follow,
          std::uint8_t(B::RightThigh), follow * right};
}
// The shell offset is along the actual parametric normal. Every corresponding
// inner/outer point retains identical bone weights, including the cut edge.
void nell_sheet(Sculpt &b, int nx, int ny,
                const std::function<P(float, float)> &sample, float thickness,
                const std::function<Vec3(Vec3)> &support_correction = {}) {
  std::vector<P> p;
  std::vector<Vec3> normal;
  const auto start = unsigned(b.m.bind_mesh.vertices.size());
  for (int j = 0; j <= ny; ++j)
    for (int i = 0; i <= nx; ++i) {
      P q = sample(float(i) / nx, float(j) / ny);
      p.push_back(q);
      normal.push_back({});
    }
  if (support_correction) {
    for (int pass = 0; pass < 4; ++pass) {
      std::vector<Vec3> delta(p.size());
      for (int j = 0; j < ny; ++j)
        for (int i = 0; i < nx; ++i) {
          unsigned a = j * (nx + 1) + i, c = a + nx + 1;
          for (auto t : {std::array<unsigned, 3>{a, c, c + 1},
                         std::array<unsigned, 3>{a, c + 1, a + 1}}) {
            for (auto weights :
                 {Vec3{.333333f, .333333f, .333334f}, Vec3{.5f, .5f, 0},
                  Vec3{0, .5f, .5f}, Vec3{.5f, 0, .5f}}) {
              const Vec3 q = p[t[0]].p * weights.x + p[t[1]].p * weights.y +
                             p[t[2]].p * weights.z;
              const Vec3 correction = support_correction(q);
              for (auto k : t)
                if (dot(correction, correction) > dot(delta[k], delta[k]))
                  delta[k] = correction;
            }
          }
        }
      for (std::size_t i = 0; i < p.size(); ++i)
        p[i].p += delta[i];
    }
  }
  for (const auto &q : p)
    b.vertex(q);
  // Thickness follows the actual emitted polygonal sheet. This also avoids
  // the absolute epsilon in normalize() swallowing tiny difference crosses.
  for (int j = 0; j < ny; ++j)
    for (int i = 0; i < nx; ++i) {
      unsigned a = j * (nx + 1) + i, c = a + nx + 1;
      for (auto t : {std::array<unsigned, 3>{a, c, c + 1},
                     std::array<unsigned, 3>{a, c + 1, a + 1}}) {
        Vec3 n = cross(p[t[1]].p - p[t[0]].p, p[t[2]].p - p[t[0]].p);
        for (auto k : t)
          normal[k] += n;
      }
    }
  for (auto &n : normal) {
    const float length2 = dot(n, n);
    if (length2 < 1e-20f)
      throw std::runtime_error(
          "Nell cloth shell has a degenerate geometric normal");
    n = n * (1 / std::sqrt(length2));
  }
  const unsigned count = p.size();
  for (unsigned i = 0; i < count; ++i) {
    P q = p[i];
    q.p -= normal[i] * thickness;
    q.c = q.c * .97f;
    b.vertex(q);
  }
  auto id = [&](int i, int j) { return start + j * (nx + 1) + i; };
  for (int j = 0; j < ny; ++j)
    for (int i = 0; i < nx; ++i) {
      auto a = id(i, j), c = id(i, j + 1);
      b.tri(a, c, c + 1);
      b.tri(a, c + 1, a + 1);
      b.tri(a + count, c + 1 + count, c + count);
      b.tri(a + count, a + 1 + count, c + 1 + count);
    }
  std::vector<unsigned> edge;
  for (int i = 0; i < nx; ++i)
    edge.push_back(id(i, 0));
  for (int j = 0; j < ny; ++j)
    edge.push_back(id(nx, j));
  for (int i = nx; i > 0; --i)
    edge.push_back(id(i, ny));
  for (int j = ny; j > 0; --j)
    edge.push_back(id(0, j));
  for (std::size_t i = 0; i < edge.size(); ++i) {
    const unsigned a = edge[i] - start, c = edge[(i + 1) % edge.size()] - start;
    P oa = p[a], oc = p[c], ia = oa, ic = oc;
    ia.p -= normal[a] * thickness;
    ic.p -= normal[c] * thickness;
    const auto va = b.vertex(oa), vi = b.vertex(ia), vc = b.vertex(ic),
               vo = b.vertex(oc);
    b.tri(va, vc, vi);
    b.tri(va, vo, vc);
  }
}
Vec3 nell_apron_point(float u, float v) {
  const float y = lerp(-.205f, .294f, v), t = 2 * u - 1;
  if (y > .071f) {
    float blend = smooth((y - .071f) / .223f),
          width = lerp(.090f, .044f, blend), x = -width * t;
    float radius = nell_jacket_detail::section(
              y, {.060f, .056f, .051f, .064f, .063f, .052f}),
          rx = nell_jacket_detail::section(
              y, {.094f, .088f, .080f, .093f, .105f, .105f});
    float z = radius * std::sqrt(std::max(.02f, 1 - sq(x / rx))) + .013f;
    z += .0025f * g(y, .105f, .040f) * std::cos(t * pi) +
         .0015f * std::sin(3 * pi * u) * g(y, .225f, .09f);
    return {x, y, z};
  }
  float down = smooth((.071f - y) / .276f), a = -t * lerp(1.19f, 1.35f, down),
        rx = lerp(.097f, .123f, down), rz = lerp(.073f, .078f, down);
  float fold = (.0050f * g(t, -.46f, .16f) - .0036f * g(t, -.24f, .14f) +
                .0045f * g(t, .48f, .18f) - .0016f * g(t, .23f, .12f)) *
               down;
  const float x = (rx + fold) * std::sin(a);
  const float hip_fullness = .039f * smooth((.080f - y) / .080f) *
                             smooth((std::fabs(x) - .020f) / .065f);
  return {x, y + .009f * sq(t) * (1 - smooth(v / .12f)),
          (rz + fold) * std::cos(a) + .002f + hip_fullness +
              .018f * smooth((.080f - y) / .090f) * g(x, 0, .040f)};
}
CharacterInfluence nell_mix_weights(CharacterInfluence a, CharacterInfluence c,
                                    CharacterInfluence d, float u, float v) {
  std::array<float, character_bone_count> weights{};
  auto add = [&](CharacterInfluence w, float f) {
    weights[w.first] += f * w.first_weight;
    weights[w.second] += f * (1 - w.first_weight - w.third_weight);
    weights[w.third] += f * w.third_weight;
  };
  add(a, 1 - u - v);
  add(c, u);
  add(d, v);
  std::array<unsigned, character_bone_count> order{};
  for (unsigned i = 0; i < order.size(); ++i)
    order[i] = i;
  std::stable_sort(order.begin(), order.end(), [&](unsigned x, unsigned y) {
    return weights[x] > weights[y];
  });
  float sum = weights[order[0]] + weights[order[1]] + weights[order[2]];
  if (sum <= 0)
    return wt(B::Chest);
  float first = weights[order[0]] / sum, third = weights[order[2]] / sum;
  if (first + third > 1)
    third = 1 - first;
  return {std::uint8_t(order[0]), std::uint8_t(order[1]), first,
          std::uint8_t(order[2]), third};
}
P nell_fit_shirt(const Sculpt &b, Vec3 p, Vec3 direction, bool required = true,
                 std::size_t *support_triangle = nullptr) {
  const auto &mesh = b.m.bind_mesh;
  Vec3 origin = p - direction * 2.f;
  float hit = -1;
  P result{p, {}, nell_jacket_detail::body_weight(p.y), {}};
  const auto low = character_surface_uv(S::Shirt, 0, 0),
             high = character_surface_uv(S::Shirt, 1, 1);
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
    const auto ia = mesh.indices[t], ic = mesh.indices[t + 1],
               id = mesh.indices[t + 2];
    const auto uv = mesh.vertices[ia].uv;
    if (uv.x < low.x || uv.x > high.x || uv.y < low.y || uv.y > high.y)
      continue;
    const Vec3 a = mesh.vertices[ia].position * (1 / b.h),
               c = mesh.vertices[ic].position * (1 / b.h),
               d = mesh.vertices[id].position * (1 / b.h);
    // Every current support ray is stationary in X, and the front/back
    // rays are stationary in Y too. Reject only impossible axis intervals;
    // the padding exceeds the accepted barycentric edge tolerance.
    constexpr float padding = .00001f;
    if (std::fabs(direction.x) < 1e-7f &&
        (p.x < std::min({a.x, c.x, d.x}) - padding ||
         p.x > std::max({a.x, c.x, d.x}) + padding))
      continue;
    if (std::fabs(direction.y) < 1e-7f &&
        (p.y < std::min({a.y, c.y, d.y}) - padding ||
         p.y > std::max({a.y, c.y, d.y}) + padding))
      continue;
    const Vec3 e1 = c - a, e2 = d - a, q = cross(direction, e2);
    float det = dot(e1, q);
    if (std::fabs(det) < 1e-12f)
      continue;
    Vec3 off = origin - a, r = cross(off, e1);
    float u = dot(off, q) / det, v = dot(direction, r) / det,
          distance = dot(e2, r) / det;
    if (u >= -1e-5f && v >= -1e-5f && u + v <= 1.00001f && distance > hit) {
      hit = distance;
      if (support_triangle)
        *support_triangle = t;
      Vec3 n = normalize(cross(e1, e2));
      if (dot(n, direction) < 0)
        n = -n;
      result.p = origin + direction * distance + n * .0028f;
      result.c = n;
      result.w = nell_mix_weights(b.m.influences[ia], b.m.influences[ic],
                                  b.m.influences[id], u, v);
    }
  }
  if (hit < 0 && required)
    throw std::runtime_error("Nell strap has no physical shirt support at " +
                             std::to_string(p.x) + "," + std::to_string(p.y) +
                             "," + std::to_string(p.z));
  return result;
}

void make_nell_apron(Sculpt &b) {
  const Vec3 cloth = b.m.appearance.accent;
  auto sheet = [&](float u, float v) {
    Vec3 p = nell_apron_point(u, v);
    const auto support = nell_fit_shirt(b, p, {0, 0, 1}, false);
    const auto weights = p.y > .071f ? support.w : nell_apron_weight(p.x, p.y);
    // Modest extra Far bib fullness compensates its long skinned chords.
    // It fades to zero at the waist and strap attachments and follows the
    // actual shirt normal; the closed shell still has independent thickness.
    if (!b.near && p.y > .071f && dot(support.c, support.c) > .5f)
      p += support.c * (.006f * std::sin(pi * smooth((p.y - .071f) / .223f)));
    return P{p, cloth, weights,
             character_surface_uv(S::Jacket, .02f + .70f * u, .02f + .86f * v)};
  };
  nell_sheet(b, b.near ? 20 : 10, b.near ? 16 : 8, sheet, .0013f);
  // One spacious lower pocket with a quieter divided utility channel. Its
  // silhouette follows the curved skirt; the top has an actual returned lip.
  auto pocket = [&](float u, float v) {
    float y = lerp(-.145f, -.052f, v), a = lerp(.20f, .80f, u),
          av = (y + .205f) / .499f;
    Vec3 p = nell_apron_point(a, av);
    Vec3 du = nell_apron_point(a + .001f, av) - nell_apron_point(a - .001f, av),
         dv = nell_apron_point(a, av + .001f) - nell_apron_point(a, av - .001f);
    Vec3 normal = normalize(cross(normalize(dv), normalize(du)));
    const auto weights = nell_apron_weight(p.x, p.y);
    p += normal * (.0020f + .0070f * std::sin(pi * u) * smooth(v / .6f));
    return P{p, cloth * 1.015f, weights,
             character_surface_uv(S::Jacket, .02f + .35f * u, .02f + .17f * v)};
  };
  nell_sheet(b, b.near ? 18 : 6, b.near ? 10 : 4, pocket, .0010f);
  // The open mouth folds over a narrow cloth strip; the stitch piping is an
  // actual raised edge that remains subordinate to the pocket volume.
  for (int edge : {0, 3}) {
    const int steps = b.near ? 18 : 6;
    std::vector<Vec3> line;
    std::vector<float> widths;
    for (int i = 0; i <= steps; ++i) {
      float t = float(i) / steps,
            u = edge == 0   ? t
                : edge == 1 ? 0
                : edge == 2 ? 1
                            : .34f,
            v = edge == 0 ? 1 : t;
      P p = pocket(u, v);
      line.push_back(p.p);
      widths.push_back(edge == 0 ? .00135f : .00055f);
    }
    b.tube(
        line, widths, .45f, cloth * .94f, S::Jacket,
        [&](float t) {
          float u = edge == 0   ? t
                    : edge == 1 ? 0
                    : edge == 2 ? 1
                                : .34f,
                v = edge == 0 ? 1 : t;
          P p = pocket(u, v);
          return p.w;
        },
        b.near ? 6 : 4);
  }

  // Cross-back shoulder tapes wrap continuously over actual LOD triangles.
  // Their width follows the surface tangent; one crosses above the other.
  for (float side : {-1.f, 1.f}) {
    const auto center = [&](float v) {
      Vec3 p, d;
      if (v <= .60f) {
        float a = pi * v / .60f;
        p = {side * (.055f + .012f * std::sin(a)), .294f + .052f * std::sin(a),
             .065f * std::cos(a)};
        d = {0, std::sin(a), std::cos(a)};
      } else {
        float t = (v - .60f) / .40f;
        p = {side * lerp(.055f, -.065f, smooth(t)), lerp(.294f, .073f, t),
             -.060f};
        d = {0, 0, -1};
      }
      P fit = nell_fit_shirt(b, p, d);
      if (side > 0 && v > .60f)
        fit.p += fit.c * (.0018f * g(fit.p.y, .205f, .055f));
      if (v < .055f) {
        Vec3 root = nell_apron_point(side > 0 ? 0 : 1, 1);
        fit.p = root * (1 - smooth(v / .055f)) + fit.p * smooth(v / .055f);
      }
      return fit;
    };
    // Resolve every support-face transition along the tape center and both
    // edges. Intervals follow actual shirt triangle boundaries, rather than
    // a long chord above an analytic shoulder envelope.
    const auto supporting_face = [&](float u, float v) {
      P fit = center(v);
      const Vec3 tangent = normalize(center(std::min(1.f, v + .001f)).p -
                                     center(std::max(0.f, v - .001f)).p);
      fit.p += normalize(cross(tangent, fit.c)) * ((.5f - u) * .010f);
      const float a = pi * std::min(v / .60f, 1.f);
      std::size_t face = 0;
      nell_fit_shirt(b, fit.p, {0, std::sin(a), std::cos(a)}, true, &face);
      return face;
    };
    std::vector<float> knots{0, .055f, .30f, .60f, 1};
    const int regular_rows = b.near ? 36 : 14;
    for (int i = 1; i < regular_rows; ++i)
      knots.push_back(float(i) / regular_rows);
    const auto face_normal = [&](std::size_t face) {
      const auto &mesh = b.m.bind_mesh;
      const Vec3 a = mesh.vertices[mesh.indices[face]].position,
                 c = mesh.vertices[mesh.indices[face + 1]].position,
                 d = mesh.vertices[mesh.indices[face + 2]].position;
      return normalize(cross(normalize(c - a), normalize(d - a)));
    };
    for (float u : {0.f, .5f, 1.f}) {
      float previous = .055f;
      auto face = supporting_face(u, previous);
      for (int i = 1; i <= 160; ++i) {
        float v = lerp(.055f, 1.f, float(i) / 160);
        const auto next = supporting_face(u, v);
        if (next != face) {
          float low = previous, high = v;
          for (int step = 0; step < 8; ++step) {
            const float mid = (low + high) * .5f;
            if (supporting_face(u, mid) == face)
              low = mid;
            else
              high = mid;
          }
          if (dot(face_normal(face), face_normal(next)) < .93f)
            knots.push_back((low + high) * .5f);
        }
        previous = v;
        face = next;
      }
    }
    std::sort(knots.begin(), knots.end());
    std::vector<float> rows;
    for (float v : knots)
      if (rows.empty() || v - rows.back() > .0015f)
        rows.push_back(v);
    rows.back() = 1;
    nell_sheet(
        b, 2, int(rows.size()) - 1,
        [&](float u, float v) {
          v = rows[std::size_t(std::lround(v * (rows.size() - 1)))];
          P fit = center(v);
          Vec3 tangent = normalize(center(std::min(1.f, v + .001f)).p -
                                   center(std::max(0.f, v - .001f)).p);
          Vec3 width = normalize(cross(tangent, fit.c));
          fit.p += width * ((.5f - u) * .010f);
          // Fit the actual width corners, not only a centerline. The coarse
          // Far shirt has chordal folds that can swallow a straight tape.
          if (v >= .055f) {
            const float a = pi * std::min(v / .60f, 1.f);
            const Vec3 outward{0, std::sin(a), std::cos(a)};
            fit = nell_fit_shirt(b, fit.p, outward);
          }
          if (side > 0 && v > .60f)
            fit.p += fit.c * (.0018f * g(fit.p.y, .205f, .055f));
          return P{fit.p, cloth * .96f, fit.w,
                   character_surface_uv(S::Jacket, .02f + .018f * u,
                                        .02f + .80f * v)};
        },
        .0011f,
        [&](Vec3 point) {
          Vec3 direction{0, 0, point.z >= 0 ? 1.f : -1.f};
          if (point.y > .294f)
            direction =
                normalize(Vec3{0, (point.y - .294f) / .052f, point.z / .065f});
          const P support = nell_fit_shirt(b, point, direction, false);
          if (dot(support.c, support.c) < .5f)
            return Vec3{};
          const float gap = dot(support.p - point, direction);
          return direction * std::max(0.f, gap + .00020f);
        });
  }
  b.ellipsoid({-.055f, .066f, -.059f}, {.0055f, .0040f, .0032f}, cloth * .92f,
              S::Jacket, B::Pelvis, 8, 3);
  // Narrow waist band wraps around the uncovered back and meets on the left.
  const int n = b.near ? 52 : 20;
  nell_sheet(
      b, n, 2,
      [&](float u, float v) {
        float a = lerp(.17f * pi, 1.83f * pi, u), y = .070f + (v - .5f) * .010f;
        Vec3 p{.093f * std::cos(a), y, .067f * std::sin(a) - .001f};
        return P{
            p, cloth * .94f, wt(B::Pelvis),
            character_surface_uv(S::Jacket, .02f + .8f * u, .02f + .018f * v)};
      },
      .0012f);
  for (float side : {-1.f, 1.f}) {
    const int steps = b.near ? 18 : 5;
    nell_sheet(
        b, 2, steps,
        [&](float u, float v) {
          float a = pi * 2 * v;
          Vec3 p{-.055f + side * .020f * std::sin(pi * v),
                 .066f + .007f * std::sin(a),
                 -.059f - side * .0010f - .005f * std::sin(pi * v)};
          Vec3 tangent{side * .020f * pi * std::cos(pi * v),
                       .014f * pi * std::cos(a),
                       -.005f * pi * std::cos(pi * v)};
          Vec3 width = normalize(Vec3{-tangent.y, tangent.x, 0});
          p += width * ((u - .5f) * .0065f);
          return P{p, cloth, wt(B::Pelvis),
                   character_surface_uv(S::Jacket, .02f + .014f * u,
                                        .02f + .09f * v)};
        },
        .0010f);
    nell_sheet(
        b, 2, steps,
        [&](float u, float v) {
          Vec3 p{-.055f + side * (.008f + .009f * std::sin(v * pi)),
                 lerp(.061f, -.052f, v), -.064f - .004f * std::sin(v * 3 * pi)};
          p.x += (.5f - u) * .009f;
          return P{p, cloth, wt(B::Pelvis),
                   character_surface_uv(S::Jacket, .02f + .016f * u,
                                        .02f + .20f * v)};
        },
        .0010f);
  }
}
CharacterRig nell_rig(float height) {
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
  child(B::Neck, B::Chest, nell_anatomy::neck_center);
  child(B::Head, B::Neck, nell_anatomy::head_center);
  for (float sign : {-1.f, 1.f}) {
    bool q = sign > 0;
    B upper = q ? B::RightUpperArm : B::LeftUpperArm,
      fore = q ? B::RightForearm : B::LeftForearm,
      hand = q ? B::RightHand : B::LeftHand,
      thigh = q ? B::RightThigh : B::LeftThigh,
      shin = q ? B::RightShin : B::LeftShin,
      foot = q ? B::RightFoot : B::LeftFoot;
    child(upper, B::Chest, {sign * .107f, .322f, 0});
    child(fore, upper, {sign * .135f, .12f, 0});
    child(hand, fore, {sign * .151f, -.044f, 0});
    child(thigh, B::Pelvis, {sign * .051f, 0, 0});
    child(shin, thigh, {sign * .051f, -.225f, 0});
    child(foot, shin, {sign * .051f, -.46f, 0});
  }
  return r;
}
} // namespace
CharacterProfile make_nell_character_profile(float height, CharacterRole role,
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
  m.rig = nell_rig(height);
  m.dual_quaternion_skinning = true;
  m.bind_mesh.vertices.reserve(lod == CharacterLod::Near ? 36000 : 16000);
  m.bind_mesh.indices.reserve(lod == CharacterLod::Near ? 180000 : 90000);
  m.influences.reserve(m.bind_mesh.vertices.capacity());
  Sculpt b{m, height, lod == CharacterLod::Near, {}, {}};
  make_nell_shirt(b, anatomy_radius, nell_anatomy::neck_center);
  make_hands_and_forearms(b);
  make_trousers_and_shoes(b);
  make_head(b);
  make_nell_hair(b);
  make_nell_apron(b);
  b.finish();
  // The tape crosses a sharp shoulder fold. Some very small folded faces
  // need independent rendering corners instead of an averaged normal from
  // the opposite side of that crease. Preserve geometry, UVs and influences;
  // proper-interior collision and closed-shell tests still use the same faces.
  const auto jacket_low = character_surface_uv(S::Jacket, 0, 0);
  const auto jacket_high = character_surface_uv(S::Jacket, 1, 1);
  for (std::size_t t = 0; t < m.bind_mesh.indices.size(); t += 3) {
    auto &mesh = m.bind_mesh;
    const auto a = mesh.vertices[mesh.indices[t]],
               c = mesh.vertices[mesh.indices[t + 1]],
               d = mesh.vertices[mesh.indices[t + 2]];
    if (a.uv.x < jacket_low.x || a.uv.x > jacket_high.x ||
        a.uv.y < jacket_low.y || a.uv.y > jacket_high.y ||
        (a.position.y + c.position.y + d.position.y) / (3 * height) < .30f)
      continue;
    const Vec3 face = cross(c.position - a.position, d.position - a.position);
    if (dot(face, a.normal + c.normal + d.normal) >= 0)
      continue;
    const Vec3 normal = normalize(cross(normalize(c.position - a.position),
                                        normalize(d.position - a.position)));
    for (int k = 0; k < 3; ++k) {
      const auto old = mesh.indices[t + k];
      auto vertex = mesh.vertices[old];
      vertex.normal = normal;
      mesh.indices[t + k] = std::uint32_t(mesh.vertices.size());
      mesh.vertices.push_back(vertex);
      m.influences.push_back(m.influences[old]);
    }
  }
  p.material = make_character_surface_material();
  p.material.alpha_cutoff = .4f;
  p.lod_distance = 12;
  p.individualized = true;
  return p;
}
} // namespace fury
