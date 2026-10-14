#include "character_renn_equipment.hpp"
#include "character_renn_shirt.hpp"
#include "character_sculpt.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/renn_anatomy.hpp"
#include "fury/generated/renn_hands.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <stdexcept>
#include <vector>
namespace fury {
namespace {
using namespace character_detail;
float anatomy_front(float x, float y) {
  float best = -1.f;
  for (std::size_t i = 0; i < renn_anatomy::indices.size(); i += 3) {
    const Vec3 a = renn_anatomy::positions[renn_anatomy::indices[i]];
    const Vec3 b = renn_anatomy::positions[renn_anatomy::indices[i + 1]];
    const Vec3 c = renn_anatomy::positions[renn_anatomy::indices[i + 2]];
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
  for (std::size_t i = 0; i < renn_anatomy::indices.size(); i += 3) {
    const Vec3 a = renn_anatomy::positions[renn_anatomy::indices[i]];
    const Vec3 b = renn_anatomy::positions[renn_anatomy::indices[i + 1]];
    const Vec3 c = renn_anatomy::positions[renn_anatomy::indices[i + 2]];
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
CharacterInfluence renn_neck_weight(float y) {
  return y < .365f ? wt(B::Neck, B::Chest, smooth((y - .333f) / .032f))
                   : wt(B::Head, B::Neck, smooth((y - .365f) / .022f));
}
void make_head(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> corners;
  std::vector<std::vector<std::uint32_t>> groups(
      renn_anatomy::positions.size());
  for (std::size_t triangle = 0; triangle < renn_anatomy::indices.size();
       triangle += 3) {
    std::uint32_t out[3];
    for (int corner = 0; corner < 3; ++corner) {
      const auto index = renn_anatomy::indices[triangle + corner];
      const auto uv_index = renn_anatomy::uv_indices[triangle + corner];
      const auto key = std::make_pair(index, uv_index);
      auto found = corners.find(key);
      if (found == corners.end()) {
        const Vec3 position = renn_anatomy::positions[index];
        const Vec2 uv = renn_anatomy::uv_coordinates[uv_index];
        const float lip = g(position.x, 0, .015f) *
                          g(position.y, .39073197f, .0035f) *
                          smooth((position.z - .079f) / .010f);
        // Small anatomical pigment zones retain the stable base complexion.
        const float cheeks = g(std::fabs(position.x), .030f, .015f) *
                             g(position.y, .410f, .015f) *
                             smooth((position.z - .067f) / .02f);
        const float nose = g(position.x, 0, .009f) *
                           g(position.y, .411f, .012f) *
                           smooth((position.z - .081f) / .013f);
        const float front = smooth((position.z - .054f) / .025f);
        const float vascular =
            .5f + .25f * std::sin(position.x * 360 + position.y * 97) +
            .25f * std::sin(position.y * 285 - position.z * 190);
        const float lid = g(std::fabs(position.x), .0185f, .008f) *
                          g(position.y, .427f, .006f) * front;
        Vec3 color = skin * (1 - .025f * vascular * front);
        color.x *= 1 - .015f * lid;
        color.y *= 1 - .055f * lid;
        color.z *= 1 - .037f * lid;
        color.y *= 1 - .052f * cheeks - .043f * nose;
        color.z *= 1 - .077f * cheeks - .054f * nose;
        color.y *= 1 - .22f * lip;
        color.z *= 1 - .19f * lip;
        const auto vertex =
            b.vertex({position, color, renn_neck_weight(position.y),
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
  auto boundary = renn_anatomy::attachment_boundary_indices;
  float area = 0;
  for (std::size_t i = 0; i < boundary.size(); ++i) {
    const Vec3 a = renn_anatomy::positions[boundary[i]];
    const Vec3 c = renn_anatomy::positions[boundary[(i + 1) % boundary.size()]];
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
        Vec3 point = renn_anatomy::positions[boundary[i]];
        point.y -= .015f * (1 - v);
        return P{point, skin, renn_neck_weight(point.y),
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
  for (std::size_t ei = 0; ei < renn_anatomy::eye_centers.size(); ++ei) {
    const Vec3 eye = renn_anatomy::eye_centers[ei] +
                     Vec3{ei == 0 ? -.0003f : .0003f, -.0002f, -.00291419f};
    const Vec3 radius = renn_anatomy::eye_helper_half_extents[ei];
    b.ellipsoid(eye, radius, {.79f, .80f, .77f}, S::Eyes, B::Head,
                b.near ? 24 : 16, b.near ? 8 : 6);
    // The complete imported eyelids carry their own anatomical rim. Avoid
    // duplicate cosmetic tubes at the canthi, where tight source curvature
    // can fold a radius-thick curve through itself.
    Vec3 aperture{};
    const auto &opening = ei == 0 ? renn_anatomy::left_eye_opening
                                  : renn_anatomy::right_eye_opening;
    for (auto point : opening)
      aperture += point;
    aperture = aperture * (1.f / opening.size());
    const float iris_dx = aperture.x - eye.x, iris_dy = aperture.y - eye.y;
    const int iris_columns = b.near ? 24 : 12, iris_rows = b.near ? 5 : 3;
    const auto iris_base = std::uint32_t(b.m.bind_mesh.vertices.size());
    b.grid(
        iris_columns, iris_rows,
        [&](float u, float v) {
          const float near_r[]{.00005f, .00122f, .00140f,
                               .00225f, .00310f, .00355f};
          const float far_r[]{.00005f, .00122f, .00140f, .00355f};
          int row = std::min(iris_rows, int(std::lround(v * iris_rows)));
          float r = b.near ? near_r[row] : far_r[row], a = 2 * pi * u,
                x = iris_dx + r * std::cos(a), y = iris_dy + r * std::sin(a);
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
    auto center =
        b.vertex({eye + Vec3{iris_dx, iris_dy,
                             radius.z * std::sqrt(std::max(
                                            0.f, 1 - sq(iris_dx / radius.x) -
                                                     sq(iris_dy / radius.y))) +
                                 .00015f},
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
            y = eye.y + .0120f + .0025f * std::sin(pi * outward) -
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

Vec3 renn_hand_offset(Vec3 p, float side) {
  p.x *= side;
  const float a = side * 1.05f, c = std::cos(a), sn = std::sin(a);
  return {c * p.x + sn * p.z, p.y, -sn * p.x + c * p.z};
}
void make_hands_and_forearms(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  for (float side : {-1.f, 1.f}) {
    const B fore = side > 0 ? B::RightForearm : B::LeftForearm;
    const B hand = side > 0 ? B::RightHand : B::LeftHand;
    const Vec3 wrist{side * .185f, -.043f, .018f};
    std::map<std::pair<unsigned, unsigned>, unsigned> corners;
    std::vector<std::vector<unsigned>> groups(renn_hands::positions.size());
    const auto emit = [&](auto const &indices, auto const &uv_indices) {
      for (std::size_t t = 0; t < indices.size(); t += 3) {
        unsigned out[3];
        for (int k = 0; k < 3; ++k) {
          auto index = indices[t + k], uv = uv_indices[t + k];
          const bool hard =
              b.near
                  ? std::find(renn_hands::hard_normal_triangle_indices.begin(),
                              renn_hands::hard_normal_triangle_indices.end(),
                              t / 3) !=
                        renn_hands::hard_normal_triangle_indices.end()
                  : std::find(
                        renn_hands::far_hard_normal_triangle_indices.begin(),
                        renn_hands::far_hard_normal_triangle_indices.end(),
                        t / 3) !=
                        renn_hands::far_hard_normal_triangle_indices.end();
          auto key = std::make_pair(
              index,
              uv + (hard ? unsigned((t + 1) * renn_hands::uv_coordinates.size())
                         : 0));
          auto found = corners.find(key);
          if (found == corners.end()) {
            Vec3 p = renn_hand_offset(renn_hands::positions[index], side);
            p += wrist;
            const Vec2 tex = renn_hands::uv_coordinates[uv];
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
      emit(renn_hands::near_indices, renn_hands::near_uv_indices);
    else
      emit(renn_hands::far_indices, renn_hands::far_uv_indices);
    // Exactly reuse every anatomical wrist point at the terminal forearm ring.
    // The opposite terminal ring tucks inside the open rolled cuff, not a cap.
    auto boundary = renn_hands::wrist_boundary_indices;
    float area = 0;
    for (std::size_t i = 0; i < boundary.size(); ++i) {
      auto a = renn_hands::positions[boundary[i]],
           c = renn_hands::positions[boundary[(i + 1) % boundary.size()]];
      area += a.x * c.z - c.x * a.z;
    }
    if (area * side < 0)
      std::reverse(boundary.begin(), boundary.end());
    Vec3 center{};
    for (auto i : boundary)
      center += renn_hands::positions[i];
    center = center * (1.f / boundary.size());
    const unsigned start = b.m.bind_mesh.vertices.size();
    const int columns = boundary.size(), rows = b.near ? 4 : 2;
    b.grid(
        columns, rows,
        [&](float u, float v) {
          const auto index =
              boundary[std::size_t(std::lround(u * columns)) % boundary.size()];
          Vec3 source = renn_hand_offset(renn_hands::positions[index], side);
          source += wrist;
          const Vec3 local =
              renn_hand_offset(renn_hands::positions[index] - center, side);
          const float a = std::atan2(local.z, local.x);
          const float y = lerp(source.y, -.004f, v),
                      radius = .016f + .0130f * g(y, .078f, .071f);
          const float x = side * .185f;
          Vec3 target{x + radius * std::cos(a), y,
                      .018f + radius * .83f * std::sin(a)};
          Vec3 p = source * (1 - smooth(v / .28f)) + target * smooth(v / .28f);
          // Continue skin above the actual inner cuff return at -.008.
          // Its upper ring shares the cuff's rigid forearm transform; the
          // connected wrist transitions continuously from the source hand.
          CharacterInfluence w =
              wt(hand, fore, 1 - smooth((y + .043f) / .029f));
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
void make_renn_trousers(Sculpt &b) {
  // Original pair-of-pants topology. Each half of one waist loop joins one leg
  // through a shared curved crotch seam. There is no intersecting seat shell
  // or exposed horizontal thigh top under the tucked shirt.
  using renn_shirt_detail::Pattern;
  Pattern pattern{b, {}, {}, {}};
  const Vec3 cloth{.095f, .13f, .175f};
  const int half = b.near ? 16 : 8, rise = b.near ? 8 : 4;
  const int count = half + rise;
  const int hip_rows = b.near ? 8 : 3, leg_rows = b.near ? 20 : 8;
  const float rx = .096f, rz = .061f, top = .048f;
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
          const Vec3 end{sign * .059f + .043f * cs, -.105f, .047f * sn - .002f};
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
          const float near_y[]{-.105f, -.126f, -.148f, -.172f, -.188f, -.204f,
                               -.217f, -.225f, -.234f, -.249f, -.267f, -.287f,
                               -.313f, -.339f, -.365f, -.391f, -.398f, -.402f,
                               -.406f, -.409f, -.410f};
          const float far_y[]{-.105f, -.160f, -.195f, -.225f, -.265f,
                              -.325f, -.389f, -.404f, -.410f};
          const int row = j - hip_rows;
          const float y = b.near ? near_y[row] : far_y[row];
          float rad = .033f + .008f * smooth((y + .340f) / .240f);
          rad -= .003f * smooth((-.405f - y) / .054f);
          rad += .0012f * std::pow(std::max(0.f, sn), 14.f);
          rad += .0012f * g(y, -.224f, .027f) * std::sin(y * 220 + a) +
                 .0009f * g(y, -.443f, .023f) * std::sin(y * 310 + a * 2);
          // Match the first leg ring exactly to the last rise ring, then
          // transition smoothly to the independently shaped calf/knee.
          const float blend = smooth((-.105f - y) / .065f);
          const float rx = lerp(.043f, rad, blend),
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
  b.grid(
      b.near ? 48 : 16, 4,
      [&](float u, float v) {
        const float a = 2 * pi * u;
        const int row = int(std::lround(v * 4));
        const float yy[]{.047f, .059f, .059f, .047f, .047f};
        const float xx[]{.097f, .097f, .0956f, .0956f, .097f};
        const float zz[]{.061f, .061f, .0596f, .0596f, .061f};
        return P{{xx[row] * std::cos(a), yy[row], zz[row] * std::sin(a)},
                 {.075f, .080f, .078f},
                 wt(B::Pelvis),
                 character_surface_uv(S::Leather, u, v)};
      },
      true, false);
  b.ellipsoid({0, .0545f, .0623f}, {.009f, .0052f, .0014f}, {.55f, .56f, .51f},
              S::Metal, B::Pelvis, b.near ? 12 : 6, b.near ? 4 : 2);
  pattern.finish();
}

void make_renn_boots(Sculpt &b) {
  const Vec3 leather = b.m.appearance.shoes;
  for (float side : {-1.f, 1.f}) {
    const B foot = side < 0 ? B::LeftFoot : B::RightFoot;
    const B shin = side < 0 ? B::LeftShin : B::RightShin;
    const float x = side * .059f;
    auto shape = [](float v, const std::array<float, 6> &p) {
      int i = std::min(4, int(v * 5));
      return lerp(p[i], p[i + 1], smooth(v * 5 - i));
    };
    auto shaft_weight = [&](float y) {
      return wt(foot, shin, 1 - smooth((y + .455f) / .016f));
    };
    b.grid(b.near ? 20 : 12, b.near ? 3 : 2, [&](float u, float v) {
      const float a = 2 * pi * u,
                  edge = .0009f * std::sin(pi * v) + .0012f * g(v, .72f, .24f);
      const float z = .034f + (.072f + edge) * std::sin(a);
      const float arch_phase = std::clamp((z - .012f) / .045f, 0.f, 1.f);
      const float arch = .0056f * sq(std::sin(pi * arch_phase));
      // A shallow midfoot recess separates the heel and forefoot contact
      // pads. Both terminal pads retain the exact authored -.5 sole plane.
      return P{
          {x + (.0355f + edge) * std::cos(a), lerp(-.5f + arch, -.488f, v), z},
          Vec3{.049f, .055f, .061f} * (.90f + .12f * v),
          wt(foot),
          character_surface_uv(S::Rubber, u, v)};
    });
    const std::size_t begin = b.m.bind_mesh.indices.size();
    b.grid(b.near ? 20 : 10, b.near ? 8 : 4, [&](float u, float v) {
      const float a = 2 * pi * u,
                  y = lerp(-.488f, -.416f, v) + .011f * smooth((v - .6f) / .4f);
      return P{{x + shape(v, {.035f, .0355f, .033f, .027f, .025f, .0245f}) *
                        std::cos(a),
                y,
                shape(v, {.034f, .032f, .020f, .006f, .001f, 0.f}) +
                    shape(v, {.071f, .060f, .042f, .030f, .027f, .026f}) *
                        std::sin(a)},
               leather,
               shaft_weight(y),
               character_surface_uv(S::Leather, u, v)};
    });
    const std::size_t end = b.m.bind_mesh.indices.size();
    // The upper boot follows the shin where the trouser cuff lies over it.
    // Laces and tongue inherit the actual supporting triangle's blend, so a
    // rotating planted foot cannot pull them through the flexible instep.
    auto top = [&](float xx, float zz) {
      float best = -1;
      P result{};
      for (std::size_t t = begin; t < end; t += 3) {
        const auto ia = b.m.bind_mesh.indices[t],
                   ib = b.m.bind_mesh.indices[t + 1],
                   ic = b.m.bind_mesh.indices[t + 2];
        const Vec3 a = b.m.bind_mesh.vertices[ia].position * (1 / b.h),
                   e = b.m.bind_mesh.vertices[ib].position * (1 / b.h) - a,
                   f = b.m.bind_mesh.vertices[ic].position * (1 / b.h) - a;
        const float d = e.x * f.z - e.z * f.x;
        if (std::fabs(d) < 1e-12f)
          continue;
        const float u = ((xx - a.x) * f.z - (zz - a.z) * f.x) / d,
                    v = (e.x * (zz - a.z) - e.z * (xx - a.x)) / d;
        if (u < -1e-6f || v < -1e-6f || u + v > 1.000001f)
          continue;
        const float y = a.y + u * e.y + v * f.y;
        if (y <= best)
          continue;
        best = y;
        result = {
            {xx, y, zz},
            leather,
            renn_shirt_detail::blend_weights(
                {b.m.influences[ia], b.m.influences[ib], b.m.influences[ic]},
                {1 - u - v, u, v}),
            {}};
      }
      if (best < -.5f)
        throw std::runtime_error("Renn boot detail unsupported");
      return result;
    };
    if (b.near) {
      b.grid(
          4, 8,
          [&](float u, float v) {
            P p = top(x + lerp(-.013f, .013f, u), lerp(.030f, .074f, v));
            p.p.y += .0007f;
            p.c = leather * .92f;
            p.uv = character_surface_uv(S::Leather, u, v);
            return p;
          },
          false, false);
      for (int k = 0; k < 4; ++k) {
        std::vector<Vec3> lace;
        std::vector<CharacterInfluence> weights;
        const float z = .035f + .010f * k;
        for (int j = 0; j <= 4; ++j) {
          P p = top(x + lerp(-.012f, .012f, float(j) / 4), z);
          p.p.y += .0014f;
          lace.push_back(p.p);
          weights.push_back(p.w);
        }
        b.tube(
            lace, std::vector<float>(lace.size(), .00062f), 1,
            {.12f, .13f, .14f}, S::Shirt,
            [&](float t) {
              return weights[std::min(
                  weights.size() - 1,
                  std::size_t(std::lround(t * (weights.size() - 1))))];
            },
            6);
      }
    }
    b.grid(
        b.near ? 24 : 12, b.near ? 6 : 2,
        [&](float u, float v) {
          const float a = 2 * pi * u, y = lerp(-.475f, -.432f, v),
                      rx = lerp(.024f, .023f, v);
          return P{{x + rx * std::cos(a), y, .005f + rx * .96f * std::sin(a)},
                   {.065f, .071f, .077f},
                   shaft_weight(y),
                   character_surface_uv(S::Shirt, u, v)};
        },
        true, false);
  }
}

CharacterRig renn_rig(float height) {
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
  child(B::Chest, B::Spine, {0, .276f, 0});
  child(B::Neck, B::Chest, renn_anatomy::neck_center);
  child(B::Head, B::Neck, renn_anatomy::head_center);
  for (float s : {-1.f, 1.f}) {
    bool q = s > 0;
    B arm = q ? B::RightUpperArm : B::LeftUpperArm,
      fore = q ? B::RightForearm : B::LeftForearm,
      hand = q ? B::RightHand : B::LeftHand,
      thigh = q ? B::RightThigh : B::LeftThigh,
      shin = q ? B::RightShin : B::LeftShin,
      foot = q ? B::RightFoot : B::LeftFoot;
    child(arm, B::Chest, {s * .125f, .312f, 0});
    child(fore, arm, {s * .164f, .126f, .008f});
    child(hand, fore, {s * .185f, -.043f, .018f});
    child(thigh, B::Pelvis, {s * .059f, 0, 0});
    child(shin, thigh, {s * .059f, -.225f, 0});
    child(foot, shin, {s * .059f, -.46f, 0});
  }
  return r;
}
float neck_fit_radius(float y, float a, float z) {
  float q = std::clamp(y, .334f, .365f), r = anatomy_radius(q, a, z);
  if (y <= .365f)
    return r;
  return std::max(.012f, r + (r - anatomy_radius(.360f, a, z)) *
                                 ((y - .365f) / .005f));
}
} // namespace
CharacterProfile make_renn_character_profile(float height, CharacterRole role,
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
  m.rig = renn_rig(height);
  m.dual_quaternion_skinning = true;
  m.bind_mesh.vertices.reserve(lod == CharacterLod::Near ? 36000 : 17000);
  m.bind_mesh.indices.reserve(lod == CharacterLod::Near ? 150000 : 70000);
  m.influences.reserve(m.bind_mesh.vertices.capacity());
  Sculpt b{m, height, lod == CharacterLod::Near, {}, {}};
  auto shirt = make_renn_shirt(b, neck_fit_radius, renn_anatomy::neck_center);
  make_hands_and_forearms(b);
  make_renn_trousers(b);
  make_renn_boots(b);
  make_head(b);
  make_renn_cap(b, anatomy_radius);
  make_renn_vest(b, shirt);
  b.finish();
  p.material = make_character_surface_material();
  p.material.alpha_cutoff = .4f;
  p.lod_distance = 12;
  p.individualized = true;
  return p;
}
} // namespace fury
