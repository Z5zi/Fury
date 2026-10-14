#include "character_sculpt.hpp"
#include "character_vale_cap.hpp"
#include "character_vale_shirt.hpp"
#include "character_vale_vest.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/vale_anatomy.hpp"
#include "fury/generated/vale_hands.hpp"
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
  for (std::size_t i = 0; i < vale_anatomy::indices.size(); i += 3) {
    const Vec3 a = vale_anatomy::positions[vale_anatomy::indices[i]];
    const Vec3 b = vale_anatomy::positions[vale_anatomy::indices[i + 1]];
    const Vec3 c = vale_anatomy::positions[vale_anatomy::indices[i + 2]];
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
  for (std::size_t i = 0; i < vale_anatomy::indices.size(); i += 3) {
    const Vec3 a = vale_anatomy::positions[vale_anatomy::indices[i]];
    const Vec3 b = vale_anatomy::positions[vale_anatomy::indices[i + 1]];
    const Vec3 c = vale_anatomy::positions[vale_anatomy::indices[i + 2]];
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
CharacterInfluence vale_neck_weight(float y) {
  // Keep the mouth and jaw planes on the head. The old high blend ended
  // inside the mouth and visibly sheared the face during strong attention.
  // All attachment deformation is confined to the actual lower neck.
  return y < .350f ? wt(B::Neck, B::Chest, smooth((y - .333f) / .017f))
                   : wt(B::Head, B::Neck, smooth((y - .350f) / .023f));
}
void make_head(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> corners;
  std::vector<std::vector<std::uint32_t>> groups(
      vale_anatomy::positions.size());
  for (std::size_t triangle = 0; triangle < vale_anatomy::indices.size();
       triangle += 3) {
    std::uint32_t out[3];
    for (int corner = 0; corner < 3; ++corner) {
      const auto index = vale_anatomy::indices[triangle + corner];
      const auto uv_index = vale_anatomy::uv_indices[triangle + corner];
      const auto key = std::make_pair(index, uv_index);
      auto found = corners.find(key);
      if (found == corners.end()) {
        const Vec3 position = vale_anatomy::positions[index];
        const Vec2 uv = vale_anatomy::uv_coordinates[uv_index];
        const float lip = g(position.x, 0, .015f) *
                          g(position.y, .38201f, .0035f) *
                          smooth((position.z - .079f) / .010f);
        // Small anatomical pigment zones retain the stable base complexion.
        const float cheeks = g(std::fabs(position.x), .030f, .015f) *
                             g(position.y, .403f, .015f) *
                             smooth((position.z - .067f) / .02f);
        const float nose = g(position.x, 0, .009f) *
                           g(position.y, .405f, .012f) *
                           smooth((position.z - .081f) / .013f);
        const float front = smooth((position.z - .054f) / .025f);
        const float vascular =
            .5f + .25f * std::sin(position.x * 360 + position.y * 97) +
            .25f * std::sin(position.y * 285 - position.z * 190);
        const float lid = g(std::fabs(position.x), .0189f, .008f) *
                          g(position.y, .424f, .006f) * front;
        Vec3 color = skin * (1 - .025f * vascular * front);
        color.x *= 1 - .015f * lid;
        color.y *= 1 - .055f * lid;
        color.z *= 1 - .037f * lid;
        color.y *= 1 - .052f * cheeks - .043f * nose;
        color.z *= 1 - .077f * cheeks - .054f * nose;
        color.y *= 1 - .22f * lip;
        color.z *= 1 - .19f * lip;
        const auto vertex =
            b.vertex({position, color, vale_neck_weight(position.y),
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
  auto boundary = vale_anatomy::attachment_boundary_indices;
  float area = 0;
  for (std::size_t i = 0; i < boundary.size(); ++i) {
    const Vec3 a = vale_anatomy::positions[boundary[i]];
    const Vec3 c = vale_anatomy::positions[boundary[(i + 1) % boundary.size()]];
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
        Vec3 point = vale_anatomy::positions[boundary[i]];
        point.y -= .015f * (1 - v);
        return P{point, skin, vale_neck_weight(point.y),
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
  for (std::size_t ei = 0; ei < vale_anatomy::eye_centers.size(); ++ei) {
    const Vec3 eye = vale_anatomy::eye_centers[ei] +
                     Vec3{ei == 0 ? -.00080f : .00080f, -.00055f, -.00170f};
    const Vec3 radius = vale_anatomy::eye_helper_half_extents[ei];
    b.ellipsoid(eye, radius, {.79f, .80f, .77f}, S::Eyes, B::Head,
                b.near ? 24 : 12, b.near ? 12 : 6);
    if (b.near) {
      std::vector<Vec3> upper_lid, lower_lid;
      const auto &rim = ei == 0 ? vale_anatomy::left_eye_opening
                                : vale_anatomy::right_eye_opening;
      for (Vec3 p : rim) {
        if (std::fabs(std::fabs(p.x) - .01885f) > .0052f)
          continue;
        p.z += .00010f;
        (p.y >= .4241f ? upper_lid : lower_lid).push_back(p);
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
    const int iris_columns = b.near ? 24 : 10, iris_rows = b.near ? 5 : 3;
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
    const int count = b.near ? 20 : 6;
    for (int i = 0; i <= count; ++i) {
      float t = float(i) / count, x = eye.x + lerp(-.0118f, .0118f, t),
            outward = eye.x > 0 ? t : 1 - t,
            y = eye.y + .0120f + .0025f * std::sin(pi * outward) -
                .0017f * outward;
      float z = anatomy_front(x, y);
      brow.push_back({x, y, z + .0004f});
      brow_radius.push_back(
          .00012f + .00092f * std::pow(std::max(0.f, std::sin(pi * t)), .6f));
    }
    b.tube(
        brow, brow_radius, .35f, b.m.appearance.hair * .74f, S::Hair,
        [](float) { return wt(B::Head); }, b.near ? 6 : 4);
  }
}

Vec3 vale_hand_offset(Vec3 p, float side) {
  p.x *= side;
  const float a = side * 1.05f, c = std::cos(a), sn = std::sin(a);
  return {c * p.x + sn * p.z, p.y, -sn * p.x + c * p.z};
}
void make_hands_and_forearms(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  for (float side : {-1.f, 1.f}) {
    const B fore = side > 0 ? B::RightForearm : B::LeftForearm;
    const B upper = side > 0 ? B::RightUpperArm : B::LeftUpperArm;
    const B hand = side > 0 ? B::RightHand : B::LeftHand;
    const Vec3 wrist{side * .181f, -.044f, .018f};
    std::map<std::pair<unsigned, unsigned>, unsigned> corners;
    std::vector<std::vector<unsigned>> groups(vale_hands::positions.size());
    const auto emit = [&](auto const &indices, auto const &uv_indices) {
      for (std::size_t t = 0; t < indices.size(); t += 3) {
        unsigned out[3];
        for (int k = 0; k < 3; ++k) {
          auto index = indices[t + k], uv = uv_indices[t + k];
          const bool hard =
              b.near
                  ? std::find(vale_hands::hard_normal_triangle_indices.begin(),
                              vale_hands::hard_normal_triangle_indices.end(),
                              t / 3) !=
                        vale_hands::hard_normal_triangle_indices.end()
                  : std::find(
                        vale_hands::far_hard_normal_triangle_indices.begin(),
                        vale_hands::far_hard_normal_triangle_indices.end(),
                        t / 3) !=
                        vale_hands::far_hard_normal_triangle_indices.end();
          auto key = std::make_pair(
              index,
              uv + (hard ? unsigned((t + 1) * vale_hands::uv_coordinates.size())
                         : 0));
          auto found = corners.find(key);
          if (found == corners.end()) {
            Vec3 p = vale_hand_offset(vale_hands::positions[index], side);
            p += wrist;
            const Vec2 tex = vale_hands::uv_coordinates[uv];
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
      emit(vale_hands::near_indices, vale_hands::near_uv_indices);
    else
      emit(vale_hands::far_indices, vale_hands::far_uv_indices);
    // Exactly reuse every anatomical wrist point at the terminal forearm ring.
    // The opposite terminal ring tucks inside the open rolled cuff, not a cap.
    auto boundary = vale_hands::wrist_boundary_indices;
    float area = 0;
    for (std::size_t i = 0; i < boundary.size(); ++i) {
      auto a = vale_hands::positions[boundary[i]],
           c = vale_hands::positions[boundary[(i + 1) % boundary.size()]];
      area += a.x * c.z - c.x * a.z;
    }
    if (area * side < 0)
      std::reverse(boundary.begin(), boundary.end());
    Vec3 center{};
    for (auto i : boundary)
      center += vale_hands::positions[i];
    center = center * (1.f / boundary.size());
    const unsigned start = b.m.bind_mesh.vertices.size();
    const int columns = boundary.size(), rows = b.near ? 6 : 2;
    b.grid(
        columns, rows,
        [&](float u, float v) {
          const auto index =
              boundary[std::size_t(std::lround(u * columns)) % boundary.size()];
          Vec3 source = vale_hand_offset(vale_hands::positions[index], side);
          source += wrist;
          const Vec3 local =
              vale_hand_offset(vale_hands::positions[index] - center, side);
          const float a = std::atan2(local.z, local.x);
          const float y = lerp(source.y, -.014f, v),
                      radius = .0175f + .0130f * g(y, .078f, .071f);
          const float x =
              side * lerp(.181f, .179f, smooth((y + .044f) / .030f));
          Vec3 target{x + radius * std::cos(a), y,
                      .018f + radius * .83f * std::sin(a)};
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
void make_vale_trousers(Sculpt &b) {
  // Original pair-of-pants topology. Each half of one waist loop joins one leg
  // through a shared curved crotch seam. There is no intersecting seat shell
  // or exposed horizontal thigh top under the tucked shirt.
  using vale_shirt_detail::Pattern;
  Pattern pattern{b, {}, {}, {}, {}};
  const Vec3 cloth{.095f, .13f, .175f};
  const int half = b.near ? 20 : 8, rise = b.near ? 10 : 4;
  const int count = half + rise;
  const int hip_rows = b.near ? 7 : 3, leg_rows = b.near ? 25 : 10;
  const float rx = .096f, rz = .063f, top = .051f;
  std::vector<std::uint32_t> waist(2 * half), crotch(rise + 1);
  for (int i = 0; i < 2 * half; ++i) {
    const float a = pi * .5f + pi * float(i) / half;
    waist[i] = pattern.node({rx * std::cos(a), top, rz * std::sin(a)}, cloth,
                            wt(B::Pelvis));
  }
  crotch.front() = waist[half];
  crotch.back() = waist[0];
  for (int i = 1; i < rise; ++i) {
    const float t = float(i) / rise;
    const float z = lerp(-rz, rz, t);
    const float y =
        top - .121f * std::pow(std::max(0.f, std::sin(pi * t)), .70f);
    const float support =
        .65f * std::pow(std::max(0.f, std::sin(pi * t)), 1.35f);
    crotch[i] =
        pattern.node({0, y, z}, cloth,
                     {std::uint8_t(B::Pelvis), std::uint8_t(B::LeftThigh),
                      1 - support, std::uint8_t(B::RightThigh), support * .5f});
  }
  const auto tex = [&](float u, float v) {
    return character_surface_uv(S::Pants, .02f + u, .02f + .94f * v);
  };
  for (float sign : {-1.f, 1.f}) {
    const B thigh = sign < 0 ? B::LeftThigh : B::RightThigh;
    const B shin = sign < 0 ? B::LeftShin : B::RightShin;
    const B foot = sign < 0 ? B::LeftFoot : B::RightFoot;
    std::vector<std::uint32_t> root;
    if (sign < 0) {
      for (int i = 0; i < half; ++i)
        root.push_back(waist[i]);
      for (int i = 0; i < rise; ++i)
        root.push_back(crotch[i]);
    } else {
      for (int i = half; i < 2 * half; ++i)
        root.push_back(waist[i]);
      for (int i = rise; i > 0; --i)
        root.push_back(crotch[i]);
    }
    std::vector<float> phase(count);
    for (int i = 0; i < count; ++i) {
      const Vec3 p = pattern.points[root[i]].p;
      phase[i] = std::atan2(p.z, p.x - sign * .059f);
    }
    auto previous = root;
    for (int j = 1; j <= hip_rows + leg_rows; ++j) {
      std::vector<std::uint32_t> next(count);
      for (int i = 0; i < count; ++i) {
        const float a = phase[i], cs = std::cos(a), sn = std::sin(a);
        Vec3 p{};
        CharacterInfluence weights{};
        if (j <= hip_rows) {
          const float t = float(j) / hip_rows;
          const Vec3 first = pattern.points[root[i]].p;
          const Vec3 end{sign * .059f + .0445f * cs, -.105f,
                         .049f * sn - .002f};
          p = first * (1 - t) + end * t;
          // Broad hip/seat ease, concentrated below the waist rather than a
          // separate capsule. The shared rise stays attached to the pelvis.
          p.x += sign * .0040f * std::sin(pi * t) * std::max(0.f, sign * cs);
          p.z -= .0035f * std::sin(pi * t) * std::max(0.f, -sn);
          const auto start = pattern.points[root[i]].w;
          const float amount = smooth(t / .82f);
          const float support = start.third == std::uint8_t(B::RightThigh)
                                    ? 2 * start.third_weight
                                    : 0.f;
          const float pelvis = (1 - support) * (1 - amount);
          const float right =
              support * .5f * (1 - amount) + (sign > 0 ? amount : 0.f);
          weights = {std::uint8_t(B::Pelvis), std::uint8_t(B::LeftThigh),
                     pelvis, std::uint8_t(B::RightThigh),
                     std::min(right, 1 - pelvis)};
        } else {
          const float near_y[]{-.105f, -.121f, -.139f, -.157f, -.173f, -.185f,
                               -.196f, -.205f, -.215f, -.225f, -.235f, -.245f,
                               -.255f, -.267f, -.280f, -.295f, -.313f, -.331f,
                               -.350f, -.367f, -.384f, -.403f, -.422f, -.436f,
                               -.441f, -.446f};
          const float far_y[]{-.105f, -.150f, -.190f, -.225f, -.260f, -.300f,
                              -.350f, -.395f, -.425f, -.440f, -.446f};
          const int row = j - hip_rows;
          const float y = b.near ? near_y[row] : far_y[row];
          float rad = .034f + .009f * smooth((y + .340f) / .240f);
          rad -= .003f * smooth((-.405f - y) / .054f);
          rad += .0012f * std::pow(std::max(0.f, sn), 14.f);
          rad += .0012f * g(y, -.224f, .027f) * std::sin(y * 220 + a) +
                 .0009f * g(y, -.443f, .023f) * std::sin(y * 310 + a * 2);
          // Match the first leg ring exactly to the last rise ring, then
          // transition smoothly to the independently shaped calf/knee.
          const float blend = smooth((-.105f - y) / .065f);
          const float rx = lerp(.0445f, rad, blend),
                      rz = lerp(.047f, rad * 1.07f, blend);
          p = {sign * .059f + rx * cs, y,
               rz * sn - .002f +
                   .010f * g(y, -.225f, .19f) * std::max(0.f, -sn) * blend};
          const float span = .10f + .12f * std::max(0.f, -sn),
                      low = -.225f - .5f * span;
          weights = y < low ? wt(shin)
                    : y < low + span
                        ? wt(shin, thigh, 1 - smooth((y - low) / span))
                        : wt(thigh);
        }
        // Posterior ease is narrower than the broad front/side drape. This
        // leaves the calf/hamstring contact envelope clear in deep crouch.
        p.z +=
            (.0180f * g(p.y, -.095f, .070f) + .0065f * g(p.y, -.360f, .075f)) *
            std::max(0.f, -sn);
        // The cuff drapes over the measured service-boot instep. Its lower
        // ring follows the same ankle blend as the boot, preserving clearance
        // as the foot counter-rotates beneath a bent shin.
        const float break_ease=g(p.y,-.446f,.030f);
        p.z += .0125f*break_ease + .0125f*break_ease*sn;
        // Coarser Far polygons chord inside the analytic cuff; preserve the
        // boot clearance with local ease rather than extra subdivisions.
        if (!b.near) { p.x += .0030f*break_ease*cs; p.z += .0030f*break_ease*sn; }
        if(p.y<-.360f) weights=wt(foot,shin,1-smooth((p.y+.414f)/.054f));
        next[i] = pattern.node(p, cloth, weights);
      }
      const float v0 = float(j - 1) / (hip_rows + leg_rows),
                  v1 = float(j) / (hip_rows + leg_rows);
      for (int i = 0; i < count; ++i) {
        const int k = (i + 1) % count;
        const float u0 = .94f * float(i) / count,
                    u1 = .94f * float(i + 1) / count;
        // The ring flows down: opposite quad order from an upward torso.
        pattern.quad(previous[i], previous[k], next[k], next[i], tex(u0, v0),
                     tex(u1, v0), tex(u1, v1), tex(u0, v1));
      }
      previous = std::move(next);
    }
    // Narrow ankle return gives the trouser cuff fabric thickness.
    for (int row = 0; row < 2; ++row) {
      std::vector<std::uint32_t> next(count);
      for (int i = 0; i < count; ++i) {
        const P q = pattern.points[previous[i]];
        const float a = phase[i];
        next[i] =
            pattern.node(q.p + Vec3{-.0011f * std::cos(a), row ? .005f : 0,
                                    -.0011f * std::sin(a)},
                         cloth * .94f, q.w);
      }
      for (int i = 0; i < count; ++i) {
        const int k = (i + 1) % count;
        const float u0 = .94f * float(i) / count,
                    u1 = .94f * float(i + 1) / count;
        pattern.quad(previous[i], previous[k], next[k], next[i], tex(u0, .98f),
                     tex(u1, .98f), tex(u1, .99f), tex(u0, .99f));
      }
      previous = std::move(next);
    }
  }
  // A folded waistband stands just outside the tucked service shirt.
  std::vector<std::uint32_t> previous = waist;
  for (int row = 1; row <= 4; ++row) {
    std::vector<std::uint32_t> next(2 * half);
    for (int i = 0; i < 2 * half; ++i) {
      const float a = pi * .5f + pi * float(i) / half;
      const float y = top + .004f * std::min(row, 3);
      const float inset = row == 4 ? .0012f : 0;
      next[i] = pattern.node(
          {(rx - inset) * std::cos(a), y, (rz - inset) * std::sin(a)},
          cloth * .92f, wt(B::Pelvis));
    }
    for (int i = 0; i < 2 * half; ++i) {
      int k = (i + 1) % (2 * half);
      float u0 = .94f * i / (2 * half), u1 = .94f * (i + 1) / (2 * half);
      pattern.quad(previous[i], next[i], next[k], previous[k], tex(u0, .01f),
                   tex(u0, .03f), tex(u1, .03f), tex(u1, .01f));
    }
    previous = std::move(next);
  }
  pattern.finish();
}

// Vale's service boot follows the animator's supported toe/heel envelope.
// The tall shaft tucks inside the trouser cuff; the sole has exact flat
// support.
void make_vale_boots(Sculpt &b) {
  for (float side : {-1.f, 1.f}) {
    const B foot = side > 0 ? B::RightFoot : B::LeftFoot;
    const B shin = side > 0 ? B::RightShin : B::LeftShin;
    const auto ankle_weight=[&](float y){return wt(foot,shin,1-smooth((y+.414f)/.054f));};
    const float x = side * .059f;
    const Vec3 rubber{.042f, .046f, .050f}, leather = b.m.appearance.shoes;
    const auto boot = [&](float a, float v) {
      const float yy[]{-.485f, -.477f, -.465f, -.451f, -.436f, -.418f};
      const float xx[]{.034f, .033f, .031f, .027f, .0248f, .024f};
      const float zz[]{.066f, .063f, .056f, .041f, .029f, .026f};
      const float cc[]{.033f, .031f, .026f, .015f, .006f, .002f};
      const float t = v * 5;
      const int i = std::min(4, int(t));
      const float f = t - i;
      const float rx = lerp(xx[i], xx[i + 1], f),
                  rz = lerp(zz[i], zz[i + 1], f),
                  cz = lerp(cc[i], cc[i + 1], f);
      return Vec3{x + rx * std::cos(a), lerp(yy[i], yy[i + 1], f),
                  cz + rz * std::sin(a)};
    };
    b.grid(b.near ? 28 : 12, b.near ? 3 : 2, [&](float u, float v) {
      const float a = 2 * pi * u, ridge = .00065f * std::sin(pi * v);
      return P{{x + (.0355f + ridge) * std::cos(a), lerp(-.5f, -.4848f, v),
                .033f + (.067f + ridge) * std::sin(a)},
               rubber,
               wt(foot),
               character_surface_uv(S::Rubber, u, v)};
    });
    const unsigned upper_start = b.m.bind_mesh.vertices.size();
    b.grid(b.near ? 32 : 10, b.near ? 10 : 5, [&](float u, float v) {
      const auto p=boot(2*pi*u,v);
      return P{p,leather,ankle_weight(p.y),character_surface_uv(S::Leather,u,v)};
    });
    // Lines are fitted to the actual discrete upper, rather than floating at
    // analytic positions above the coarse triangles of the Far boot.
    const auto front = [&](float xx, float yy) {
      float zz = -1;
      const auto &mesh = b.m.bind_mesh;
      for (std::size_t k = 0; k < mesh.indices.size(); k += 3) {
        if (mesh.indices[k] < upper_start ||
            mesh.indices[k + 1] < upper_start ||
            mesh.indices[k + 2] < upper_start)
          continue;
        const Vec3 a = mesh.vertices[mesh.indices[k]].position * (1 / b.h),
                   c = mesh.vertices[mesh.indices[k + 1]].position * (1 / b.h),
                   d = mesh.vertices[mesh.indices[k + 2]].position * (1 / b.h);
        const float det = (c.x - a.x) * (d.y - a.y) - (c.y - a.y) * (d.x - a.x);
        if (std::fabs(det) < 1e-12f)
          continue;
        const float q = ((xx - a.x) * (d.y - a.y) - (yy - a.y) * (d.x - a.x)) /
                        det,
                    r = ((c.x - a.x) * (yy - a.y) - (c.y - a.y) * (xx - a.x)) /
                        det;
        if (q >= 0 && r >= 0 && q + r <= 1)
          zz = std::max(zz, a.z + q * (c.z - a.z) + r * (d.z - a.z));
      }
      return zz;
    };
    for (int row = 0; row < (b.near ? 4 : 1); ++row) {
      const float y = lerp(-.458f, -.427f, b.near ? row / 3.f : .5f);
      std::vector<Vec3> lace;
      for (int i = 0; i <= (b.near?6:4); ++i) {
        const float xx = x + lerp(-.010f, .010f, i / float(b.near?6:4)),
                    yy = y + .0016f * std::sin(pi * i / float(b.near?6:4));
        lace.push_back({xx, yy, front(xx, yy) + .0010f});
      }
      const auto lace_begin=b.m.influences.size();
      b.curve(lace, .00065f, leather * .50f, S::Shirt, foot, b.near ? 6 : 4);
      for(std::size_t i=lace_begin;i<b.m.influences.size();++i)b.m.influences[i]=ankle_weight(b.m.bind_mesh.vertices[i].position.y/b.h);
    }
    if (b.near) {
      std::vector<Vec3> welt;
      for (int i = 0; i <= 32; ++i) {
        float a = 2 * pi * i / 32.f;
        Vec3 p = boot(a, 0);
        p += Vec3{.0008f * std::cos(a), .0008f, .0008f * std::sin(a)};
        welt.push_back(p);
      }
      b.curve(welt, .00052f, leather * .58f, S::Leather, foot, 4);
    }
  }
}
CharacterRig vale_rig(float height) {
  CharacterRig r;
  r.height = height;
  const auto joint = [&](B bone, int parent, Vec3 p) {
    r.joints[bone_index(bone)] = {parent, p * height};
  };
  const auto child = [&](B bone, B parent, Vec3 p) {
    joint(bone, int(bone_index(parent)), p);
  };
  joint(B::Pelvis, -1, {0, 0, 0});
  child(B::Spine, B::Pelvis, {0, .13f, 0});
  child(B::Chest, B::Spine, {0, .28f, 0});
  child(B::Neck, B::Chest, vale_anatomy::neck_center);
  child(B::Head, B::Neck, vale_anatomy::head_center);
  for (float s : {-1.f, 1.f}) {
    const bool q = s > 0;
    B upper = q ? B::RightUpperArm : B::LeftUpperArm,
      fore = q ? B::RightForearm : B::LeftForearm,
      hand = q ? B::RightHand : B::LeftHand,
      thigh = q ? B::RightThigh : B::LeftThigh,
      shin = q ? B::RightShin : B::LeftShin,
      foot = q ? B::RightFoot : B::LeftFoot;
    child(upper, B::Chest, {s * .124f, .314f, 0});
    child(fore, upper, {s * .164f, .12f, .008f});
    child(hand, fore, {s * .181f, -.044f, .018f});
    child(thigh, B::Pelvis, {s * .059f, 0, 0});
    child(shin, thigh, {s * .059f, -.225f, 0});
    child(foot, shin, {s * .059f, -.46f, 0});
  }
  return r;
}
} // namespace
CharacterProfile make_vale_character_profile(float height, CharacterRole role,
                                             std::string_view identity,
                                             CharacterLod lod) {
  if (!std::isfinite(height) || height <= 0 ||
      (lod != CharacterLod::Near && lod != CharacterLod::Far))
    throw std::invalid_argument("Invalid Vale profile request");
  CharacterProfile p;
  auto &m = p.model;
  m.role = role;
  m.lod = lod;
  m.seed = character_seed(identity);
  m.appearance = character_appearance(m.seed);
  m.rig = vale_rig(height);
  m.dual_quaternion_skinning = true;
  m.bind_mesh.vertices.reserve(lod == CharacterLod::Near ? 32000 : 16000);
  m.bind_mesh.indices.reserve(lod == CharacterLod::Near ? 150000 : 80000);
  m.influences.reserve(m.bind_mesh.vertices.capacity());
  Sculpt b{m, height, lod == CharacterLod::Near, {}, {}};
  const auto shirt =
      make_vale_shirt(b, anatomy_radius, vale_anatomy::neck_center);
  make_vale_trousers(b);
  make_vale_boots(b);
  make_hands_and_forearms(b);
  make_head(b);
  make_vale_cap(b, anatomy_radius);
  make_vale_vest(b, shirt);
  b.finish();
  p.material = make_character_surface_material();
  p.material.alpha_cutoff = .4f;
  p.lod_distance = 12;
  p.individualized = true;
  return p;
}
} // namespace fury
