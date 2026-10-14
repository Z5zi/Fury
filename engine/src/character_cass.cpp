#include "character_cass_jacket.hpp"
#include "character_sculpt.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/cass_anatomy.hpp"
#include "fury/generated/cass_hands.hpp"
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
  for (std::size_t i = 0; i < cass_anatomy::indices.size(); i += 3) {
    const Vec3 a = cass_anatomy::positions[cass_anatomy::indices[i]];
    const Vec3 b = cass_anatomy::positions[cass_anatomy::indices[i + 1]];
    const Vec3 c = cass_anatomy::positions[cass_anatomy::indices[i + 2]];
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
  for (std::size_t i = 0; i < cass_anatomy::indices.size(); i += 3) {
    const Vec3 a = cass_anatomy::positions[cass_anatomy::indices[i]];
    const Vec3 b = cass_anatomy::positions[cass_anatomy::indices[i + 1]];
    const Vec3 c = cass_anatomy::positions[cass_anatomy::indices[i + 2]];
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
CharacterInfluence cass_neck_weight(float y) {
  return y < .357f ? wt(B::Neck, B::Chest, smooth((y - .333f) / .024f))
                   : wt(B::Head, B::Neck, smooth((y - .357f) / .027f));
}
CharacterInfluence cass_head_weight(Vec3 p) {
  // The source jaw descends in front of the neck. A horizontal Y-only blend
  // pulls the chin and inner mouth toward Neck during combined attention.
  // Move the transition below the actual forward jaw while keeping the
  // posterior neck gradual and the complete source attachment loop unchanged.
  const float forward_jaw = smooth((p.z - (cass_anatomy::jaw_center.z - .047f)) / .022f);
  const float start = lerp(.357f, .346f, forward_jaw);
  const float end = lerp(.384f, .358f, forward_jaw);
  const float head = smooth((p.y - start) / (end - start));
  if (head <= 0) return cass_neck_weight(p.y);
  if (head >= 1) return wt(B::Head);
  const float neck = smooth((p.y - .333f) / .024f);
  const float chest = std::min(1 - head, (1 - head) * (1 - neck));
  return {std::uint8_t(B::Head), std::uint8_t(B::Neck), head,
          std::uint8_t(B::Chest), chest};
}

CharacterInfluence cass_source_weight(std::size_t index) {
  static const auto mouth = [] {
    std::array<bool, cass_anatomy::positions.size()> result{};
    for (std::size_t t = 0; t < cass_anatomy::indices.size(); t += 3) {
      const auto uv = cass_anatomy::uv_coordinates[cass_anatomy::uv_indices[t]];
      if (uv.x > .75f && uv.x < .93f && uv.y < .13645f)
        for (int k = 0; k < 3; ++k) result[cass_anatomy::indices[t + k]] = true;
    }
    return result;
  }();
  return mouth[index] ? wt(B::Head) : cass_head_weight(cass_anatomy::positions[index]);
}
CharacterInfluence cass_scarf_weight(Vec3 point) {
  if (point.y < .345f) return cass_neck_weight(point.y);
  const Vec3 origin{0, point.y, cass_anatomy::neck_center.z};
  const Vec3 direction = normalize(Vec3{point.x, 0, point.z - origin.z});
  float best = -1;
  std::array<float, character_bone_count> weights{};
  // Follow the actual triangulated anatomical neck, including its fitted
  // head transition. A Y-only accessory blend does not follow a source facet
  // spanning that transition and can cut through skin during attention.
  for (std::size_t t = 0; t < cass_anatomy::indices.size(); t += 3) {
    const auto ia = cass_anatomy::indices[t], ib = cass_anatomy::indices[t+1], ic = cass_anatomy::indices[t+2];
    const Vec3 a = cass_anatomy::positions[ia], b = cass_anatomy::positions[ib], c = cass_anatomy::positions[ic];
    const Vec3 e1 = b-a, e2 = c-a, q = cross(direction,e2);
    const float det = dot(e1,q);
    if (std::fabs(det) < 1e-12f) continue;
    const Vec3 offset = origin-a;
    const float u = dot(offset,q)/det;
    const Vec3 r = cross(offset,e1);
    const float v = dot(direction,r)/det, d = dot(e2,r)/det;
    if (u < 0 || v < 0 || u+v > 1 || d <= best) continue;
    best = d; weights.fill(0);
    for (const auto &item : std::array<std::pair<unsigned,float>,3>{{{ia,1-u-v},{ib,u},{ic,v}}}) {
      const auto w = cass_source_weight(item.first);
      weights[w.first] += item.second*w.first_weight;
      weights[w.second] += item.second*(1-w.first_weight-w.third_weight);
      weights[w.third] += item.second*w.third_weight;
    }
  }
  if (best < 0) throw std::runtime_error("Cass scarf has no source neck support at " + std::to_string(point.x) + "," + std::to_string(point.y) + "," + std::to_string(point.z));
  const float head = std::clamp(weights[std::size_t(B::Head)],0.f,1.f);
  const float chest = std::clamp(weights[std::size_t(B::Chest)],0.f,1-head);
  return {std::uint8_t(B::Head),std::uint8_t(B::Neck),head,std::uint8_t(B::Chest),chest};
}

void make_head(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> corners;
  std::vector<std::vector<std::uint32_t>> groups(
      cass_anatomy::positions.size());
  for (std::size_t triangle = 0; triangle < cass_anatomy::indices.size();
       triangle += 3) {
    std::uint32_t out[3];
    for (int corner = 0; corner < 3; ++corner) {
      const auto index = cass_anatomy::indices[triangle + corner];
      const auto uv_index = cass_anatomy::uv_indices[triangle + corner];
      const auto key = std::make_pair(index, uv_index);
      auto found = corners.find(key);
      if (found == corners.end()) {
        const Vec3 position = cass_anatomy::positions[index];
        const Vec2 uv = cass_anatomy::uv_coordinates[uv_index];
        const float lip = g(position.x, 0, .015f) *
                          g(position.y, .3884566f, .0035f) *
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
                          g(position.y, .4270f, .006f) * front;
        Vec3 color = skin * (1 - .025f * vascular * front);
        color.x *= 1 - .015f * lid;
        color.y *= 1 - .055f * lid;
        color.z *= 1 - .037f * lid;
        color.y *= 1 - .052f * cheeks - .043f * nose;
        color.z *= 1 - .077f * cheeks - .054f * nose;
        color.y *= 1 - .22f * lip;
        color.z *= 1 - .19f * lip;
        // The canonical internal-mouth UV chart is cavity tissue, never a
        // stripe of bright external skin. Geometry and UVs remain unchanged.
        if (uv.x > .75f && uv.x < .93f && uv.y < .14f)
          color = skin * .23f;
        const auto vertex =
            b.vertex({position, color, cass_source_weight(index),
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
  auto boundary = cass_anatomy::attachment_boundary_indices;
  float area = 0;
  for (std::size_t i = 0; i < boundary.size(); ++i) {
    const Vec3 a = cass_anatomy::positions[boundary[i]];
    const Vec3 c = cass_anatomy::positions[boundary[(i + 1) % boundary.size()]];
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
        Vec3 point = cass_anatomy::positions[boundary[i]];
        point.y -= .015f * (1 - v);
        return P{point, skin, cass_neck_weight(point.y),
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
  for (std::size_t ei = 0; ei < cass_anatomy::eye_centers.size(); ++ei) {
    const Vec3 eye = cass_anatomy::eye_centers[ei] +
                     Vec3{ei == 0 ? .0010f : -.0010f, -.0011f, -.00160f};
    const Vec3 radius = cass_anatomy::eye_helper_half_extents[ei];
    b.ellipsoid(eye, radius, {.79f, .80f, .77f}, S::Eyes, B::Head,
                b.near ? 20 : 10, b.near ? 8 : 4);
    if (b.near) {
      const auto &rim = ei == 0 ? cass_anatomy::left_eye_opening
                                : cass_anatomy::right_eye_opening;
      // Follow each ordered source-boundary arc between its horizontal
      // extrema. Sorting a mixed upper/lower subset by X connected opposing
      // corners across the socket and produced folded eyelid tubes.
      const auto lo = std::size_t(std::min_element(
          rim.begin(), rim.end(), [](Vec3 a, Vec3 c) { return a.x < c.x; }) - rim.begin());
      const auto hi = std::size_t(std::max_element(
          rim.begin(), rim.end(), [](Vec3 a, Vec3 c) { return a.x < c.x; }) - rim.begin());
      for (int direction : {-1, 1}) {
        std::vector<Vec3> line;
        std::vector<float> widths;
        std::size_t index = lo;
        while (true) {
          Vec3 p = rim[index];
          const float edge = std::min(p.x - rim[lo].x, rim[hi].x - p.x);
          if (edge > .00065f) {
            p.z += .00010f;
            line.push_back(p);
            widths.push_back(.000085f * smooth(edge / .0015f));
          }
          if (index == hi) break;
          index = std::size_t((int(index) + direction + int(rim.size())) % int(rim.size()));
        }
        if (line.size() > 1)
          b.tube(line, widths, .6f,
                 {skin.x * .85f, skin.y * .74f, skin.z * .72f}, S::Eyes,
                 [](float) { return wt(B::Head); }, 6);
      }
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
            y = eye.y + .0128f + .0019f * std::sin(pi * outward) -
                .0013f * outward;
      float z = anatomy_front(x, y);
      brow.push_back({x, y, z + .0004f});
      brow_radius.push_back(
          .00012f + .00095f * std::pow(std::max(0.f, std::sin(pi * t)), .6f));
    }
    b.tube(
        brow, brow_radius, .35f, b.m.appearance.hair * .74f, S::Hair,
        [](float) { return wt(B::Head); }, b.near ? 6 : 4);
  }
}

// Cass's relaxed bind turns each palm toward the thigh, with thumbs forward.
// This fixed rigid fitting is shared by source fingers and the sewn wrist ring;
// it is not a finger controller or per-frame hand animation.
Vec3 cass_hand_offset(Vec3 p, float side) {
  p.x *= side;
  const float a = side * 1.12f, c = std::cos(a), sn = std::sin(a);
  return {c * p.x + sn * p.z, p.y, -sn * p.x + c * p.z};
}
void make_hands_and_forearms(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  for (float side : {-1.f, 1.f}) {
    const B fore = side > 0 ? B::RightForearm : B::LeftForearm;
    const B upper = side > 0 ? B::RightUpperArm : B::LeftUpperArm;
    const B hand = side > 0 ? B::RightHand : B::LeftHand;
    const Vec3 wrist{side * .163f, -.043f, .018f};
    std::map<std::pair<unsigned, unsigned>, unsigned> corners;
    std::vector<std::vector<unsigned>> groups(cass_hands::positions.size());
    const auto emit = [&](auto const &indices, auto const &uv_indices) {
      for (std::size_t t = 0; t < indices.size(); t += 3) {
        unsigned out[3];
        for (int k = 0; k < 3; ++k) {
          auto index = indices[t + k], uv = uv_indices[t + k];
          const bool hard =
              b.near
                  ? std::find(cass_hands::hard_normal_triangle_indices.begin(),
                              cass_hands::hard_normal_triangle_indices.end(),
                              t / 3) !=
                        cass_hands::hard_normal_triangle_indices.end()
                  : std::find(
                        cass_hands::far_hard_normal_triangle_indices.begin(),
                        cass_hands::far_hard_normal_triangle_indices.end(),
                        t / 3) !=
                        cass_hands::far_hard_normal_triangle_indices.end();
          auto key = std::make_pair(
              index,
              uv + (hard ? unsigned((t + 1) * cass_hands::uv_coordinates.size())
                         : 0));
          auto found = corners.find(key);
          if (found == corners.end()) {
            Vec3 p = cass_hand_offset(cass_hands::positions[index], side);
            p += wrist;
            const Vec2 tex = cass_hands::uv_coordinates[uv];
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
      emit(cass_hands::near_indices, cass_hands::near_uv_indices);
    else
      emit(cass_hands::far_indices, cass_hands::far_uv_indices);
    // Exactly reuse every anatomical wrist point at the terminal forearm ring.
    // The opposite terminal ring tucks inside the open rolled cuff, not a cap.
    auto boundary = cass_hands::wrist_boundary_indices;
    float area = 0;
    for (std::size_t i = 0; i < boundary.size(); ++i) {
      auto a = cass_hands::positions[boundary[i]],
           c = cass_hands::positions[boundary[(i + 1) % boundary.size()]];
      area += a.x * c.z - c.x * a.z;
    }
    if (area * side < 0)
      std::reverse(boundary.begin(), boundary.end());
    Vec3 center{};
    for (auto i : boundary)
      center += cass_hands::positions[i];
    center = center * (1.f / boundary.size());
    const unsigned start = b.m.bind_mesh.vertices.size();
    const int columns = boundary.size(), rows = b.near ? 5 : 3;
    b.grid(
        columns, rows,
        [&](float u, float v) {
          const auto index =
              boundary[std::size_t(std::lround(u * columns)) % boundary.size()];
          Vec3 source = cass_hand_offset(cass_hands::positions[index], side);
          source += wrist;
          const Vec3 local =
              cass_hand_offset(cass_hands::positions[index] - center, side);
          const float a = std::atan2(local.z, local.x);
          const float y = lerp(source.y, -.020f, v),
                      radius = .0150f + .0030f * smooth(v);
          const float x = side * lerp(.163f, .139f, smooth((y + .043f) / .18f));
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
void make_trousers_and_shoes(Sculpt &b) {
  const Vec3 cloth = b.m.appearance.trousers;
  const int columns = b.near ? 28 : 12, rows = b.near ? 24 : 9;
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
                y = lerp(-.429f, -.09f, std::min(v / .78f, 1.f));
          float radius = .027f + .011f * g(y, -.34f, .073f) +
                         .018f * smooth((y + .24f) / .19f);
          const float fold =
              .0028f * g(y, -.22f + .012f * cs, .025f) * sn +
              .0022f * g(y, -.446f, .021f) * std::sin(2 * a + y * 210);
          Vec3 p{side * (.055f + (radius + fold) * cs), y,
                 (radius * 1.10f + fold) * sn - .002f};
          const float join = smooth((v - .78f) / .22f);
          const Vec3 top{side * .096f * std::max(0.f, cs),
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
    const auto boot_weight = [&](float y) {
      return wt(foot, shin, 1 - smooth((y + .474f) / .032f));
    };
    const float x = side * .055f;
    b.grid(b.near ? 28 : 10, 1, [&](float u, float v) {
      float a = 2 * pi * u;
      return P{{x + .034f * std::cos(a), lerp(-.5f, -.484f, v),
                .027f + .066f * std::sin(a)},
               {.061f, .065f, .069f},
               wt(foot),
               character_surface_uv(S::Rubber, u, v)};
    });
    const auto boot_section = [](float v, const std::array<float, 6> &values) {
      int k = std::min(4, int(v * 5));
      return lerp(values[k], values[k + 1], smooth(v * 5 - k));
    };
    const auto upper_start = b.m.bind_mesh.indices.size();
    b.grid(
        b.near ? 32 : 12, b.near ? 10 : 4,
        [&](float u, float v) {
          float a = 2 * pi * u, yy = lerp(-.484f, -.416f, v);
          return P{
              {x + boot_section(v,
                                {.033f, .0335f, .031f, .026f, .024f, .024f}) *
                       std::cos(a),
               yy,
               boot_section(v, {.027f, .027f, .021f, .007f, .001f, 0.f}) +
                   boot_section(v, {.065f, .064f, .050f, .034f, .027f, .027f}) *
                       std::sin(a)},
              b.m.appearance.shoes,
              boot_weight(yy),
              character_surface_uv(S::Leather, u, v)};
        },
        true, false);
    const auto upper_end = b.m.bind_mesh.indices.size();
    auto shoe_top = [&](float xx, float zz) {
      float highest = -1;
      for (std::size_t i = upper_start; i < upper_end; i += 3) {
        Vec3 a = b.m.bind_mesh.vertices[b.m.bind_mesh.indices[i]].position *
                 (1 / b.h),
             c = b.m.bind_mesh.vertices[b.m.bind_mesh.indices[i + 1]].position *
                 (1 / b.h),
             d = b.m.bind_mesh.vertices[b.m.bind_mesh.indices[i + 2]].position *
                 (1 / b.h);
        Vec3 e = c - a, f = d - a;
        float det = e.x * f.z - e.z * f.x;
        if (std::fabs(det) < 1e-12f)
          continue;
        float u = ((xx - a.x) * f.z - (zz - a.z) * f.x) / det,
              v = (e.x * (zz - a.z) - e.z * (xx - a.x)) / det;
        if (u >= -1e-6f && v >= -1e-6f && u + v <= 1.000001f)
          highest = std::max(highest, a.y + u * e.y + v * f.y);
      }
      if (highest < -.6f || highest > -.40f)
        throw std::runtime_error("Cass boot lace has no upper support");
      return highest;
    };
    if (b.near)
      b.grid(
          4, 8,
          [&](float u, float v) {
            float xx = x + lerp(-.013f, .013f, u), zz = lerp(.029f, .072f, v);
            return P{{xx, shoe_top(xx, zz) + .00085f, zz},
                     b.m.appearance.shoes * .90f,
                     boot_weight(shoe_top(xx, zz) + .00085f),
                     character_surface_uv(S::Leather, u, v)};
          },
          false, false);
    // A shaped ankle underlay bridges the trouser opening to the moving shoe.
    // Its weights continue the shin-to-foot bend, avoiding an empty cuff gap.
    b.grid(
        b.near ? 20 : 10, b.near ? 6 : 2,
        [&](float u, float v) {
          float a = 2 * pi * u, y = lerp(-.477f, -.432f, v),
                rx = lerp(.024f, .0228f, v);
          return P{{x + rx * std::cos(a), y, .005f + rx * .93f * std::sin(a)},
                   Vec3{.071f, .078f, .083f},
                   boot_weight(y),
                   character_surface_uv(S::Shirt, u, v)};
        },
        true, false);
    // Recessed dark laces follow the actual triangulated instep. Side and toe
    // inspection exposed buried laces on the taller early boot envelope.
    for (int lace = 0; lace < (b.near ? 4 : 2); ++lace) {
      float z = .035f + lace * (b.near ? .010f : .020f);
      std::vector<Vec3> line;
      for (int j = 0; j <= 4; ++j) {
        float xx = x + lerp(-.013f, .013f, float(j) / 4);
        line.push_back({xx, shoe_top(xx, z) + .0016f, z});
      }
      b.tube(line, std::vector<float>(line.size(), .00065f), 1.f,
             {.15f, .16f, .16f}, S::Shirt,
             [&](float t) {
               const auto i = std::min(line.size() - 1, std::size_t(std::lround(t * (line.size() - 1))));
               return boot_weight(line[i].y);
             }, b.near ? 6 : 4);
    }
  }
  for (std::size_t i = 0; i < seams[0].size(); ++i)
    b.normal_groups.push_back({seams[0][i], seams[1][i]});
  b.grid(
      b.near ? 48 : 20, 3,
      [&](float u, float v) {
        float a = 2 * pi * u;
        return P{{.0965f * std::cos(a), lerp(.0565f, .071f, v),
                  .061f * std::sin(a) - .002f},
                 cloth * .94f,
                 wt(B::Pelvis),
                 character_surface_uv(S::Pants, u, v)};
      },
      true, false);
}

float cass_hairline(float a) {
  const float front = std::max(0.f, std::sin(a)),
              back = std::max(0.f, -std::sin(a));
  // A slight off-center break keeps the swept front from reading as a
  // straight bowl-cut edge. This original hairline retains the complete nape.
  const float part = .0038f * g(std::cos(a), -.48f, .24f) * front * front;
  return .437f + .021f * front - .032f * back +
         .008f * std::pow(std::fabs(std::cos(a)), 10.f) +
         .0024f * std::cos(a) * front * front +
         .0011f * std::sin(3 * a + .6f) + .0004f * std::sin(11 * a) + part;
}
Vec3 cass_scalp(float a, float t, float lift = 0) {
  t = std::clamp(t, 0.f, 1.f);
  const float f = std::sin(t * pi * .5f);
  const float source_y = lerp(cass_hairline(a), .493f, f);
  const float y = source_y + .007f * f * f;
  const float cz =
      lerp(cass_anatomy::head_center.z, cass_anatomy::crown_surface.z,
           smooth((source_y - .450f) / .043f));
  const float taper = std::sqrt(
      std::max(0.f, 1 - sq(std::max(0.f, (source_y - .478f) / .015f))));
  float r = anatomy_radius(std::min(source_y, .49297f), a, cz) +
            (.0045f + lift) * taper;
  const float sweep =
      .0032f * std::sin(pi * t) * smooth((source_y - .442f) / .028f);
  r = std::max(.00020f, r);
  return {r * std::cos(a) + sweep, y, cz + r * std::sin(a)};
}
void make_cass_hair(Sculpt &b) {
  const Vec3 color = b.m.appearance.hair;
  constexpr int sectors = 8;
  const int cols = b.near ? 8 : 3, rows = b.near ? 12 : 6;
  std::array<unsigned, sectors> starts{};
  for (int s = 0; s < sectors; ++s) {
    starts[s] = unsigned(b.m.bind_mesh.vertices.size());
    b.grid(
        cols, rows,
        [&](float u, float v) {
          const float a = 2 * pi * (s + u) / sectors;
          // Eight physical sectors share exact boundaries; UVs follow local
          // growth.
          return P{cass_scalp(a, .018f + .982f * v, b.near ? 0 : .0028f),
                   color * (.78f + .06f * smooth(v / .35f)), wt(B::Head),
                   character_surface_uv(S::HairCrop, u, v)};
        },
        false, false);
  }
  for (int s = 0; s < sectors; ++s)
    for (int row = 0; row <= rows; ++row) {
      unsigned first = starts[s] + row * (cols + 1),
               last = starts[(s + sectors - 1) % sectors] + row * (cols + 1) +
                      cols;
      b.m.bind_mesh.vertices[last].position =
          b.m.bind_mesh.vertices[first].position;
      b.normal_groups.push_back({first, last});
    }
  const auto pole = b.vertex({{0, .5f, cass_anatomy::crown_surface.z},
                              color * .84f,
                              wt(B::Head),
                              character_surface_uv(S::HairCrop, .5f, 1)});
  for (int s = 0; s < sectors; ++s)
    for (int i = 0; i < cols; ++i) {
      unsigned a = starts[s] + rows * (cols + 1) + i;
      b.tri(a, pole, a + 1);
    }
  // Uneven overlapping locks follow a sweep across the upper/front scalp,
  // rather than comb-like vertical tubes. The cap remains opaque beneath.
  for (int k = 0; k < (b.near ? 34 : 6); ++k) {
    float q = float(k) / (b.near ? 34.f : 6.f),
          a0 = pi * .5f - 1.14f + 2.28f * q;
    const auto point = [&](float t) {
      const float a = a0 - .62f * std::sin(t * pi * .68f);
      const float height = .03f + .74f * t;
      return cass_scalp(a, height,
                        (b.near ? .0006f : .0034f) + .0013f * std::sin(pi * t));
    };
    b.grid(
        2, b.near ? 9 : 5,
        [&](float u, float v) {
          Vec3 p = point(v),
               tangent = normalize(point(std::min(1.f, v + .002f)) -
                                   point(std::max(0.f, v - .002f)));
          Vec3 normal = normalize(Vec3{p.x, p.y - .433f, p.z - .024f});
          Vec3 across = normalize(cross(tangent, normal));
          p += across * ((.5f - u) * (.0032f + .0010f * std::sin(k * 1.91f)) *
                         (.45f + .55f * std::sin(pi * v)));
          return P{p, color * (.72f + .025f * std::sin(k * 2.23f)), wt(B::Head),
                   character_surface_uv(S::HairShortStrands, u, 1 - v)};
        },
        false, false);
  }
  // Soft boundary cards around the nape and temples, with no rectangular gap.
  for (int k = 0; k < (b.near ? 40 : 8); ++k) {
    const float a0 = 2 * pi * k / (b.near ? 40.f : 8.f);
    b.grid(
        2, b.near ? 5 : 3,
        [&](float u, float v) {
          const float a = a0 + (.5f - u) * (b.near ? .055f : .12f) -
                          .07f * std::sin(pi * v);
          const float t = lerp(.23f, -.032f, v);
          Vec3 p = cass_scalp(a, std::max(0.f, t),
                              (b.near ? .0004f : .0033f) +
                                  .0005f * std::sin(pi * v));
          if (t < 0)
            p.y += .075f * t;
          return P{p, color * .81f, wt(B::Head),
                   character_surface_uv(S::HairShortStrands, u, v)};
        },
        false, false);
  }
}

// Cut edges receive their own normal corners while their physical points,
// influences and UV values stay exact. A thin shell must not average its
// front, back and edge into a reversed shading normal.
void crease_quad(Sculpt &b, unsigned a, unsigned c, unsigned d, unsigned e) {
  const auto copy = [&](unsigned i) {
    const auto &v = b.m.bind_mesh.vertices[i];
    return b.vertex({v.position * (1 / b.h), v.color, b.m.influences[i], v.uv});
  };
  const auto aa = copy(a), cc = copy(c), dd = copy(d), ee = copy(e);
  b.tri(aa, cc, dd);
  b.tri(aa, dd, ee);
}

// Glasses are opaque dark lenses in the existing material system. Their
// solid topology and restrained shine are not optical refraction simulation.
void make_cass_glasses(Sculpt &b) {
  const Vec3 frame{.027f, .030f, .031f}, lens{.029f, .039f, .041f};
  constexpr float cy = .430f, cx = .0220f, rx = .0185f, ry = .0104f;
  const auto plane = [](float x, float y) {
    return .0895f - .34f * std::max(0.f, std::fabs(x) - .018f) +
           .10f * (y - .430f);
  };
  const int n = b.near ? 32 : 16;
  const auto outline = [&](float side, float a, float scale = 1.f) {
    float c = std::cos(a), s = std::sin(a);
    const float x =
        side * cx + scale * rx * std::copysign(std::pow(std::fabs(c), .64f), c);
    const float y =
        cy + scale * ry * std::copysign(std::pow(std::fabs(s), .73f), s);
    return Vec3{x, y, plane(x, y)};
  };
  for (float side : {-1.f, 1.f}) {
    // Closed toroidal rim with exact end seam, no capped overlapping ends.
    const auto rim_start = unsigned(b.m.bind_mesh.vertices.size());
    const int rim_rows = b.near ? 6 : 4;
    b.grid(
        n, rim_rows,
        [&](float u, float v) {
          float a = 2 * pi * u, theta = -2 * pi * v;
          Vec3 center = outline(side, a), next = outline(side, a + .001f),
               prev = outline(side, a - .001f);
          const Vec3 tangent = normalize(next - prev);
          const Vec3 outward = normalize(cross(tangent, {0, 0, 1}));
          Vec3 p = center + outward * (.00125f * std::cos(theta)) +
                   Vec3{0, 0, .00125f * std::sin(theta)};
          return P{p, frame, wt(B::Head),
                   character_surface_uv(S::Rubber, u, v)};
        },
        true, false);
    for (int i = 0; i <= n; ++i) {
      const auto first = rim_start + i,
                 last = rim_start + rim_rows * (n + 1) + i;
      b.m.bind_mesh.vertices[last].position =
          b.m.bind_mesh.vertices[first].position;
      b.normal_groups.push_back({first, last});
    }
    // Convex front and back lens fans, with a finite edge under the rim.
    const auto front =
        b.vertex({{side * cx, cy, plane(side * cx, cy) + .00045f},
                  lens,
                  wt(B::Head),
                  character_surface_uv(S::Eyes, .5f, .5f)});
    const auto back = b.vertex({{side * cx, cy, plane(side * cx, cy) - .0007f},
                                lens * .82f,
                                wt(B::Head),
                                character_surface_uv(S::Eyes, .5f, .5f)});
    std::vector<unsigned> f(n), z(n);
    for (int i = 0; i < n; ++i) {
      const float a = 2 * pi * i / n;
      Vec3 p = outline(side, a, .974f);
      Vec2 uv = character_surface_uv(S::Eyes, .5f + .47f * std::cos(a),
                                     .5f + .47f * std::sin(a));
      f[i] = b.vertex({p, lens, wt(B::Head), uv});
      p.z -= .0008f;
      z[i] = b.vertex({p, lens * .82f, wt(B::Head), uv});
    }
    for (int i = 0; i < n; ++i) {
      int k = (i + 1) % n;
      b.tri(front, f[i], f[k]);
      b.tri(back, z[k], z[i]);
      crease_quad(b, f[i], z[i], z[k], f[k]);
    }
    // Hinges, straight temples and curved ear ends are individually fitted to
    // this head; both remain present at Far rather than losing the glasses.
    const float outer = side * (cx + rx);
    std::vector<Vec3> temple{
        {outer, .432f, plane(outer, .432f)}, {side * .0445f, .4323f, .075f},
        {side * .0467f, .4330f, .052f},      {side * .0499f, .4338f, .030f},
        {side * .0501f, .4338f, .020f},      {side * .0476f, .4290f, .011f},
        {side * .0454f, .4235f, .009f}};
    b.curve(temple, b.near ? .00135f : .00165f, frame, S::Rubber, B::Head,
            b.near ? 8 : 6, .85f);
    b.ellipsoid({outer, .432f, plane(outer, .432f)}, {.0023f, .0021f, .0020f},
                frame, S::Rubber, B::Head, b.near ? 10 : 6, 4);
    const float px = side * .0070f, py = .4287f,
                pz = anatomy_front(px, py) + .0024f;
    // Nose pads conform to this source nose rather than embedding a planar
    // ellipsoid across a sloping bridge. Positive offset is a fitting gap,
    // not contact/soft-tissue simulation.
    b.grid(b.near ? 12 : 6, b.near ? 6 : 4, [&](float u, float v) {
      const float a = 2 * pi * u, e = lerp(-pi * .49f, pi * .49f, v);
      const float x = px + .0016f * std::cos(a) * std::cos(e);
      const float y = py + .0030f * std::sin(e);
      const float z = anatomy_front(x, y) + .0024f +
                      .0007f * std::sin(a) * std::cos(e);
      return P{{x, y, z}, frame * .92f, wt(B::Head),
               character_surface_uv(S::Rubber, u, v)};
    });
    b.curve({{side * .0044f, .4352f, .0918f},
             {px, .432f, pz + .001f},
             {px, py, pz}},
            .00075f, frame, S::Rubber, B::Head, 6);
  }
  b.curve({{-.0052f, .4360f, .0904f},
           {-.0025f, .4372f, .0922f},
           {0, .4376f, .0928f},
           {.0025f, .4372f, .0922f},
           {.0052f, .4360f, .0904f}},
          .0015f, frame, S::Rubber, B::Head, b.near ? 8 : 6, .88f);
}

void make_cass_scarf(Sculpt &b) {
  const Vec3 cloth = b.m.appearance.accent;
  const float cz = cass_anatomy::neck_center.z;
  const int cols = b.near ? 40 : 32, rows = b.near ? 6 : 3;
  // A closed annular cloth shell: front drapes lower, back hugs the nape.
  std::array<std::vector<unsigned>, 2> starts;
  for (int layer = 0; layer < 2; ++layer) {
    starts[layer].resize(rows + 1);
    const auto base = unsigned(b.m.bind_mesh.vertices.size());
    b.grid(
        cols, rows,
        [&](float u, float v) {
          float a = 2 * pi * u, front = std::max(0.f, std::sin(a));
          const float low = .345f - .003f * front, high = .359f - .011f * front;
          float y = lerp(low, high, v);
          float radius = anatomy_radius(y, a, cz);
          const float folds = .0005f *
                              std::sin(3 * pi * v + .45f * std::sin(a * 2)) *
                              std::sin(pi * v);
          radius += (layer == 0 ? .0032f : .0020f) + folds + .0004f * front;
          const Vec3 point{radius * std::cos(a), y, cz + radius * std::sin(a)};
          return P{point,
                   cloth * (layer == 0 ? 1.f : .92f),
                   cass_scarf_weight(point),
                   character_surface_uv(S::Shirt, u, v)};
        },
        true, false);
    for (int j = 0; j <= rows; ++j)
      starts[layer][j] = base + j * (cols + 1);
    if (layer == 1)
      for (std::size_t t = b.m.bind_mesh.indices.size() - cols * rows * 6;
           t < b.m.bind_mesh.indices.size(); t += 3)
        std::swap(b.m.bind_mesh.indices[t + 1], b.m.bind_mesh.indices[t + 2]);
  }
  for (int end = 0; end < 2; ++end)
    for (int i = 0; i < cols; ++i) {
      unsigned a = starts[0][end * rows] + i, c = starts[0][end * rows] + i + 1,
               d = starts[1][end * rows] + i + 1, e = starts[1][end * rows] + i;
      if (end)
        crease_quad(b, a, e, d, c);
      else
        crease_quad(b, a, c, d, e);
    }
  const auto jacket_front = [&](float x, float y) {
    float front = -1.f;
    const auto low = character_surface_uv(S::Jacket, 0, 0);
    const auto high = character_surface_uv(S::Jacket, 1, 1);
    const auto &mesh = b.m.bind_mesh;
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
      const auto &vertex = mesh.vertices[mesh.indices[i]];
      if (vertex.uv.x < low.x || vertex.uv.x > high.x ||
          vertex.uv.y < low.y || vertex.uv.y > high.y ||
          vertex.color.z > 1.2f * vertex.color.y) continue;
      const Vec3 a = vertex.position * (1 / b.h);
      const Vec3 c = mesh.vertices[mesh.indices[i + 1]].position * (1 / b.h);
      const Vec3 d = mesh.vertices[mesh.indices[i + 2]].position * (1 / b.h);
      if (y < std::min({a.y, c.y, d.y}) || y > std::max({a.y, c.y, d.y})) continue;
      const float den = (c.x - a.x) * (d.y - a.y) - (c.y - a.y) * (d.x - a.x);
      if (std::fabs(den) < 1e-12f) continue;
      const float u = ((x - a.x) * (d.y - a.y) - (y - a.y) * (d.x - a.x)) / den;
      const float v = ((c.x - a.x) * (y - a.y) - (c.y - a.y) * (x - a.x)) / den;
      if (u >= -1e-6f && v >= -1e-6f && u + v <= 1.000001f)
        front = std::max(front, a.z + u * (c.z - a.z) + v * (d.z - a.z));
    }
    return front;
  };
  // An overlapping end starts against the lower front loop and lies on the
  // chest. All four cut edges are closed; both layers share identical weights.
  const int n = b.near ? 14 : 6, nx = b.near ? 8 : 4;
  std::array<std::vector<std::vector<unsigned>>, 2> tail;
  for (auto &layer : tail)
    layer.resize(n + 1, std::vector<unsigned>(nx + 1));
  for (int layer = 0; layer < 2; ++layer)
    for (int j = 0; j <= n; ++j)
      for (int i = 0; i <= nx; ++i) {
        const float u = float(i) / nx, v = float(j) / n;
        float y = lerp(.253f, .349f, v),
              x = (u - .5f) * 2 * lerp(.0205f, .0118f, smooth((v - .50f) / .38f)) + .001f + .004f * (1 - v);
        const float rx = cass_jacket_detail::section(
            y, {.107f, .101f, .091f, .104f, .111f, .108f});
        Vec3 support = cass_jacket_detail::body_point(
            std::acos(std::clamp(x / rx, -.99f, .99f)), y);
        Vec3 p{x, y, support.z + .0024f + .00045f * std::sin(pi * u)};
        const float angle = lerp(1.83f, 1.31f, u), front = std::sin(angle),
                    root_y = .351f - .003f * front;
        const float radius =
            anatomy_radius(root_y, angle, cz) + .0026f + .0004f * front;
        const Vec3 root{radius * std::cos(angle), root_y,
                        cz + radius * std::sin(angle) + .001f};
        const float join = smooth((v - .68f) / .32f);
        p = p * (1 - join) + root * join;
        const float support_front = jacket_front(p.x, p.y);
        if (support_front > -.1f)
          p.z = std::max(p.z, support_front + .0024f);
        p.z -= layer * .0012f;
        tail[layer][j][i] =
            b.vertex({p, cloth * (layer == 0 ? .98f : .92f), cass_scarf_weight(p),
                      character_surface_uv(S::Shirt, u * .5f, v * .6f)});
      }
  std::vector<std::array<unsigned, 3>> support_faces;
  std::map<unsigned, unsigned> paired;
  for (int j = 0; j <= n; ++j) for (int i = 0; i <= nx; ++i)
    paired[tail[1][j][i]] = tail[0][j][i];
  for (int j = 0; j < n; ++j) for (int i = 0; i < nx; ++i) {
    const auto &q = tail[1];
    support_faces.push_back({q[j][i], q[j][i + 1], q[j + 1][i + 1]});
    support_faces.push_back({q[j][i], q[j + 1][i + 1], q[j + 1][i]});
  }
  cass_jacket_detail::fit_front_overlay(b, support_faces,
      [&](unsigned id) { return b.m.bind_mesh.vertices[id].position * (1 / b.h); },
      [&](unsigned id, float dz) {
        b.m.bind_mesh.vertices[id].position.z += dz * b.h;
        b.m.bind_mesh.vertices[paired.at(id)].position.z += dz * b.h;
      }, 0, b.m.bind_mesh.indices.size(), .0012f, true);
  for (int j = 0; j < n; ++j)
    for (int i = 0; i < nx; ++i)
      for (int k = 0; k < 2; ++k) {
        auto &q = tail[k];
        if (k == 0) {
          b.tri(q[j][i], q[j][i + 1], q[j + 1][i + 1]);
          b.tri(q[j][i], q[j + 1][i + 1], q[j + 1][i]);
        } else {
          b.tri(q[j][i], q[j + 1][i + 1], q[j][i + 1]);
          b.tri(q[j][i], q[j + 1][i], q[j + 1][i + 1]);
        }
      }
  std::vector<std::pair<int, int>> edge;
  for (int i = 0; i < nx; ++i)
    edge.push_back({0, i});
  for (int j = 0; j < n; ++j)
    edge.push_back({j, nx});
  for (int i = nx; i > 0; --i)
    edge.push_back({n, i});
  for (int j = n; j > 0; --j)
    edge.push_back({j, 0});
  for (std::size_t q = 0; q < edge.size(); ++q) {
    auto [j, i] = edge[q];
    auto [jj, ii] = edge[(q + 1) % edge.size()];
    unsigned a = tail[0][j][i], c = tail[1][j][i], d = tail[1][jj][ii],
             e = tail[0][jj][ii];
    crease_quad(b, a, c, d, e);
  }
}

CharacterRig cass_rig(float height) {
  CharacterRig r;
  r.height = height;
  auto joint = [&](B b, int parent, Vec3 p) {
    r.joints[bone_index(b)] = {parent, p * height};
  };
  auto child = [&](B b, B parent, Vec3 p) {
    joint(b, int(bone_index(parent)), p);
  };
  joint(B::Pelvis, -1, {0, 0, 0});
  child(B::Spine, B::Pelvis, {0, .13f, 0});
  child(B::Chest, B::Spine, {0, .272f, 0});
  child(B::Neck, B::Chest, cass_anatomy::neck_center);
  child(B::Head, B::Neck, cass_anatomy::head_center);
  for (float sign : {-1.f, 1.f}) {
    bool q = sign > 0;
    B arm = q ? B::RightUpperArm : B::LeftUpperArm,
      fore = q ? B::RightForearm : B::LeftForearm,
      hand = q ? B::RightHand : B::LeftHand,
      thigh = q ? B::RightThigh : B::LeftThigh,
      shin = q ? B::RightShin : B::LeftShin,
      foot = q ? B::RightFoot : B::LeftFoot;
    child(arm, B::Chest, {sign * .116f, .310f, 0});
    child(fore, arm, {sign * .139f, .12f, .008f});
    child(hand, fore, {sign * .163f, -.044f, .018f});
    child(thigh, B::Pelvis, {sign * .055f, 0, 0});
    child(shin, thigh, {sign * .055f, -.225f, 0});
    child(foot, shin, {sign * .055f, -.46f, 0});
  }
  return r;
}
float neck_fit_radius(float y, float a, float cz) {
  y = std::max(y, .337f);
  if (y <= .350f)
    return anatomy_radius(y, a, cz);
  const float x = anatomy_radius(.345f, a, cz),
              z = anatomy_radius(.350f, a, cz);
  return std::max(.012f, z + (z - x) * ((y - .350f) / .005f));
}
} // namespace
CharacterProfile build_cass_profile(float height, CharacterRole role,
                                    std::string_view id, CharacterLod lod,
                                    bool glasses) {
  if (!std::isfinite(height) || height <= 0 ||
      (lod != CharacterLod::Near && lod != CharacterLod::Far))
    throw std::invalid_argument("Invalid individual character request");
  CharacterProfile p;
  auto &m = p.model;
  m.role = role;
  m.lod = lod;
  m.seed = character_seed(id);
  m.appearance = character_appearance(m.seed);
  m.rig = cass_rig(height);
  m.dual_quaternion_skinning = true;
  m.bind_mesh.vertices.reserve(lod == CharacterLod::Near ? 34000 : 14000);
  m.bind_mesh.indices.reserve(lod == CharacterLod::Near ? 150000 : 60000);
  m.influences.reserve(m.bind_mesh.vertices.capacity());
  Sculpt b{m, height, lod == CharacterLod::Near, {}, {}};
  make_cass_jacket(b, neck_fit_radius, cass_anatomy::neck_center);
  make_hands_and_forearms(b);
  make_trousers_and_shoes(b);
  make_head(b);
  make_cass_hair(b);
  make_cass_scarf(b);
  if (glasses)
    make_cass_glasses(b);
  b.finish();
  p.material = make_character_surface_material();
  p.material.alpha_cutoff = .4f;
  p.lod_distance = 12;
  p.individualized = true;
  return p;
}
CharacterProfile make_cass_character_profile(float height, CharacterRole role,
                                             std::string_view id,
                                             CharacterLod lod) {
  return build_cass_profile(height, role, id, lod, true);
}
// Called only by the explicitly labeled diagnostic stage. The shipping
// identity always goes through the wrapper above, with glasses present.
CharacterProfile make_cass_glasses_removed_diagnostic(float height,
                                                      CharacterLod lod) {
  return build_cass_profile(height, CharacterRole::Fence, "NpcFence", lod,
                            false);
}
} // namespace fury
