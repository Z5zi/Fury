#include "character_lia_shirt.hpp"
#include "character_sculpt.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/lia_anatomy.hpp"
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
  for (std::size_t i = 0; i < lia_anatomy::indices.size(); i += 3) {
    const Vec3 a = lia_anatomy::positions[lia_anatomy::indices[i]];
    const Vec3 b = lia_anatomy::positions[lia_anatomy::indices[i + 1]];
    const Vec3 c = lia_anatomy::positions[lia_anatomy::indices[i + 2]];
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
  for (std::size_t i = 0; i < lia_anatomy::indices.size(); i += 3) {
    const Vec3 a = lia_anatomy::positions[lia_anatomy::indices[i]];
    const Vec3 b = lia_anatomy::positions[lia_anatomy::indices[i + 1]];
    const Vec3 c = lia_anatomy::positions[lia_anatomy::indices[i + 2]];
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
CharacterInfluence lia_neck_weight(float y) {
  return y < .365f ? wt(B::Neck, B::Chest, smooth((y - .333f) / .032f))
                   : wt(B::Head, B::Neck, smooth((y - .365f) / .022f));
}
void make_head(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> corners;
  std::vector<std::vector<std::uint32_t>> groups(lia_anatomy::positions.size());
  for (std::size_t triangle = 0; triangle < lia_anatomy::indices.size();
       triangle += 3) {
    std::uint32_t out[3];
    for (int corner = 0; corner < 3; ++corner) {
      const auto index = lia_anatomy::indices[triangle + corner];
      const auto uv_index = lia_anatomy::uv_indices[triangle + corner];
      const auto key = std::make_pair(index, uv_index);
      auto found = corners.find(key);
      if (found == corners.end()) {
        const Vec3 position = lia_anatomy::positions[index];
        const Vec2 uv = lia_anatomy::uv_coordinates[uv_index];
        const float lip = g(position.x, 0, .015f) *
                          g(position.y, .39006124f, .0035f) *
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
        const float lid = g(std::fabs(position.x), .0192f, .008f) *
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
            b.vertex({position, color, lia_neck_weight(position.y),
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
  auto boundary = lia_anatomy::attachment_boundary_indices;
  float area = 0;
  for (std::size_t i = 0; i < boundary.size(); ++i) {
    const Vec3 a = lia_anatomy::positions[boundary[i]];
    const Vec3 c = lia_anatomy::positions[boundary[(i + 1) % boundary.size()]];
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
        Vec3 point = lia_anatomy::positions[boundary[i]];
        point.y -= .015f * (1 - v);
        return P{point, skin, lia_neck_weight(point.y),
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
  for (std::size_t ei = 0; ei < lia_anatomy::eye_centers.size(); ++ei) {
    const Vec3 eye = lia_anatomy::eye_centers[ei] +
                     Vec3{ei == 0 ? .00065f : -.00065f, -.00042f, -.00167f};
    const Vec3 radius = lia_anatomy::eye_helper_half_extents[ei];
    b.ellipsoid(eye, radius, {.79f, .80f, .77f}, S::Eyes, B::Head,
                b.near ? 24 : 12, b.near ? 8 : 6);
    if (b.near) {
      std::vector<Vec3> upper_lid, lower_lid;
      const auto &rim = ei == 0 ? lia_anatomy::left_eye_opening
                                : lia_anatomy::right_eye_opening;
      for (Vec3 p : rim) {
        if (std::fabs(p.x - eye.x) > .0055f)
          continue;
        p.z += .00010f;
        (p.y >= .42825005f ? upper_lid : lower_lid).push_back(p);
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

void make_hands_and_legs(Sculpt &b) {
  auto skin = b.m.appearance.skin;
  for (float sign : {-1.f, 1.f}) {
    bool right = sign > 0;
    const auto hand_start = b.m.bind_mesh.vertices.size();
    B hand = right ? B::RightHand : B::LeftHand;
    const float wrist = sign * .170f;
    // Keep Lia's narrow palm outline and exact proximal cuff fit. The lower
    // palm tapers into a shallow knuckle arch instead of a thick planar end.
    // The next unchanged ring also preserves the proximal attachment normals.
    b.grid(b.near ? 20 : 8, b.near ? 8 : 3, [&](float u, float v) {
      const float angle = u == 1.f ? 0.f : 2 * pi * u,
                  lateral = sign * std::cos(angle),
                  distal = 1 - smooth(v * 3),
                  knuckle = .0025f * lateral * lateral + .0008f * lateral,
                  y = lerp(-.108f, -.036f, v) + knuckle * distal,
                  rx = lerp(.0185f, .015f, smooth((v - .5f) / .5f)),
                  rz = .011f + .002f * std::sin(pi * v) - .006f * distal,
                  sine = std::sin(angle),
                  flattened = std::copysign(std::pow(std::fabs(sine), .60f), sine);
      return P{{wrist + rx * std::cos(angle), y,
                rz * lerp(sine, flattened, distal) + .0195f},
               skin, wt(hand), character_surface_uv(S::Skin, u, v)};
    });
    const float offsets[4] = {-.014f, -.0045f, .005f, .014f},
                lengths[4] = {.041f, .047f, .043f, .033f},
                roots[4] = {-.098f, -.100f, -.0995f, -.097f},
                curl[4] = {.16f, .24f, .32f, .40f};
    for (int finger = 0; finger < 4; ++finger) {
      std::vector<Vec3> path;
      std::vector<float> radii;
      // Preserve authored flexion lengths, widths and fingertip fan. The
      // two Far roots sit 0.0009h inward for the coarse palm triangles;
      // their lateral paths open smoothly to the authored fingertip positions.
      const float root_inset = b.near ? 0.f :
          (finger == 0 ? -.0009f : finger == 3 ? .0009f : 0.f);
      auto center = [&](float t) {
        const float a = curl[finger] * t;
        return Vec3{wrist + sign * (offsets[finger] * (1 + .14f * t) -
                            root_inset * (1 - t)),
                    roots[finger] - lengths[finger] * std::sin(a) / curl[finger],
                    .020f - lengths[finger] * (1 - std::cos(a)) / curl[finger]};
      };
      int n = b.near ? 6 : 2;
      for (int i = 0; i <= n; ++i) {
        float t = float(i) / n;
        path.push_back(center(t));
        radii.push_back((.0043f - .0013f * t) *
                        (1 + .08f * std::sin(t * 4 * pi)));
      }
      b.tube(
          path, radii, .88f, skin, S::Skin, [&](float) { return wt(hand); },
          b.near ? 8 : 4);
      if (b.near) {
        const float t = 1.f - .006f / lengths[finger], a = curl[finger] * t;
        const Vec3 tangent = normalize(Vec3{sign * (offsets[finger] * .14f +
                       root_inset),
                       -lengths[finger] * std::cos(a),
                       -lengths[finger] * std::sin(a)}),
                   across = normalize(cross(Vec3{0, 0, 1}, tangent)),
                   dorsal = normalize(cross(tangent, across)),
                   p = center(t) + dorsal * .0028f;
        // Retain each nail's material, dimensions and topology while seating
        // its lower shell in the same digit's curved dorsal surface.
        b.grid(8, 4, [&](float u, float v) {
          const float angle = 2 * pi * u, e = lerp(-pi * .49f, pi * .49f, v);
          return P{p + across * (.0030f * std::cos(angle) * std::cos(e)) -
                           tangent * (.0055f * std::sin(e)) +
                           dorsal * (.00055f * std::sin(angle) * std::cos(e)),
                   skin * .90f + Vec3{.05f, .03f, .03f}, wt(hand),
                   character_surface_uv(S::Skin, u, v)};
        });
      }
    }
    std::vector<Vec3> thumb{{wrist - sign * .012f, -.063f, .020f},
                            {wrist - sign * .024f, -.073f, .026f}};
    // Relax the thumb's two distal segments without shortening either one.
    const Vec3 old_middle{-sign * .004f, -.016f, .004f},
               old_tip{-sign * .002f, -.011f, .003f};
    thumb.push_back(thumb.back() +
        normalize(Vec3{-sign * .0045f, -.016f, .0005f}) * length(old_middle));
    thumb.push_back(thumb.back() +
        normalize(Vec3{sign * .0005f, -.011f, -.003f}) * length(old_tip));
    for (auto &point : thumb)
      point.x += sign * .0002f;
    b.tube(
        thumb, {.007f, .006f, .0051f, .0039f}, .9f, skin, S::Skin,
        [&](float) { return wt(hand); }, b.near ? 8 : 4);
    // Lia's forearm is independently fitted to the service-shirt proportions.
    // The 0.038-height wrist rise preserves palm/finger shape and thumb roots.
    for (std::size_t i = hand_start; i < b.m.bind_mesh.vertices.size(); ++i)
      b.m.bind_mesh.vertices[i].position.y += .038f * b.h;
    B foot = right ? B::RightFoot : B::LeftFoot;
    float x = sign * .062f;
    // Sole and toe box share the animator's exact supported heel/toe envelope.
    b.grid(b.near ? 28 : 14, 3, [&](float u, float v) {
      float a = 2 * pi * u, y = lerp(-.5f, -.483f, v);
      return P{{x + .032f * std::cos(a), y, .033f + .067f * std::sin(a)},
               {.07f, .072f, .068f},
               wt(foot),
               character_surface_uv(S::Rubber, u, v)};
    });
    b.grid(b.near ? 32 : 10, b.near ? 10 : 3, [&](float u, float v) {
      float a = 2 * pi * u, y = lerp(-.483f, -.430f, v),
            rx = lerp(.0315f, .022f, smooth(v)),
            rz = lerp(.066f, .027f, smooth(v)),
            cz = lerp(.033f, 0.f, smooth(v));
      return P{{x + rx * std::cos(a), y, cz + rz * std::sin(a)},
               b.m.appearance.shoes,
               wt(foot),
               character_surface_uv(S::Leather, u, v)};
    });
    for (int lace = 0; lace < (b.near ? 5 : 2); ++lace) {
      float z = .035f + lace * .006f, y = -.445f - lace * .004f;
      std::vector<Vec3> p{
          {x - .012f, y, z}, {x, y + .0018f, z + .002f}, {x + .012f, y, z}};
      b.curve(p, .00085f, {.39f, .37f, .30f}, S::Shirt, foot, b.near ? 6 : 4);
    }
  }
}

void make_lia_trousers(Sculpt &b) {
  // Original pair-of-pants topology. Each half of one waist loop joins one leg
  // through a shared curved crotch seam. There is no intersecting seat shell
  // or exposed horizontal thigh top under the tucked shirt.
  using lia_shirt_detail::Pattern;
  Pattern pattern{b, {}, {}, {}};
  const Vec3 cloth = b.m.appearance.trousers;
  const int half = b.near ? 24 : 10, rise = b.near ? 12 : 5;
  const int count = half + rise;
  const int hip_rows = b.near ? 9 : 4, leg_rows = b.near ? 25 : 10;
  const float rx = .095f, rz = .059f, top = .048f;
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
      phase[i] = std::atan2(p.z, p.x - sign * .062f);
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
          const Vec3 end{sign * .062f + .043f * cs, -.105f, .047f * sn - .002f};
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
                               -.350f, -.370f, -.391f, -.412f, -.431f, -.446f,
                               -.453f, -.459f};
          const float far_y[]{-.105f, -.150f, -.190f, -.225f, -.260f, -.300f,
                              -.350f, -.400f, -.435f, -.450f, -.459f};
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
          p = {sign * .062f + rx * cs, y,
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
        const float yy[]{.047f, .062f, .062f, .047f, .047f};
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

// Lia owns the swept crown construction below; it follows her measured skull.

float lia_hairline(float a) {
  const float front = std::max(0.f, std::sin(a));
  const float back = std::max(0.f, -std::sin(a));
  const float temple = std::pow(std::fabs(std::cos(a)), 10.f);
  // A swept, asymmetric forehead edge, close temples and a complete nape.
  const float wrapped = std::atan2(std::sin(a), std::cos(a));
  const float sideburn =
      .009f * (g(wrapped, .38f, .15f) + g(wrapped, pi - .38f, .15f));
  return .438f + .022f * front - .027f * back + .007f * temple - sideburn +
         .0028f * std::cos(a) * front + .00055f * std::sin(a * 9 + .3f);
}
Vec3 lia_sweep(float a, float t, float offset = 0.f) {
  const float v = std::clamp(t, 0.f, 1.f);
  // Follow the measured skull all the way to its crown, then lift hair along
  // the growth volume. Repeating one high section above .4875 made a dome.
  const float skin_y =
      lerp(lia_hairline(a) - .0012f, .493f, std::sin(v * pi * .5f));
  const float y = skin_y + .007f * smooth(v);
  const float cz =
      lerp(lia_anatomy::head_center.z, lia_anatomy::skin_crown_center.z,
           smooth((skin_y - .454f) / .039f));
  const float front = std::max(0.f, std::sin(a));
  const float sweep = .5f + .5f * std::cos(a - .8f);
  float radius = anatomy_radius(std::min(skin_y, .49295f), a, cz);
  radius += (.0012f + .0026f * front * sweep * std::sin(pi * v) +
             .0010f * std::sin(pi * v) + offset) *
            std::cos(v * pi * .5f);
  radius = std::max(.00020f, radius);
  return {radius * std::cos(a) + .0025f * std::sin(pi * v) * front,
          std::min(.500f, y), cz + radius * std::sin(a)};
}
void make_lia_hair(Sculpt &b) {
  const Vec3 hair = b.m.appearance.hair;
  const int sectors = 8, columns = b.near ? 8 : 3, rows = b.near ? 15 : 5;
  std::array<std::uint32_t, sectors> starts{};
  for (int sector = 0; sector < sectors; ++sector) {
    starts[sector] = std::uint32_t(b.m.bind_mesh.vertices.size());
    b.grid(
        columns, rows,
        [&](float u, float v) {
          const float a = 2 * pi * (float(sector) + u) / sectors;
          const float t = .025f + .975f * v;
          const float root = .75f + .10f * smooth(t / .4f);
          return P{lia_sweep(a, t, b.near ? 0.f : .0014f), hair * root,
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
  const auto pole = b.vertex({{0, .500f, lia_anatomy::skin_crown_center.z},
                              hair * .84f,
                              wt(B::Head),
                              character_surface_uv(S::HairCrop, .5f, 1.f)});
  for (int sector = 0; sector < sectors; ++sector)
    for (int col = 0; col < columns; ++col) {
      const auto i = starts[sector] + std::uint32_t(rows * (columns + 1) + col);
      b.tri(i, pole, i + 1);
    }
  // Short temple/nape fibers soften the hairline. Longer crown cards run from
  // the left part across the front mass; all roots remain inside the cap.
  const int edge_cards = b.near ? 80 : 10;
  const int crown_cards = b.near ? 16 : 4;
  for (int k = 0; k < edge_cards + crown_cards; ++k) {
    const bool crown = k >= edge_cards;
    const float f = std::fmod(float(k) * .6180339887f, 1.f);
    const float a0 = crown ? 1.84f + .68f * f : float(k) * 2.39996323f;
    const float start = crown ? .72f + .22f * f : .18f + .04f * f;
    const float finish = crown ? .025f + .27f * f : -.017f - .039f * f;
    const float width =
        crown ? (b.near ? .095f : .20f) : (b.near ? .073f : .22f);
    if (crown) {
      const int rows = b.near ? 16 : 6;
      std::vector<Vec3> path(rows + 1), normal(rows + 1), across(rows + 1);
      for (int j = 0; j <= rows; ++j) {
        const float v = float(j) / rows, t = lerp(start, finish, v);
        const float a = lerp(a0, -.12f + 1.56f * f, v);
        const float lift =
            (b.near ? 0.f : .0014f) - .00015f + .0017f * std::sin(pi * v);
        path[j] = lia_sweep(a, t, lift);
        const Vec3 da = lia_sweep(a + .0005f, t) - lia_sweep(a - .0005f, t);
        const Vec3 dt = lia_sweep(a, std::min(.999f, t + .0005f)) -
                        lia_sweep(a, std::max(0.f, t - .0005f));
        normal[j] = normalize(cross(normalize(dt), normalize(da)));
      }
      for (int j = 0; j <= rows; ++j) {
        const Vec3 tangent =
            normalize(path[std::min(rows, j + 1)] - path[std::max(0, j - 1)]);
        across[j] = normalize(cross(normal[j], tangent));
      }
      b.grid(
          2, rows,
          [&](float u, float v) {
            const int j = std::min(rows, int(std::lround(v * rows)));
            const float taper = .32f + .68f * std::sin(pi * (.08f + .87f * v));
            const float half = (b.near ? .0021f : .0036f) * taper;
            const Vec3 p = path[j] + across[j] * (half * (2 * u - 1)) -
                           normal[j] * (.00015f * sq(2 * u - 1));
            return P{p, hair * (.76f + .09f * smooth(v / .35f)), wt(B::Head),
                     character_surface_uv(S::HairShortStrands, u, v)};
          },
          false, false);
      continue;
    }
    b.grid(
        2, b.near ? 6 : 3,
        [&](float u, float v) {
          const float t = lerp(start, finish, v);
          const float a =
              (crown ? lerp(a0, -.12f + 1.56f * f, v) : a0 + .10f * v) +
              (.5f - u) * width;
          // Millimetre-scale lift with roots buried; large ridges cast false
          // pore-like shadows.
          const float lift =
              -.00015f + (crown ? .0022f : .00055f) * std::sin(pi * v);
          Vec3 p = lia_sweep(a, t, lift);
          if (t < 0)
            p.y += .064f * t;
          return P{p, hair * (.74f + .11f * smooth(v / .35f)), wt(B::Head),
                   character_surface_uv(S::HairShortStrands, u, v)};
        },
        false, false);
  }
}

CharacterRig lia_rig(float height) {
  CharacterRig rig;
  rig.height = height;
  auto joint = [&](B bone, int parent, Vec3 p) {
    rig.joints[bone_index(bone)] = {parent, p * height};
  };
  auto child = [&](B bone, B parent, Vec3 p) {
    joint(bone, int(bone_index(parent)), p);
  };
  joint(B::Pelvis, -1, {0, 0, 0});
  child(B::Spine, B::Pelvis, {0, .13f, 0});
  child(B::Chest, B::Spine, {0, .272f, 0});
  child(B::Neck, B::Chest, lia_anatomy::neck_center);
  child(B::Head, B::Neck, lia_anatomy::head_center);
  for (float sign : {-1.f, 1.f}) {
    const bool right = sign > 0;
    const B arm = right ? B::RightUpperArm : B::LeftUpperArm;
    const B fore = right ? B::RightForearm : B::LeftForearm;
    const B hand = right ? B::RightHand : B::LeftHand;
    const B thigh = right ? B::RightThigh : B::LeftThigh;
    const B shin = right ? B::RightShin : B::LeftShin;
    const B foot = right ? B::RightFoot : B::LeftFoot;
    child(arm, B::Chest, {sign * .125f, .310f, 0});
    child(fore, arm, {sign * .145f, .143f, .008f});
    child(hand, fore, {sign * .170f, -.006f, .018f});
    child(thigh, B::Pelvis, {sign * .062f, 0, 0});
    child(shin, thigh, {sign * .062f, -.225f, 0});
    child(foot, shin, {sign * .062f, -.46f, 0});
  }
  return rig;
}

float neck_fit_radius(float y, float angle, float center_z) {
  // Lia has complete neck-chart contours through .365. Fit below her jaw,
  // using measured skin at each requested height rather than Owen's collar.
  const float low = std::clamp(y, .334f, .365f);
  const float r = anatomy_radius(low, angle, center_z);
  if (y <= .365f)
    return r;
  const float prior = anatomy_radius(.360f, angle, center_z);
  return std::max(.012f, r + (r - prior) * ((y - .365f) / .005f));
}
} // namespace

CharacterProfile make_lia_character_profile(float height, CharacterRole role,
                                            std::string_view identity,
                                            CharacterLod lod) {
  if (!std::isfinite(height) || height <= 0.f ||
      (lod != CharacterLod::Near && lod != CharacterLod::Far))
    throw std::invalid_argument("Invalid individual character request");
  CharacterProfile profile;
  auto &model = profile.model;
  model.role = role;
  model.lod = lod;
  model.seed = character_seed(identity);
  model.appearance = character_appearance(model.seed);
  model.rig = lia_rig(height);
  model.dual_quaternion_skinning = true;
  model.bind_mesh.vertices.reserve(lod == CharacterLod::Near ? 32000 : 12000);
  model.bind_mesh.indices.reserve(lod == CharacterLod::Near ? 140000 : 60000);
  model.influences.reserve(model.bind_mesh.vertices.capacity());
  Sculpt sculpt{model, height, lod == CharacterLod::Near, {}, {}};
  make_lia_shirt(sculpt, neck_fit_radius, lia_anatomy::neck_center);
  make_lia_trousers(sculpt);
  make_hands_and_legs(sculpt);
  make_head(sculpt);
  make_lia_hair(sculpt);
  sculpt.finish();
  profile.material = make_character_surface_material();
  profile.material.alpha_cutoff = .4f;
  profile.lod_distance = 12.f;
  profile.individualized = true;
  return profile;
}
} // namespace fury
