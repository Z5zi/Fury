#include "character_owen_jacket.hpp"
#include "character_sculpt.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/owen_anatomy.hpp"
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
  for (std::size_t i = 0; i < owen_anatomy::indices.size(); i += 3) {
    const Vec3 a = owen_anatomy::positions[owen_anatomy::indices[i]];
    const Vec3 b = owen_anatomy::positions[owen_anatomy::indices[i + 1]];
    const Vec3 c = owen_anatomy::positions[owen_anatomy::indices[i + 2]];
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
  for (std::size_t i = 0; i < owen_anatomy::indices.size(); i += 3) {
    const Vec3 a = owen_anatomy::positions[owen_anatomy::indices[i]];
    const Vec3 b = owen_anatomy::positions[owen_anatomy::indices[i + 1]];
    const Vec3 c = owen_anatomy::positions[owen_anatomy::indices[i + 2]];
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
CharacterInfluence owen_neck_weight(float y) {
  return y < .360f ? wt(B::Neck, B::Chest, smooth((y - .337f) / .023f))
                   : wt(B::Head, B::Neck, smooth((y - .360f) / .024f));
}
void make_head(Sculpt &b) {
  const Vec3 skin = b.m.appearance.skin;
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> corners;
  std::vector<std::vector<std::uint32_t>> groups(
      owen_anatomy::positions.size());
  for (std::size_t triangle = 0; triangle < owen_anatomy::indices.size();
       triangle += 3) {
    std::uint32_t out[3];
    for (int corner = 0; corner < 3; ++corner) {
      const auto index = owen_anatomy::indices[triangle + corner];
      const auto uv_index = owen_anatomy::uv_indices[triangle + corner];
      const auto key = std::make_pair(index, uv_index);
      auto found = corners.find(key);
      if (found == corners.end()) {
        const Vec3 position = owen_anatomy::positions[index];
        const Vec2 uv = owen_anatomy::uv_coordinates[uv_index];
        const float lip = g(position.x, 0, .015f) *
                          g(position.y, .38657f, .0035f) *
                          smooth((position.z - .079f) / .010f);
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
            b.vertex({position, color, owen_neck_weight(position.y),
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
  auto boundary = owen_anatomy::attachment_boundary_indices;
  float area = 0;
  for (std::size_t i = 0; i < boundary.size(); ++i) {
    const Vec3 a = owen_anatomy::positions[boundary[i]];
    const Vec3 c = owen_anatomy::positions[boundary[(i + 1) % boundary.size()]];
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
        Vec3 point = owen_anatomy::positions[boundary[i]];
        point.y -= .015f * (1 - v);
        return P{point, skin, owen_neck_weight(point.y),
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
  for (std::size_t ei = 0; ei < owen_anatomy::eye_centers.size(); ++ei) {
    const Vec3 eye = owen_anatomy::eye_centers[ei] +
                     Vec3{ei == 0 ? .0010f : -.0010f, -.0011f, -.00155f};
    const Vec3 radius = owen_anatomy::eye_helper_half_extents[ei];
    b.ellipsoid(eye, radius, {.79f, .80f, .77f}, S::Eyes, B::Head,
                b.near ? 24 : 12, b.near ? 8 : 6);
    if (b.near) {
      std::vector<Vec3> upper_lid, lower_lid;
      const auto &rim = ei == 0 ? owen_anatomy::left_eye_opening
                                : owen_anatomy::right_eye_opening;
      for (Vec3 p : rim) {
        if (std::fabs(p.x - eye.x) > .0055f)
          continue;
        p.z += .00010f;
        (p.y >= .42765f ? upper_lid : lower_lid).push_back(p);
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

void make_hands_and_legs(Sculpt &b) {
  auto skin = b.m.appearance.skin;
  Vec3 pants = b.m.appearance.trousers;
  // One sewn garment, retaining Owen's authored knee/calf rings exactly.
  const int columns = b.near ? 22 : 10;
  const int lower_rows = b.near ? 21 : 9;
  const int upper_rows = b.near ? 14 : 8;
  const int rows = lower_rows + upper_rows;
  std::array<std::vector<std::uint32_t>, 2> tops;
  for (float sign : {-1.f, 1.f}) {
    bool right = sign > 0;
    B hand = right ? B::RightHand : B::LeftHand;
    const float wrist = sign * .170f;
    // The distal palm narrows in depth toward the knuckles. The proximal
    // two rings of each LOD retain their exact wrist/cuff geometry and normals.
    b.grid(b.near ? 20 : 8, b.near ? 8 : 3, [&](float u, float v) {
      const float angle = u == 1.f ? 0.f : 2 * pi * u, lateral = sign * std::cos(angle),
                  distal = 1 - smooth(v * 3),
                  arc = .003f * lateral * lateral + .001f * lateral,
                  y = lerp(-.108f, -.036f, v) + arc * distal,
                  rx = lerp(.020f, .0155f, smooth((v - .5f) / .5f)),
                  rz = .011f + .002f * std::sin(pi * v) - .0055f * distal,
                  sine = std::sin(angle),
                  broad = std::copysign(std::pow(std::fabs(sine), .55f), sine),
                  cross_section = lerp(sine, broad, distal);
      return P{{wrist + rx * std::cos(angle), y, rz * cross_section + .0195f},
               skin, wt(hand), character_surface_uv(S::Skin, u, v)};
    });
    const float offsets[4] = {-.014f, -.0045f, .005f, .014f},
                lengths[4] = {.043f, .050f, .046f, .035f},
                roots[4] = {-.099f, -.100f, -.0995f, -.0975f},
                curl[4] = {.20f, .30f, .40f, .48f};
    for (int finger = 0; finger < 4; ++finger) {
      std::vector<Vec3> path;
      std::vector<float> radii;
      // Arc length in the flexion plane stays at the authored digit length.
      // The small original lateral fan and each original radius are retained.
      auto center = [&](float t) {
        const float angle = curl[finger] * t;
        return Vec3{wrist + sign * offsets[finger] * (1 + .14f * t),
                    roots[finger] - lengths[finger] * std::sin(angle) / curl[finger],
                    .020f - lengths[finger] * (1 - std::cos(angle)) / curl[finger]};
      };
      int n = b.near ? 6 : 2;
      for (int i = 0; i <= n; ++i) {
        float t = float(i) / n;
        path.push_back(center(t));
        radii.push_back((.0045f - .0014f * t) *
                        (1 + .08f * std::sin(t * 4 * pi)));
      }
      b.tube(
          path, radii, .88f, skin, S::Skin, [&](float) { return wt(hand); },
          b.near ? 8 : 4);
      if (b.near) {
        // The original nail shell follows the distal tangent and dorsal face.
        const float t = 1.f - .006f / lengths[finger], angle = curl[finger] * t;
        const Vec3 tangent = normalize(Vec3{sign * offsets[finger] * .14f,
            -lengths[finger] * std::cos(angle), -lengths[finger] * std::sin(angle)}),
            across = normalize(cross(Vec3{0, 0, 1}, tangent)),
            dorsal = normalize(cross(tangent, across)),
            p = center(t) + dorsal * .0029f;
        b.grid(8, 4, [&](float u, float v) {
          const float a = 2 * pi * u, e = lerp(-pi * .49f, pi * .49f, v);
          return P{p + across * (.0030f * std::cos(a) * std::cos(e)) -
                           tangent * (.0055f * std::sin(e)) +
                           dorsal * (.00055f * std::sin(a) * std::cos(e)),
                   skin * .90f + Vec3{.05f, .03f, .03f}, wt(hand),
                   character_surface_uv(S::Skin, u, v)};
        });
      }
    }
    std::vector<Vec3> thumb{{wrist - sign * .012f, -.063f, .020f},
                            {wrist - sign * .024f, -.073f, .026f}};
    // Relax the last two thumb segments without shortening either segment.
    const Vec3 old_middle{-sign * .004f, -.016f, .004f},
               old_tip{-sign * .002f, -.011f, .003f};
    thumb.push_back(thumb.back() +
        normalize(Vec3{-sign * .005f, -.016f, -.001f}) * length(old_middle));
    thumb.push_back(thumb.back() +
        normalize(Vec3{sign * .001f, -.011f, -.004f}) * length(old_tip));
    b.tube(
        thumb, {.007f, .006f, .0051f, .0039f}, .9f, skin, S::Skin,
        [&](float) { return wt(hand); }, b.near ? 8 : 4);
    B thigh = right ? B::RightThigh : B::LeftThigh,
      shin = right ? B::RightShin : B::LeftShin,
      foot = right ? B::RightFoot : B::LeftFoot;
    float x = sign * .059f;
    // Retain leg topology cost while placing rings at the anatomical knee.
    const float near_y[]{-.459f, -.443f, -.425f, -.400f, -.375f, -.350f, -.325f,
                         -.300f, -.285f, -.275f, -.265f, -.255f, -.245f, -.235f,
                         -.225f, -.215f, -.205f, -.195f, -.185f, -.175f, -.160f,
                         -.140f, -.115f, -.090f, -.060f, -.030f, .007f};
    const float far_y[]{-.459f, -.420f, -.370f, -.320f, -.275f, -.250f, -.225f,
                        -.200f, -.175f, -.130f, -.080f, -.035f, .007f};
    const float join_y = b.near ? -.140f : -.130f;
    auto lower_point = [&](float u, float y) {
      float angle = 2 * pi * u;
      float radius = .0265f + .010f * g(y, -.34f, .08f) +
                     .020f * smooth((y + .225f) / .22f);
      radius += .0017f * g(y, -.224f, .027f) * std::sin(y * 220 + angle) +
                .0013f * g(y, -.443f, .023f) * std::sin(y * 310 + angle * 2.f);
      // The anterior knee bends locally around the patella. Posterior
      // fabric distributes the compression without hollowing the bind calf.
      const float knee_span = .100f + .120f * std::max(0.f, -std::sin(angle));
      const float knee_low = -.225f - .5f * knee_span;
      const float knee_high = -.225f + .5f * knee_span;
      auto weight =
          y < knee_low ? wt(shin)
          : y < knee_high
              ? wt(shin, thigh, 1 - smooth((y - knee_low) / knee_span))
              : wt(thigh, B::Pelvis, smooth((.045f - y) / .10f));
      return P{
          {x + radius * std::cos(angle), y,
           radius * 1.04f * std::sin(angle) - .001f +
               .013f * g(y, -.225f, .20f) * std::max(0.f, -std::sin(angle))},
          pants,
          weight,
          character_surface_uv(S::Pants, u, (y + .459f) / .466f)};
    };
    const auto base = std::uint32_t(b.m.bind_mesh.vertices.size());
    auto point = [&](int i, int j) {
      const float u = float(i) / columns;
      if (j <= lower_rows)
        return lower_point(u, b.near ? near_y[j] : far_y[j]);
      const float t = float(j - lower_rows) / upper_rows;
      // Owen's 22/10-sided lower rings are kept, including all UV corners.
      // A half-sample phase at the upper edge supplies exact front/rear
      // saddle endpoints without changing that authored lower topology.
      const int canonical = right ? i % columns
          : (columns / 2 - i + columns) % columns;
      const float a = 2 * pi * (float(canonical) + .5f) / columns;
      float cs = std::cos(a), sn = std::sin(a);
      if (std::fabs(cs) < 1e-6f) cs = 0;
      if (std::fabs(sn) < 1e-6f) sn = 0;
      const float inward = std::max(0.f, -cs);
      const Vec3 top{sign * .090f * std::max(0.f, cs),
                     .058f - .128f * inward,
                     .054f * sn - .002f};
      P p = lower_point(u, join_y);
      const auto original = p.w;
      p.p = p.p * (1 - t) + top * t;
      // Keep Owen's small posterior shin blend continuous at the retained
      // ring. It fades with the thigh contribution into the pelvis saddle.
      const float leg = std::min(1 - smooth(t),
          1 - smooth((p.p.y - join_y) / (-.010f - join_y)));
      auto component = [&](B bone) {
        const auto k = std::uint8_t(bone);
        return (original.first == k ? original.first_weight : 0.f) +
               (original.second == k ? 1 - original.first_weight - original.third_weight : 0.f) +
               (original.third == k ? original.third_weight : 0.f);
      };
      // A sewn crotch follows both femoral roots during bilateral flexion.
      // Symmetric weights keep both seam copies identical while preventing
      // the common saddle from pinning the fabric to the pelvis in a crouch.
      const float saddle = .30f * smooth((-.010f - top.y) / .060f);
      const float shared = saddle * smooth((t - .25f) / .75f);
      const float shin_fade = smooth(t / .25f);
      const float own = (component(thigh) + component(shin) * shin_fade) * leg;
      const float knee = component(shin) * (1 - shin_fade) * leg;
      if (p.p.y >= -.010f)
        p.w = owen_jacket_detail::body_weight(p.p.y);
      else if (knee > 0.f)
        p.w = {std::uint8_t(thigh), std::uint8_t(B::Pelvis), own,
               std::uint8_t(shin), knee};
      else
        p.w = {std::uint8_t(B::LeftThigh), std::uint8_t(B::Pelvis),
               shared + (right ? 0.f : own), std::uint8_t(B::RightThigh),
               shared + (right ? own : 0.f)};
      p.uv = character_surface_uv(S::Pants, u, (p.p.y + .459f) / .517f);
      if (j == rows) {
        p.p = top;
        p.w = top.y >= -.010f ? owen_jacket_detail::body_weight(top.y)
            : CharacterInfluence{std::uint8_t(B::LeftThigh),
                                 std::uint8_t(B::Pelvis), saddle,
                                 std::uint8_t(B::RightThigh), saddle};
      }
      return p;
    };
    for (int j = 0; j <= rows; ++j) {
      for (int i = 0; i <= columns; ++i) {
        P p = point(i, j);
        if (i == columns) {
          const P first = point(0, j);
          p.p = first.p; p.c = first.c; p.w = first.w;
        }
        b.vertex(p);
      }
      if (j != rows)
        b.smooth_seams.push_back({base + j * (columns + 1),
                                 base + j * (columns + 1) + columns});
    }
    for (int j = 0; j < rows; ++j)
      for (int i = 0; i < columns; ++i) {
        const auto a = base + j * (columns + 1) + i;
        const auto next = a + columns + 1;
        b.tri(a, next, next + 1); b.tri(a, next + 1, a + 1);
      }
    // The exact original ankle cap remains. Upper leg openings are sewn.
    P center = lower_point(0, -.459f); center.p = {};
    for (int i = 0; i < columns; ++i)
      center.p += lower_point(float(i) / columns, -.459f).p;
    center.p = center.p * (1.f / float(columns));
    const auto cap_center = b.vertex(center);
    const auto cap_ring = std::uint32_t(b.m.bind_mesh.vertices.size());
    for (int i = 0; i <= columns; ++i) {
      P p = lower_point(float(i) / columns, -.459f);
      if (i == columns) {
        const P first = lower_point(0, -.459f);
        p.p = first.p; p.c = first.c; p.w = first.w;
      }
      b.vertex(p);
    }
    for (int i = 0; i < columns; ++i)
      b.tri(cap_center, cap_ring + i, cap_ring + i + 1);
    auto &top = tops[right ? 1 : 0];
    for (int i = 0; i <= columns; ++i)
      top.push_back(base + rows * (columns + 1) + i);
    // Sole and toe box share the animator's exact supported heel/toe envelope.
    b.grid(b.near ? 28 : 14, 3, [&](float u, float v) {
      float a = 2 * pi * u, y = lerp(-.5f, -.483f, v);
      return P{{x + .032f * std::cos(a), y, .033f + .067f * std::sin(a)},
               {.07f, .072f, .068f},
               wt(foot),
               character_surface_uv(S::Rubber, u, v)};
    });
    b.grid(b.near ? 32 : 12, b.near ? 10 : 4, [&](float u, float v) {
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
  // Weld only physical saddle and texture-seam copies, each exactly once.
  // Their coordinates and influences share one canonical angular sample.
  std::map<std::tuple<float, float, float>, std::vector<std::uint32_t>> groups;
  std::map<std::tuple<float, float, float>, std::uint32_t> rim;
  for (const auto &top : tops)
    for (auto index : top) {
      const auto p = b.m.bind_mesh.vertices[index].position;
      const auto key = std::make_tuple(p.x, p.y, p.z);
      groups[key].push_back(index);
      if (std::fabs(p.y - .058f * b.h) < 1e-7f) rim.emplace(key, index);
    }
  for (auto &entry : groups)
    if (entry.second.size() > 1)
      b.normal_groups.push_back(std::move(entry.second));
  // Close the single waist inside the existing jacket. Exact coordinate
  // copies avoid divide/multiply drift at non-unit actor heights.
  std::vector<std::uint32_t> waist;
  for (const auto &entry : rim) waist.push_back(entry.second);
  std::sort(waist.begin(), waist.end(), [&](auto a, auto c) {
    const auto x = b.m.bind_mesh.vertices[a].position;
    const auto z = b.m.bind_mesh.vertices[c].position;
    return std::atan2(x.z + .002f * b.h, x.x) <
           std::atan2(z.z + .002f * b.h, z.x);
  });
  const auto center = b.vertex({{0, .058f, -.002f}, pants,
      owen_jacket_detail::body_weight(.058f), character_surface_uv(S::Pants, .5f, 1.f)});
  for (auto &index : waist) {
    const auto v = b.m.bind_mesh.vertices[index];
    const auto copy = b.vertex({{}, pants, b.m.influences[index], v.uv});
    b.m.bind_mesh.vertices[copy].position = v.position;
    index = copy;
  }
  for (std::size_t i = 0; i < waist.size(); ++i)
    b.tri(center, waist[(i + 1) % waist.size()], waist[i]);
}

float owen_hairline(float angle) {
  const float front = std::max(0.f, std::sin(angle));
  const float rear = std::max(0.f, -std::sin(angle));
  // Short sideburns and a low, complete nape; an irregular boundary follows
  // the skull rather than exposing the generic model's rectangular rear gap.
  return .438f + .020f * front - .034f * rear +
         .008f * std::pow(std::fabs(std::cos(angle)), 8.f) +
         .0007f * std::sin(angle * 7 + .4f) + .0004f * std::sin(angle * 13);
}
Vec3 owen_crop(float angle, float t, float lift = 0.f) {
  const float y = lerp(owen_hairline(angle), .500f,
                       std::sin(std::clamp(t, 0.f, 1.f) * pi * .5f));
  const float cz =
      lerp(owen_anatomy::head_center.z, owen_anatomy::crown_joint.z,
           smooth((y - .455f) / .038f));
  const float side_length = .0017f + .0013f * smooth(t / .30f);
  float radius = anatomy_radius(std::min(y, .4885f), angle, cz) + side_length;
  // Very shallow asymmetrical growth, not radial ridges or a sculpted quiff.
  radius += .0007f * std::sin(angle * 3 - t * 4) * std::sin(pi * t) + lift;
  if (y > .4885f)
    radius *= std::sqrt(std::max(0.f, 1 - sq((y - .4885f) / .0115f)));
  radius = std::max(.00025f, radius);
  return {radius * std::cos(angle), y, cz + radius * std::sin(angle)};
}
void make_owen_hair(Sculpt &b) {
  const Vec3 auburn = b.m.appearance.hair;
  const int sectors = 8, columns = b.near ? 8 : 3, rows = b.near ? 18 : 5;
  // The coarse Far chords otherwise cut through the convex source scalp,
  // including the lower rear seams. Clearance stays radial and tapers with
  // the existing crown profile, preserving the exact height and topology.
  const float cap_clearance = b.near ? 0.f : .003f;
  std::array<std::uint32_t, sectors> starts{};
  for (int sector = 0; sector < sectors; ++sector) {
    starts[sector] = std::uint32_t(b.m.bind_mesh.vertices.size());
    b.grid(
        columns, rows,
        [&](float u, float v) {
          const float a = 2 * pi * (float(sector) + u) / sectors;
          const float t = .038f + .962f * v;
          const float root = .69f + .11f * smooth(t / .35f);
          return P{owen_crop(a, t, cap_clearance), auburn * root, wt(B::Head),
                   character_surface_uv(S::HairCrop, u, v)};
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
  const auto pole = b.vertex({{0, .500f, owen_anatomy::crown_joint.z},
                              auburn * .80f,
                              wt(B::Head),
                              character_surface_uv(S::HairCrop, .5f, 1.f)});
  for (int sector = 0; sector < sectors; ++sector)
    for (int col = 0; col < columns; ++col) {
      const auto a = starts[sector] + std::uint32_t(rows * (columns + 1) + col);
      b.tri(a, pole, a + 1);
    }
  // Short growth follows a restrained offset whorl. Low-amplitude overlapping
  // cards soften silhouette and temple/nape transitions without bald openings.
  const int strands = b.near ? 184 : 24;
  for (int k = 0; k < strands; ++k) {
    const float fraction = std::fmod(float(k) * .6180339887f, 1.f);
    const float a0 = float(k) * 2.39996323f;
    const float start =
        b.near ? (k < 96 ? .19f : .38f + .55f * fraction) : .24f;
    const float finish =
        start < .39f ? -.026f - .039f * (.5f + .5f * std::sin(float(k) * 1.91f))
                     : start - .23f;
    const float width =
        (b.near ? .066f : .21f) * (.87f + .12f * std::sin(float(k) * 1.37f));
    b.grid(
        2, b.near ? 5 : 3,
        [&](float u, float v) {
          const float t = lerp(start, finish, v);
          const float swirl = .085f * std::sin(pi * v) * smooth(start / .65f);
          const float a = a0 + swirl + (.5f - u) * width;
          const float lift = .00005f + .00065f * std::sin(pi * v);
          Vec3 p = owen_crop(a, std::clamp(t, 0.f, 1.f), lift);
          if (t < 0)
            p.y += .065f * t;
          const float root = .68f + .16f * smooth(v / .4f);
          const float variation = .96f + .045f * std::sin(float(k) * 2.11f);
          return P{p, auburn * (root * variation), wt(B::Head),
                   character_surface_uv(S::HairShortStrands, u, v)};
        },
        false, false);
  }
}

CharacterRig owen_rig(float height) {
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
  child(B::Neck, B::Chest, owen_anatomy::neck_center);
  child(B::Head, B::Neck, owen_anatomy::head_center);
  for (float sign : {-1.f, 1.f}) {
    const bool right = sign > 0;
    const B arm = right ? B::RightUpperArm : B::LeftUpperArm;
    const B fore = right ? B::RightForearm : B::LeftForearm;
    const B hand = right ? B::RightHand : B::LeftHand;
    const B thigh = right ? B::RightThigh : B::LeftThigh;
    const B shin = right ? B::RightShin : B::LeftShin;
    const B foot = right ? B::RightFoot : B::LeftFoot;
    child(arm, B::Chest, {sign * .125f, .310f, 0});
    child(fore, arm, {sign * .145f, .12f, .008f});
    child(hand, fore, {sign * .170f, -.044f, .018f});
    child(thigh, B::Pelvis, {sign * .059f, 0, 0});
    child(shin, thigh, {sign * .059f, -.225f, 0});
    child(foot, shin, {sign * .059f, -.46f, 0});
  }
  return rig;
}

float neck_fit_radius(float y, float angle, float center_z) {
  // .34-.355 are closed neck contours. Higher full-head rays can hit the jaw;
  // continue the measured neck taper instead of fitting the collar to the chin.
  const float lower = std::max(y, .340f);
  if (lower <= .355f)
    return anatomy_radius(lower, angle, center_z);
  const float a = anatomy_radius(.350f, angle, center_z);
  const float c = anatomy_radius(.355f, angle, center_z);
  return std::max(.012f, c + (c - a) * ((lower - .355f) / .005f));
}

void closed_trim(Sculpt &b, const std::vector<Vec3> &path, float radius,
                 Vec3 color, S surface, B bone, int sides) {
  const int steps = int(path.size()) - 1;
  const auto base = std::uint32_t(b.m.bind_mesh.vertices.size());
  b.grid(
      sides, steps,
      [&](float u, float v) {
        const int i = int(std::lround(v * steps)) % steps;
        const Vec3 tangent =
            normalize(path[(i + 1) % steps] - path[(i + steps - 1) % steps]);
        const Vec3 normal = normalize(cross(Vec3{0, 0, 1}, tangent));
        const float a = 2 * pi * u;
        return P{path[i] + Vec3{0, 0, radius * .6f * std::cos(a)} +
                     normal * (radius * std::sin(a)),
                 color, wt(bone), character_surface_uv(surface, u, v)};
      },
      true, false);
  for (int i = 0; i <= sides; ++i)
    b.normal_groups.push_back({base + std::uint32_t(i),
                               base + std::uint32_t(steps * (sides + 1) + i)});
}

void make_owen_satchel(Sculpt &b) {
  const Vec3 leather{.22f, .145f, .085f};
  constexpr float cx = .115f, cy = .100f, cz = .025f;
  const auto satchel_begin = b.m.influences.size();
  const CharacterInfluence bag_weight = owen_jacket_detail::body_weight(cy);
  const float yaw = 90.f * pi / 180.f, tilt = 0.f;
  auto orient_vector = [&](Vec3 p) {
    const Vec3 q{p.x, std::cos(tilt) * p.y - std::sin(tilt) * p.z,
                 std::sin(tilt) * p.y + std::cos(tilt) * p.z};
    return Vec3{std::cos(yaw) * q.x + std::sin(yaw) * q.z, q.y,
                -std::sin(yaw) * q.x + std::cos(yaw) * q.z};
  };
  auto orient_point = [&](Vec3 p) {
    return Vec3{cx, cy, cz} + orient_vector(p - Vec3{cx, cy, cz});
  };
  const Vec3 bag_front = orient_vector(Vec3{0, 0, 1});
  constexpr float hx = .036f, hy = .047f, hz = .020f;
  b.grid(b.near ? 32 : 16, b.near ? 12 : 4, [&](float u, float v) {
    const float a = u == 1.f ? 0.f : 2 * pi * u;
    const float round = .88f + .12f * std::sin(pi * v);
    const float x =
        cx +
        hx * round *
            std::copysign(std::pow(std::fabs(std::cos(a)), .36f), std::cos(a));
    const float z =
        cz +
        hz * round *
            std::copysign(std::pow(std::fabs(std::sin(a)), .36f), std::sin(a));
    return P{{x, lerp(cy - hy, cy + hy, v), z},
             leather,
             wt(B::Pelvis),
             character_surface_uv(S::Leather, u, v)};
  });
  auto flap = [&](float u, float v) {
    const float half = hx - .002f - .003f * smooth((v - .8f) / .2f);
    return Vec3{cx + half * (1 - 2 * u),
                cy + hy + .002f +
                    .0035f * std::sin(pi * std::min(v / .42f, 1.f)) -
                    .052f * smooth((v - .24f) / .76f),
                cz - hz + .001f + (2 * hz + .0025f) * smooth(v / .43f)};
  };
  for (bool underside : {false, true}) {
    b.grid(
        b.near ? 16 : 8, b.near ? 12 : 6,
        [&](float u, float v) {
          const float across = underside ? u : 1 - u;
          Vec3 p = flap(across, v);
          if (underside) {
            const float step = .001f;
            const Vec3 du = flap(std::min(1.f, across + step), v) -
                            flap(std::max(0.f, across - step), v);
            const Vec3 dv = flap(across, std::min(1.f, v + step)) -
                            flap(across, std::max(0.f, v - step));
            // Offset the underside along the rolled surface normal. A fixed
            // Z shift left the nearly horizontal top sheets almost coplanar.
            p = p - normalize(cross(normalize(du), normalize(dv))) * .0010f;
          }
          return P{p, leather * (underside ? .76f : 1.01f), wt(B::Pelvis),
                   character_surface_uv(S::Leather, u, v)};
        },
        false, false);
  }
  for (float edge : {0.f, 1.f}) {
    std::vector<Vec3> path;
    const int steps = b.near ? 20 : 8;
    for (int i = 0; i <= steps; ++i)
      path.push_back(flap(edge, float(i) / steps));
    b.curve(path, .00055f, leather * .74f, S::Leather, B::Pelvis,
            b.near ? 6 : 4);
  }
  std::vector<Vec3> bottom;
  for (int i = 0; i <= 16; ++i)
    bottom.push_back(flap(float(i) / 16, 1.f));
  b.curve(bottom, .00055f, leather * .74f, S::Leather, B::Pelvis,
          b.near ? 6 : 4);
  b.ellipsoid({cx, cy + .007f, cz + hz + .0035f}, {.0036f, .0031f, .0010f},
              {.38f, .32f, .22f}, S::Metal, B::Pelvis, b.near ? 12 : 8,
              b.near ? 5 : 4);

  const Vec3 first_attachment =
      orient_point({cx + hx - .001f, cy + hy + .003f, cz});
  const Vec3 last_attachment =
      orient_point({cx - hx + .001f, cy + hy + .003f, cz});
  const std::vector<Vec3> guides{
      first_attachment,
      (first_attachment + Vec3{.092f, .174f, -.040f}) * .5f,
      {.092f, .174f, -.040f},
      {.069f, .220f, -.066f},
      {-.020f, .294f, -.061f},
      {-.073f, .335f, -.027f},
      {-.084f, .341f, .007f},
      {-.074f, .335f, .042f},
      {-.045f, .291f, .076f},
      {.005f, .196f, .078f},
      (Vec3{.005f, .196f, .078f} + last_attachment) * .5f,
      last_attachment};
  std::vector<Vec3> strap;
  const int divisions = b.near ? 6 : 3;
  for (std::size_t i = 0; i + 1 < guides.size(); ++i) {
    const Vec3 p0 = guides[i ? i - 1 : i], p1 = guides[i], p2 = guides[i + 1],
               p3 = guides[std::min(i + 2, guides.size() - 1)];
    for (int j = 0; j < divisions; ++j) {
      const float t = float(j) / divisions, t2 = t * t, t3 = t2 * t;
      strap.push_back((p1 * 2 + (p2 - p0) * t +
                       (p0 * 2 - p1 * 5 + p2 * 4 - p3) * t2 +
                       (-p0 + p1 * 3 - p2 * 3 + p3) * t3) *
                      .5f);
    }
  }
  strap.push_back(guides.back());
  const auto region = character_surface_region(S::Jacket);
  constexpr float atlas = float(character_surface_atlas_size);
  auto is_torso = [&](std::uint32_t index) {
    const auto &vertex = b.m.bind_mesh.vertices[index];
    const auto uv = vertex.uv;
    const Vec3 position = vertex.position * (1 / b.h);
    const auto w = b.m.influences[index];
    const auto limb_weight = [&](std::uint8_t bone) {
      return bone == std::uint8_t(B::LeftForearm) ||
             bone == std::uint8_t(B::RightForearm);
    };
    return uv.x >= region.x / atlas &&
           uv.x < (region.x + region.size) / atlas &&
           uv.y >= region.y / atlas &&
           uv.y < (region.y + region.size) / atlas &&
           std::fabs(position.x) < .140f && position.y > .020f &&
           !(w.first_weight > 0 && limb_weight(w.first)) &&
           !(1 - w.first_weight - w.third_weight > 0 &&
             limb_weight(w.second)) &&
           !(w.third_weight > 0 && limb_weight(w.third));
  };
  auto combine_weights = [&](CharacterInfluence a, CharacterInfluence c,
                             CharacterInfluence d, float u, float v) {
    std::array<float, character_bone_count> weights{};
    auto add = [&](CharacterInfluence w, float factor) {
      weights[w.first] += factor * w.first_weight;
      weights[w.second] += factor * (1 - w.first_weight - w.third_weight);
      weights[w.third] += factor * w.third_weight;
    };
    add(a, 1 - u - v);
    add(c, u);
    add(d, v);
    std::array<std::size_t, character_bone_count> order{};
    for (std::size_t i = 0; i < order.size(); ++i)
      order[i] = i;
    std::stable_sort(order.begin(), order.end(),
                     [&](auto x, auto y) { return weights[x] > weights[y]; });
    const float total =
        weights[order[0]] + weights[order[1]] + weights[order[2]];
    if (total <= 0)
      throw std::runtime_error("Invalid strap skin attachment");
    return CharacterInfluence{std::uint8_t(order[0]), std::uint8_t(order[1]),
                              weights[order[0]] / total, std::uint8_t(order[2]),
                              weights[order[2]] / total};
  };
  std::vector<Vec3> strap_normals(strap.size(), bag_front);
  std::vector<CharacterInfluence> strap_weights(strap.size(), bag_weight);
  for (std::size_t n = 0; n < strap.size(); ++n) {
    auto &point = strap[n];
    const Vec3 anchor{0, .205f, 0}, direction = normalize(point - anchor);
    float distance = 0;
    Vec3 surface_normal = direction;
    CharacterInfluence attachment = owen_jacket_detail::body_weight(point.y);
    const auto &mesh = b.m.bind_mesh;
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
      const auto ia = mesh.indices[i], ib = mesh.indices[i + 1],
                 ic = mesh.indices[i + 2];
      if (!is_torso(ia) || !is_torso(ib) || !is_torso(ic))
        continue;
      const Vec3 a = mesh.vertices[ia].position * (1 / b.h);
      const Vec3 edge1 =
          (mesh.vertices[ib].position - mesh.vertices[ia].position) * (1 / b.h);
      const Vec3 edge2 =
          (mesh.vertices[ic].position - mesh.vertices[ia].position) * (1 / b.h);
      const Vec3 p = cross(direction, edge2);
      const float det = dot(edge1, p);
      if (std::fabs(det) < 1e-12f)
        continue;
      const Vec3 delta = anchor - a;
      const float u = dot(delta, p) / det;
      if (u < 0 || u > 1)
        continue;
      const Vec3 q = cross(delta, edge1);
      const float v = dot(direction, q) / det;
      if (v < 0 || u + v > 1)
        continue;
      const float hit = dot(edge2, q) / det;
      if (hit > distance) {
        distance = hit;
        surface_normal = normalize(cross(edge1, edge2));
        if (dot(surface_normal, direction) < 0)
          surface_normal = -surface_normal;
        attachment = combine_weights(b.m.influences[ia], b.m.influences[ib],
                                     b.m.influences[ic], u, v);
      }
    }
    if (distance > 0) {
      const float t = float(n) / float(strap.size() - 1);
      const float fit = smooth(t / .13f) * smooth((1 - t) / .13f);
      point =
          point * (1 - fit) + (anchor + direction * (distance + .0020f)) * fit;
      strap_normals[n] =
          normalize(bag_front * (1 - fit) + surface_normal * fit);
      strap_weights[n] =
          combine_weights(bag_weight, attachment, attachment, fit, 0);
    }
  }
  std::vector<Vec3> width_axes(strap.size()), depth_axes(strap.size());
  for (std::size_t i = 0; i < strap.size(); ++i) {
    const Vec3 tangent = normalize(strap[std::min(i + 1, strap.size() - 1)] -
                                   strap[i ? i - 1 : 0]);
    Vec3 normal = strap_normals[i] - tangent * dot(strap_normals[i], tangent);
    if (dot(normal, normal) < 1e-10f)
      normal = Vec3{0, 0, 1} - tangent * tangent.z;
    depth_axes[i] = normalize(normal);
    width_axes[i] = normalize(cross(tangent, depth_axes[i]));
  }
  const int strap_rows = int(strap.size()) - 1;
  const auto strap_begin = b.m.influences.size();
  b.grid(b.near ? 8 : 6, strap_rows, [&](float u, float v) {
    const auto i =
        std::min(strap.size() - 1, std::size_t(std::lround(v * strap_rows)));
    const float a = 2 * pi * u;
    return P{strap[i] + width_axes[i] * (.0060f * std::cos(a)) +
                 depth_axes[i] * (.00060f * std::sin(a)),
             leather * .88f, strap_weights[i],
             character_surface_uv(S::Leather, u, v)};
  });
  const auto strap_end = b.m.influences.size();
  for (float sign : {-1.f, 1.f}) {
    const float x = cx + sign * (hx - .001f);
    b.ellipsoid({x, cy + hy - .005f, cz}, {.0028f, .008f, .0030f},
                leather * .91f, S::Leather, B::Pelvis, b.near ? 12 : 8, 4);
    if (b.near) {
      std::vector<Vec3> ring;
      for (int i = 0; i <= 20; ++i) {
        const float a = 2 * pi * float(i) / 20;
        ring.push_back({x + .0036f * std::cos(a),
                        cy + hy + .003f + .0045f * std::sin(a), cz});
      }
      closed_trim(b, ring, .00060f, {.38f, .32f, .22f}, S::Metal, B::Pelvis, 6);
    }
  }
  for (std::size_t i = satchel_begin; i < b.m.influences.size(); ++i)
    if (i < strap_begin || i >= strap_end) {
      b.m.influences[i] = bag_weight;
      auto &p = b.m.bind_mesh.vertices[i].position;
      p = orient_point(p * (1.f / b.h)) * b.h;
    }
}
} // namespace

CharacterProfile make_owen_character_profile(float height, CharacterRole role,
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
  model.rig = owen_rig(height);
  model.dual_quaternion_skinning = true;
  model.bind_mesh.vertices.reserve(lod == CharacterLod::Near ? 32000 : 12000);
  model.bind_mesh.indices.reserve(lod == CharacterLod::Near ? 140000 : 60000);
  model.influences.reserve(model.bind_mesh.vertices.capacity());
  Sculpt sculpt{model, height, lod == CharacterLod::Near, {}, {}};
  make_owen_jacket(sculpt, neck_fit_radius, owen_anatomy::neck_center);
  make_hands_and_legs(sculpt);
  make_head(sculpt);
  make_owen_hair(sculpt);
  // Segment-aligned carrying bind keeps the right arm clear of the side
  // satchel.
  const auto upper = std::uint8_t(CharacterBone::RightUpperArm);
  const auto fore = std::uint8_t(CharacterBone::RightForearm);
  const auto hand = std::uint8_t(CharacterBone::RightHand);
  const Vec3 shoulder = model.rig.joints[upper].bind_position;
  const Vec3 elbow = model.rig.joints[fore].bind_position;
  const Vec3 wrist = model.rig.joints[hand].bind_position;
  const Vec3 new_elbow = elbow + Vec3{.032f * height, 0, 0};
  const Vec3 new_wrist = wrist + Vec3{.025f * height, 0, 0};
  auto map_segment = [&](Vec3 p, Vec3 a, Vec3 z, Vec3 na, Vec3 nz) {
    const auto direction = z - a, new_direction = nz - na;
    const float old_length = length(direction),
                new_length = length(new_direction);
    const Vec3 u = direction * (1.f / old_length),
               v = new_direction * (1.f / new_length);
    const float t = dot(p - a, u);
    const Vec3 radial = p - a - u * t, k = cross(u, v);
    const Vec3 rotated = radial + cross(k, radial) +
                         cross(k, cross(k, radial)) * (1.f / (1.f + dot(u, v)));
    return na + v * (t * (new_length / old_length)) + rotated;
  };
  for (std::size_t i = 0; i < model.bind_mesh.vertices.size(); ++i) {
    auto &position = model.bind_mesh.vertices[i].position;
    const auto &w = model.influences[i];
    const Vec3 source = position;
    auto displacement = [&](std::uint8_t bone) {
      if (bone == upper)
        return map_segment(source, shoulder, elbow, shoulder, new_elbow) -
               source;
      if (bone == fore)
        return map_segment(source, elbow, wrist, new_elbow, new_wrist) - source;
      if (bone == hand)
        return new_wrist - wrist;
      return Vec3{};
    };
    position +=
        displacement(w.first) * w.first_weight +
        displacement(w.second) * (1.f - w.first_weight - w.third_weight) +
        displacement(w.third) * w.third_weight;
  }
  model.rig.joints[fore].bind_position = new_elbow;
  model.rig.joints[hand].bind_position = new_wrist;
  // Fit the bag/strap against the final authored carrying-pose garment.
  make_owen_satchel(sculpt);
  // Rebuild face and chart-seam normals after the remap.
  sculpt.finish();
  profile.material = make_character_surface_material();
  profile.material.alpha_cutoff = .4f;
  profile.lod_distance = 12.f;
  profile.individualized = true;
  return profile;
}
} // namespace fury
