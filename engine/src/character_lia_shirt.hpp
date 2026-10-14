#pragma once

#include "character_sculpt.hpp"
#include <tuple>

namespace fury::lia_shirt_detail {
using namespace character_detail;

// One physical pattern vertex may have several texture corners. Keeping the
// source position AND influence in this table makes sewn UV splits exact under
// every pose; the table is also the authoritative normal-welding relation.
struct Pattern {
  Sculpt &sculpt;
  std::vector<P> points;
  std::vector<std::vector<std::uint32_t>> corners;
  std::map<std::tuple<std::uint32_t, float, float>, std::uint32_t> emitted;
  std::uint32_t node(Vec3 p, Vec3 color, CharacterInfluence weights) {
    points.push_back({p, color, weights, {}});
    corners.emplace_back();
    return std::uint32_t(points.size() - 1);
  }
  std::uint32_t corner(std::uint32_t node, Vec2 uv) {
    const auto key = std::make_tuple(node, uv.x, uv.y);
    const auto found = emitted.find(key);
    if (found != emitted.end())
      return found->second;
    P p = points[node];
    p.uv = uv;
    const auto result = sculpt.vertex(p);
    emitted.emplace(key, result);
    corners[node].push_back(result);
    return result;
  }
  void triangle(std::uint32_t a, std::uint32_t b, std::uint32_t c, Vec2 ua,
                Vec2 ub, Vec2 uc) {
    sculpt.tri(corner(a, ua), corner(b, ub), corner(c, uc));
  }
  void quad(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d,
            Vec2 ua, Vec2 ub, Vec2 uc, Vec2 ud) {
    triangle(a, b, c, ua, ub, uc);
    triangle(a, c, d, ua, uc, ud);
  }
  void finish() {
    for (auto &group : corners)
      if (group.size() > 1)
        sculpt.normal_groups.push_back(std::move(group));
  }
};

inline CharacterInfluence body_weight(float y) {
  if (y < .14f)
    return wt(B::Spine, B::Pelvis, smooth((y - .025f) / .115f));
  return wt(B::Chest, B::Spine, smooth((y - .14f) / .095f));
}

inline float section(float y, const std::array<float, 6> &values) {
  constexpr std::array<float, 6> levels{.012f, .07f, .14f, .22f, .28f, .320f};
  for (int k = 0; k < 5; ++k)
    if (y <= levels[k + 1])
      return lerp(values[k], values[k + 1],
                  smooth((y - levels[k]) / (levels[k + 1] - levels[k])));
  return values.back();
}

inline Vec3 body_point(float a, float y) {
  const float cs = std::cos(a), sn = std::sin(a);
  const float rx = section(y, {.087f, .094f, .101f, .112f, .116f, .113f});
  const float rz = section(y, {.032f, .058f, .066f, .074f, .073f, .062f});
  // Local broad pulls, rather than equally spaced circumferential corrugation.
  const float front = std::max(0.f, sn), back = std::max(0.f, -sn);
  const float fold = .0011f * g(y, .075f + .039f * cs, .027f) * front * front -
                     .0008f * g(y, .115f + .031f * cs, .023f) * front * front +
                     .0010f * g(y, .205f + .055f * cs, .029f) * front * front +
                     .0012f * g(y, .162f - .033f * cs, .047f) * back * back +
                     .0010f * g(y, .035f, .024f) * std::sin(3 * a + .7f);
  return {(rx + fold) * cs, y,
          (rz + fold) * sn +
              .006f * front * front * smooth((y - .050f) / .060f) -
              .002f * back * back};
}

inline Vec3 armhole_point(float a, float y) {
  constexpr float half_angle = 3 * pi / 16;
  const float side = std::cos(a) < 0 ? -1.f : 1.f;
  const float u = std::sin(a) / std::sin(half_angle);
  const float v = (y - .2625f) / .0575f;
  const float uc = std::clamp(u, -1.f, 1.f);
  const float vc = std::clamp(v, -1.f, 1.f);
  const float U = uc * std::sqrt(1 - .5f * vc * vc);
  const float V = vc * std::sqrt(1 - .5f * uc * uc);
  float ac = std::asin(uc * std::sin(half_angle));
  if (side < 0)
    ac = pi - ac;
  const Vec3 original = body_point(ac, .2625f + .0575f * vc);
  const Vec3 round{side * (.100f + .016f * V + .009f * U * U),
                   .2625f + .0575f * V, .047f * U + .002f};
  const float support = (1 - smooth((std::fabs(u) - 1) / .72f)) *
                        (1 - smooth((std::fabs(v) - 1) / .65f));
  return body_point(a, y) + (round - original) * support;
}

inline CharacterInfluence torso_weight(float a, float y) {
  constexpr float half_angle = 3 * pi / 16;
  const float u = std::sin(a) / std::sin(half_angle);
  const float v = (y - .2625f) / .0575f;
  const float arm = (.74f - .22f * smooth((v + 1) * .5f)) *
                    (1 - smooth((std::fabs(u) - 1) / .72f)) *
                    (1 - smooth((std::fabs(v) - 1) / 1.00f));
  if (arm <= 0)
    return body_weight(y);
  const auto body = body_weight(y);
  return {body.first, body.second, (1 - arm) * body.first_weight,
          std::uint8_t(std::cos(a) < 0 ? B::LeftUpperArm : B::RightUpperArm),
          arm};
}
} // namespace fury::lia_shirt_detail

namespace fury {
// Original Lia service-shirt pattern, developed from Fury's sewn-armhole
// method. All dimensions are normalized by height, +Z faces forward and the
// pelvis is the origin. No reference mesh, texture or commercial garment
// pattern is used. The supplied query is the source neck radial intersection,
// measured from (0,y,neck_center.z).
inline void
make_lia_shirt(character_detail::Sculpt &b,
               const std::function<float(float, float, float)> &anatomy_radius,
               Vec3 neck_center) {
  using namespace character_detail;
  using namespace lia_shirt_detail;
  Pattern pattern{b, {}, {}, {}};
  const Vec3 cloth{.62f, .66f, .68f};
  const int columns = b.near ? 64 : 32;
  const int below = b.near ? 16 : 5, hole_rows = b.near ? 12 : 6;
  const int rows = below + hole_rows, half_hole = b.near ? 6 : 3;
  const auto angle = [&](int column) {
    return pi * .5f + 2 * pi * float(column % columns) / columns;
  };
  const auto height = [&](int row) {
    return row <= below ? lerp(.012f, .205f, float(row) / below)
                        : lerp(.205f, .320f, float(row - below) / hole_rows);
  };
  const auto uv = [&](float metres_u, float metres_v) {
    return character_surface_uv(S::Shirt, .025f + metres_u / .75f,
                                .025f + metres_v / .70f);
  };
  std::vector<std::vector<std::uint32_t>> torso(
      rows + 1, std::vector<std::uint32_t>(columns));
  for (int j = 0; j <= rows; ++j)
    for (int i = 0; i < columns; ++i) {
      const float a = angle(i), y = height(j);
      torso[j][i] =
          pattern.node(armhole_point(a, y), cloth, torso_weight(a, y));
    }
  const auto body_index = [&](int j, int i) { return torso[j][i % columns]; };
  // Two body charts retain cloth's metre scale; the front/back centers are
  // exact physical seams with distinct texture corners.
  std::vector<float> circumference(columns + 1);
  for (int i = 1; i <= columns; ++i)
    circumference[i] =
        circumference[i - 1] +
        length(body_point(angle(i), .23f) - body_point(angle(i - 1), .23f)) *
            b.h;
  const auto body_uv = [&](int j, int i, int chart) {
    return uv(circumference[i] - circumference[chart * columns / 2],
              (height(j) - .012f) * b.h);
  };
  const auto body_triangle_begin = b.m.bind_mesh.indices.size();
  for (int j = 0; j < rows; ++j)
    for (int i = 0; i < columns; ++i) {
      bool hole = false;
      for (int center : {columns / 4, 3 * columns / 4})
        hole |= j >= below && i >= center - half_hole && i < center + half_hole;
      if (hole)
        continue;
      const int chart = i / (columns / 2);
      pattern.quad(body_index(j, i), body_index(j + 1, i),
                   body_index(j + 1, i + 1), body_index(j, i + 1),
                   body_uv(j, i, chart), body_uv(j + 1, i, chart),
                   body_uv(j + 1, i + 1, chart), body_uv(j, i + 1, chart));
    }
  const auto body_triangle_end = b.m.bind_mesh.indices.size();

  // Shoulder yoke terminates in the real neck envelope. Its first row is the
  // actual torso row, including the upper edges of both open armholes.
  std::vector<std::uint32_t> previous = torso.back();
  std::vector<Vec3> neckline(columns);
  for (int i = 0; i < columns; ++i) {
    const float a = angle(i), front = std::max(0.f, std::sin(a));
    const float y = .347f - .003f * front + .003f * std::max(0.f, -std::sin(a));
    const float r = anatomy_radius(y, a, neck_center.z) + .0030f;
    neckline[i] = {r * std::cos(a), y, neck_center.z + r * std::sin(a)};
  }
  const int yoke_rows = b.near ? 4 : 2;
  for (int j = 1; j <= yoke_rows; ++j) {
    const float v = float(j) / yoke_rows;
    std::vector<std::uint32_t> next(columns);
    for (int i = 0; i < columns; ++i) {
      const float a = angle(i);
      const P &start = pattern.points[torso.back()[i]];
      Vec3 p = start.p * (1 - v) + neckline[i] * v;
      p.y += .002f * std::sin(pi * v);
      const Vec3 radial{p.x, 0, p.z - neck_center.z};
      const float r = length(radial);
      if (r > 1e-6f) {
        const float actual_angle = std::atan2(radial.z, radial.x);
        const float skin = anatomy_radius(p.y, actual_angle, neck_center.z);
        p += radial * (std::max(0.f, skin + .0025f - r) / r);
      }
      auto weights = torso_weight(a, .320f);
      weights.third_weight *=
          (1 - v) * smooth((std::fabs(p.x) - .060f) / .048f);
      weights.first_weight = 1 - weights.third_weight;
      next[i] = pattern.node(p, cloth, weights);
    }
    for (int i = 0; i < columns; ++i) {
      const int k = (i + 1) % columns, chart = i / (columns / 2);
      auto tex = [&](int ii, float vv) {
        return uv((circumference[ii] - circumference[chart * columns / 2]) *
                      lerp(1.f, .53f, vv),
                  (.321f + .052f * vv) * b.h);
      };
      const float low = float(j - 1) / yoke_rows;
      pattern.quad(previous[i], next[i], next[k], previous[k], tex(i, low),
                   tex(i, v), tex(i + 1, v), tex(i + 1, low));
    }
    previous = std::move(next);
  }
  const auto neckline_nodes = previous;
  const auto yoke_triangle_end = b.m.bind_mesh.indices.size();
  const auto yoke_sample = [&](float x, float z, bool required = true) {
    float highest = -1.f;
    CharacterInfluence fitted_weights = wt(B::Chest);
    const auto &mesh = b.m.bind_mesh;
    for (std::size_t i = body_triangle_end; i < yoke_triangle_end; i += 3) {
      const Vec3 a = mesh.vertices[mesh.indices[i]].position * (1 / b.h);
      const Vec3 c = mesh.vertices[mesh.indices[i + 1]].position * (1 / b.h);
      const Vec3 d = mesh.vertices[mesh.indices[i + 2]].position * (1 / b.h);
      const float denominator =
          (c.x - a.x) * (d.z - a.z) - (c.z - a.z) * (d.x - a.x);
      if (std::fabs(denominator) < 1e-12f)
        continue;
      const float u =
          ((x - a.x) * (d.z - a.z) - (z - a.z) * (d.x - a.x)) / denominator;
      const float v =
          ((c.x - a.x) * (z - a.z) - (c.z - a.z) * (x - a.x)) / denominator;
      if (u >= -1e-6f && v >= -1e-6f && u + v <= 1.000001f) {
        const float y = a.y + u * (c.y - a.y) + v * (d.y - a.y);
        if (y > highest) {
          highest = y;
          const auto wa = b.m.influences[mesh.indices[i]];
          const auto wc = b.m.influences[mesh.indices[i + 1]];
          const auto wd = b.m.influences[mesh.indices[i + 2]];
          const float arm =
              std::clamp(wa.third_weight * (1 - u - v) + wc.third_weight * u +
                             wd.third_weight * v,
                         0.f, 1.f);
          fitted_weights = {
              std::uint8_t(B::Chest), std::uint8_t(B::Spine), 1 - arm,
              std::uint8_t(x < 0 ? B::LeftUpperArm : B::RightUpperArm), arm};
        }
      }
    }
    if (highest < 0 && required)
      throw std::runtime_error(
          "Lia turned collar must rest above the actual yoke");
    return P{{x, highest, z}, cloth, fitted_weights, {}};
  };

  struct FrontFit {
    Vec3 point, normal;
    CharacterInfluence weights;
  };
  const auto front_fit = [&](float x, float y) {
    // Fit to the actual LOD triangles, not just their smooth generating
    // function: a coarse panel may bridge above the analytic surface.
    float front = -1.f;
    Vec3 normal{0, 0, 1};
    CharacterInfluence fitted = wt(B::Chest);
    const auto &mesh = b.m.bind_mesh;
    for (std::size_t i = body_triangle_begin; i < yoke_triangle_end; i += 3) {
      const Vec3 a = mesh.vertices[mesh.indices[i]].position * (1 / b.h);
      const Vec3 c = mesh.vertices[mesh.indices[i + 1]].position * (1 / b.h);
      const Vec3 d = mesh.vertices[mesh.indices[i + 2]].position * (1 / b.h);
      if (x < std::min({a.x, c.x, d.x}) || x > std::max({a.x, c.x, d.x}) ||
          y < std::min({a.y, c.y, d.y}) || y > std::max({a.y, c.y, d.y}))
        continue;
      const float denominator =
          (c.x - a.x) * (d.y - a.y) - (c.y - a.y) * (d.x - a.x);
      if (std::fabs(denominator) < 1e-12f)
        continue;
      const float u =
          ((x - a.x) * (d.y - a.y) - (y - a.y) * (d.x - a.x)) / denominator;
      const float v =
          ((c.x - a.x) * (y - a.y) - (c.y - a.y) * (x - a.x)) / denominator;
      if (u >= -1e-6f && v >= -1e-6f && u + v <= 1.000001f) {
        const float z = a.z + u * (c.z - a.z) + v * (d.z - a.z);
        if (z <= front)
          continue;
        front = z;
        normal = normalize(cross(c - a, d - a));
        if (normal.z < 0)
          normal = -normal;
        std::array<float, character_bone_count> values{};
        const float bary[]{1 - u - v, u, v};
        for (int k = 0; k < 3; ++k) {
          const auto w = b.m.influences[mesh.indices[i + k]];
          values[w.first] += bary[k] * w.first_weight;
          values[w.second] += bary[k] * (1 - w.first_weight - w.third_weight);
          values[w.third] += bary[k] * w.third_weight;
        }
        std::array<std::uint8_t, character_bone_count> order{};
        for (std::size_t k = 0; k < order.size(); ++k)
          order[k] = std::uint8_t(k);
        std::stable_sort(order.begin(), order.end(),
                         [&](auto x, auto y) { return values[x] > values[y]; });
        const float total = std::max(
            1e-8f, values[order[0]] + values[order[1]] + values[order[2]]);
        const float first = std::clamp(values[order[0]] / total, 0.f, 1.f);
        const float third =
            std::clamp(values[order[2]] / total, 0.f, 1 - first);
        fitted = {order[0], order[1], first, order[2], third};
      }
    }
    if (front < -.1f)
      throw std::runtime_error("Lia placket must fit an actual body panel");
    return FrontFit{{x, y, front}, normal, fitted};
  };
  const auto front_point = [&](float x, float y) {
    return front_fit(x, y).point;
  };
  const auto cloth_clearance = [&](Vec3 point, float margin) {
    const Vec3 origin{0, .265f, .012f};
    const Vec3 delta = point - origin, direction = normalize(delta);
    float distance = -1;
    CharacterInfluence weights = wt(B::Chest);
    const auto &mesh = b.m.bind_mesh;
    for (std::size_t t = body_triangle_begin; t < yoke_triangle_end; t += 3) {
      const auto ia = mesh.indices[t], ib = mesh.indices[t + 1],
                 ic = mesh.indices[t + 2];
      const Vec3 a = mesh.vertices[ia].position * (1 / b.h);
      const Vec3 e1 = mesh.vertices[ib].position * (1 / b.h) - a;
      const Vec3 e2 = mesh.vertices[ic].position * (1 / b.h) - a;
      const Vec3 p = cross(direction, e2);
      const float det = dot(e1, p);
      if (std::fabs(det) < 1e-12f)
        continue;
      const Vec3 q = origin - a;
      const float u = dot(q, p) / det;
      const Vec3 c = cross(q, e1);
      const float v = dot(direction, c) / det;
      const float d = dot(e2, c) / det;
      // Rays on a sewn UV boundary must hit either incident triangle. Roundoff
      // at the center seam otherwise leaves a single collar point unsupported.
      constexpr float edge_tolerance = 1e-6f;
      if (u < -edge_tolerance || v < -edge_tolerance ||
          u + v > 1 + edge_tolerance || d <= distance)
        continue;
      distance = d;
      const float arm =
          std::clamp(b.m.influences[ia].third_weight * (1 - u - v) +
                         b.m.influences[ib].third_weight * u +
                         b.m.influences[ic].third_weight * v,
                     0.f, 1.f);
      weights = {std::uint8_t(B::Chest), std::uint8_t(B::Spine), 1 - arm,
                 std::uint8_t(point.x < 0 ? B::LeftUpperArm : B::RightUpperArm),
                 arm};
    }
    if (distance > 0 && length(delta) < distance + margin)
      point = origin + direction * (distance + margin);
    return P{point, cloth, weights, {}};
  };

  // Lia's service collar is sewn to a complete fitted stand. The leaves have
  // pointed front ends and a full inner sheet, rather than an open edge ribbon.
  std::vector<std::uint32_t> stand = neckline_nodes;
  for (int row = 1; row <= 3; ++row) {
    const float t = float(row) / 3;
    std::vector<std::uint32_t> next(columns);
    for (int i = 0; i < columns; ++i) {
      const float a = angle(i);
      const P base = pattern.points[neckline_nodes[i]];
      const float y = base.p.y + .011f * t;
      const float r = anatomy_radius(y, a, neck_center.z) + .0030f;
      next[i] =
          pattern.node({r * std::cos(a), y, neck_center.z + r * std::sin(a)},
                       cloth, wt(B::Chest));
    }
    for (int i = 0; i < columns; ++i) {
      int k = (i + 1) % columns;
      float u0 = .30f * i / columns, u1 = .30f * (i + 1) / columns;
      pattern.quad(stand[i], next[i], next[k], stand[k],
                   uv(u0, .018f * (row - 1) / 3), uv(u0, .018f * t),
                   uv(u1, .018f * t), uv(u1, .018f * (row - 1) / 3));
    }
    stand = std::move(next);
  }
  const int collar_first = b.near ? 2 : 1, collar_last = columns - collar_first;
  const int leaf_rows = b.near ? 12 : 7;
  std::vector<Vec3> resting(columns);
  for (int i = collar_first; i <= collar_last; ++i) {
    const float a = angle(i);
    const Vec3 base = pattern.points[stand[i]].p;
    const float r = length(Vec3{base.x, 0, base.z - neck_center.z}) + .014f;
    Vec3 edge{r * std::cos(a), 0, neck_center.z + r * std::sin(a)};
    const P support = yoke_sample(edge.x, edge.z);
    edge.y = support.p.y + .0018f;
    resting[i] = edge;
  }
  const int point_span = b.near ? 8 : 4;
  for (int end = 0; end < 2; ++end) {
    const int endpoint = end ? collar_last : collar_first;
    const int direction = end ? -1 : 1;
    const Vec3 anchor = resting[endpoint + direction * point_span];
    const float side = end ? 1.f : -1.f;
    const Vec3 tip = front_point(side * .020f, .307f) + Vec3{0, 0, .0028f};
    for (int j = 0; j < point_span; ++j) {
      const float t = float(j) / point_span;
      resting[endpoint + direction * j] = tip * (1 - t) + anchor * t;
    }
  }
  std::vector<std::vector<std::uint32_t>> outer(
      leaf_rows + 1, std::vector<std::uint32_t>(columns));
  outer[0] = stand;
  for (int j = 1; j <= leaf_rows; ++j) {
    const float t = float(j) / leaf_rows, s = 1 - t;
    for (int i = collar_first; i <= collar_last; ++i) {
      const float a = angle(i);
      const Vec3 radial{std::cos(a), 0, std::sin(a)};
      const Vec3 base = pattern.points[stand[i]].p, edge = resting[i];
      const Vec3 first = base + radial * .0055f + Vec3{0, .0025f, 0};
      const Vec3 second = edge - radial * .005f + Vec3{0, .006f, 0};
      Vec3 p = base * (s * s * s) + first * (3 * s * s * t) +
               second * (3 * s * t * t) + edge * (t * t * t);
      const P support = cloth_clearance(p, b.near ? .0040f : .0050f);
      p = support.p;
      CharacterInfluence weights = support.w;
      weights.third_weight *= smooth(t);
      weights.first_weight = 1 - weights.third_weight;
      outer[j][i] = pattern.node(p, cloth * (1 - .035f * t), weights);
    }
    for (int i = collar_first; i < collar_last; ++i) {
      const float u0 = .34f * (i - collar_first) / (collar_last - collar_first),
                  u1 = .34f * (i + 1 - collar_first) /
                       (collar_last - collar_first);
      pattern.quad(outer[j - 1][i], outer[j][i], outer[j][i + 1],
                   outer[j - 1][i + 1], uv(u0, .058f * (j - 1) / leaf_rows),
                   uv(u0, .058f * t), uv(u1, .058f * t),
                   uv(u1, .058f * (j - 1) / leaf_rows));
    }
  }
  std::vector<std::vector<std::uint32_t>> inner = outer;
  for (int j = 0; j <= leaf_rows; ++j)
    for (int i = collar_first; i <= collar_last; ++i) {
      const P p = pattern.points[outer[j][i]];
      const Vec3 along =
          pattern.points[outer[j][std::min(i + 1, collar_last)]].p -
          pattern.points[outer[j][std::max(i - 1, collar_first)]].p;
      const Vec3 across =
          pattern.points[outer[std::min(j + 1, leaf_rows)][i]].p -
          pattern.points[outer[std::max(j - 1, 0)][i]].p;
      const Vec3 normal = normalize(cross(across, along));
      inner[j][i] = pattern.node(p.p - normal * .00075f, cloth * .94f, p.w);
    }
  for (int j = 0; j < leaf_rows; ++j)
    for (int i = collar_first; i < collar_last; ++i) {
      const float u0 = .34f * (i - collar_first) / (collar_last - collar_first),
                  u1 = .34f * (i + 1 - collar_first) /
                       (collar_last - collar_first);
      pattern.quad(inner[j][i], inner[j][i + 1], inner[j + 1][i + 1],
                   inner[j + 1][i], uv(u0, .058f * j / leaf_rows),
                   uv(u1, .058f * j / leaf_rows),
                   uv(u1, .058f * (j + 1) / leaf_rows),
                   uv(u0, .058f * (j + 1) / leaf_rows));
    }
  // Duplicate only normal corners at the fold's sharp cut edge. Position and
  // skin weights remain exact and the physical seam still closes topologically.
  auto corner_copy = [&](std::uint32_t id) {
    const P p = pattern.points[id];
    return pattern.node(p.p, p.c, p.w);
  };
  for (int i = collar_first; i < collar_last; ++i) {
    const float u0 = .34f * (i - collar_first) / (collar_last - collar_first),
                u1 = .34f * (i + 1 - collar_first) /
                     (collar_last - collar_first);
    pattern.quad(corner_copy(outer[leaf_rows][i]),
                 corner_copy(inner[leaf_rows][i]),
                 corner_copy(inner[leaf_rows][i + 1]),
                 corner_copy(outer[leaf_rows][i + 1]), uv(u0, .058f),
                 uv(u0, .06f), uv(u1, .06f), uv(u1, .058f));
  }
  for (int j = 0; j < leaf_rows; ++j) {
    const float v0 = .058f * j / leaf_rows, v1 = .058f * (j + 1) / leaf_rows;
    const int l = collar_first, r = collar_last;
    pattern.quad(corner_copy(outer[j][l]), corner_copy(inner[j][l]),
                 corner_copy(inner[j + 1][l]), corner_copy(outer[j + 1][l]),
                 uv(0, v0), uv(.002f, v0), uv(.002f, v1), uv(0, v1));
    pattern.quad(corner_copy(outer[j + 1][r]), corner_copy(inner[j + 1][r]),
                 corner_copy(inner[j][r]), corner_copy(outer[j][r]), uv(0, v1),
                 uv(.002f, v1), uv(.002f, v0), uv(0, v0));
  }

  // Sleeves are extruded from the ordered boundary of each missing torso
  // region. The first ring reuses the torso nodes: there is no cap, buried
  // sleeve tube, or solid chest surface across either opening.
  for (int center : {columns / 4, 3 * columns / 4}) {
    std::vector<std::uint32_t> root;
    const int lo = center - half_hole, hi = center + half_hole;
    for (int i = lo; i < hi; ++i)
      root.push_back(body_index(below, i));
    for (int j = below; j < rows; ++j)
      root.push_back(body_index(j, hi));
    for (int i = hi; i > lo; --i)
      root.push_back(body_index(rows, i));
    for (int j = rows; j > below; --j)
      root.push_back(body_index(j, lo));
    const int count = int(root.size());
    const float side = center < columns / 2 ? -1.f : 1.f;
    const B upper = side < 0 ? B::LeftUpperArm : B::RightUpperArm;
    const B fore = side < 0 ? B::LeftForearm : B::RightForearm;
    std::vector<float> phase(count), distance(count + 1);
    for (int i = 0; i < count; ++i) {
      const Vec3 p = pattern.points[root[i]].p;
      phase[i] = std::atan2((p.z - .002f) / .047f, (p.y - .2625f) / .0575f);
      if (i)
        distance[i] = distance[i - 1] + length(pattern.points[root[i]].p -
                                               pattern.points[root[i - 1]].p) *
                                            b.h;
    }
    distance[count] =
        distance[count - 1] +
        length(pattern.points[root.front()].p - pattern.points[root.back()].p) *
            b.h;
    previous = root;
    const int cap_rows = b.near ? 5 : 3, lower_rows = b.near ? 15 : 8;
    const int sleeve_rows = cap_rows + lower_rows;
    float previous_run = 0;
    for (int j = 1; j <= sleeve_rows + 2; ++j) {
      std::vector<std::uint32_t> next(count);
      const float run = std::min(1.f, float(j) / sleeve_rows);
      for (int i = 0; i < count; ++i) {
        const float a = phase[i], cs = std::cos(a), sn = std::sin(a);
        Vec3 p{};
        CharacterInfluence weights{};
        if (j <= cap_rows) {
          const float t = float(j) / cap_rows;
          const P &start = pattern.points[root[i]];
          const Vec3 end{side * (.137f + .032f * cs), .216f + .0035f * cs,
                         .006f + .033f * sn};
          p = start.p * (1 - t) + end * t;
          p.x += side * .010f * std::sin(pi * t) * (.5f + .5f * cs);
          // The underarm arches up and then turns down into the sleeve. A
          // straight interpolation would reverse direction at the first arm
          // ring and create a folding coarse-LOD triangle during a walk.
          p.y += std::sin(pi * t) *
                 (.030f * std::max(0.f, cs) + .020f * std::max(0.f, -cs));
          const float arm = lerp(start.w.third_weight, 1.f, smooth(t));
          const float torso_chest =
              start.w.first_weight / std::max(1e-5f, 1 - start.w.third_weight);
          weights = {std::uint8_t(B::Chest), std::uint8_t(B::Spine),
                     (1 - arm) * torso_chest, std::uint8_t(upper), arm};
        } else {
          const float t = std::min(1.f, float(j - cap_rows) / lower_rows);
          float y = lerp(.216f, -.002f, t);
          if (b.near && j - cap_rows == lower_rows - 2)
            y = .030f;
          if (b.near && j - cap_rows == lower_rows - 1)
            y = .017f;
          const float elbow_t = smooth((.216f - y) / .073f);
          const float wrist_t = smooth((.143f - y) / .145f);
          const float x = lerp(.138f, .145f, elbow_t) + .025f * wrist_t;
          const float z = lerp(.006f, .008f, elbow_t) + .010f * wrist_t;
          const float front = sq(std::max(0.f, sn));
          const float back = sq(std::max(0.f, -sn));
          const float fold = .0016f * g(y, .114f + .014f * cs, .020f) * front -
                             .0010f * g(y, .140f - .010f * cs, .018f) * front +
                             .0012f * g(y, .080f - .013f * cs, .017f) * front +
                             .0014f * g(y, .127f, .037f) * back;
          float r = lerp(.032f, .027f, elbow_t) - .006f * wrist_t + fold;
          // A proper cuff band gathers the sleeve into a quieter straight
          // section.
          const float cuff_band = smooth((.031f - y) / .004f);
          r = lerp(r, .0222f, cuff_band);
          r += .0007f * g(y, .029f, .004f);
          p = {side * (x + r * cs), y + .0035f * cs * (1 - t),
               z + r * (1.03f - .05f * t) * sn};
          if (j == sleeve_rows + 1) {
            p.x -= side * .00135f * cs;
            p.y -= .0003f;
            p.z -= .00135f * sn;
          } else if (j == sleeve_rows + 2) {
            p.x -= side * .00135f * cs;
            p.y += .006f;
            p.z -= .00135f * sn;
          }
          weights = wt(fore, upper, smooth((.186f - y) / .081f));
        }
        const float cuff = smooth((run - .92f) / .08f);
        next[i] = pattern.node(p, cloth * (1 - .045f * cuff), weights);
      }
      std::vector<float> next_distance(count + 1);
      float next_run = previous_run;
      for (int i = 0; i < count; ++i) {
        next_run +=
            length(pattern.points[next[i]].p - pattern.points[previous[i]].p) *
            b.h / count;
        next_distance[i + 1] =
            next_distance[i] + length(pattern.points[next[(i + 1) % count]].p -
                                      pattern.points[next[i]].p) *
                                   b.h;
      }
      for (int i = 0; i < count; ++i) {
        const int k = (i + 1) % count;
        pattern.quad(previous[i], next[i], next[k], previous[k],
                     uv(distance[i], previous_run),
                     uv(next_distance[i], next_run),
                     uv(next_distance[i + 1], next_run),
                     uv(distance[i + 1], previous_run));
      }
      previous = std::move(next);
      distance = std::move(next_distance);
      previous_run = next_run;
    }
  }

  // A continuous turned hem is a narrow return, not a solid body end cap.
  previous = torso.front();
  for (int row = 1; row <= 2; ++row) {
    std::vector<std::uint32_t> next(columns);
    for (int i = 0; i < columns; ++i) {
      const P &outer = pattern.points[torso.front()[i]];
      const Vec3 radial = normalize(Vec3{outer.p.x, 0, outer.p.z});
      const Vec3 p =
          outer.p - radial * .0014f + Vec3{0, row == 1 ? -.0003f : .007f, 0};
      next[i] = pattern.node(p, cloth * .95f, outer.w);
    }
    for (int i = 0; i < columns; ++i) {
      const int k = (i + 1) % columns, chart = i / (columns / 2);
      const float u0 = circumference[i] - circumference[chart * columns / 2];
      const float u1 =
          circumference[i + 1] - circumference[chart * columns / 2];
      pattern.quad(previous[i], previous[k], next[k], next[i],
                   uv(u0, .01f * (row - 1)), uv(u1, .01f * (row - 1)),
                   uv(u1, .01f * row), uv(u0, .01f * row));
    }
    previous = std::move(next);
  }

  // Cloth closure overlaps the continuous underlap. Its edge rolls toward the
  // body and its color remains the role's pale service fabric, with no painted
  // shadow.
  const int closure_rows = rows;
  const auto closure_y = [&](int row) {
    return std::clamp(height(row), .014f, .327f);
  };
  constexpr std::array<float, 5> closure_x{.008f, .004f, 0.f, -.006f, -.012f};
  std::vector<std::array<std::uint32_t, 5>> closure(closure_rows + 1);
  for (int j = 0; j <= closure_rows; ++j)
    for (int i = 0; i <= 4; ++i) {
      const float y = closure_y(j);
      const float u = float(i) / 4;
      const auto fit = front_fit(closure_x[i], y);
      const Vec3 p =
          fit.point + fit.normal * (.0012f + .0011f * std::sin(pi * u));
      closure[j][i] = pattern.node(p, cloth * .97f, fit.weights);
    }
  for (int j = 0; j < closure_rows; ++j)
    for (int i = 0; i < 4; ++i) {
      const float u0 = (.008f - closure_x[i]) * b.h;
      const float u1 = (.008f - closure_x[i + 1]) * b.h;
      const float v0 = (closure_y(j) - .014f) * b.h;
      const float v1 = (closure_y(j + 1) - .014f) * b.h;
      pattern.quad(closure[j][i], closure[j + 1][i], closure[j + 1][i + 1],
                   closure[j][i + 1], uv(u0, v0), uv(u0, v1), uv(u1, v1),
                   uv(u1, v0));
    }
  // Small, restrained fasteners have the same continuously blended torso
  // weights as the underlying cloth, including at spine/chest transitions.
  for (float y : {.054f, .107f, .160f, .213f, .266f, .316f}) {
    const auto fit = front_fit(-.002f, y);
    const Vec3 center = fit.point + fit.normal * .0040f;
    const Vec3 across = normalize(cross(Vec3{0, 1, 0}, fit.normal));
    const Vec3 up = normalize(cross(fit.normal, across));
    const auto weights = fit.weights;
    const int count = b.near ? 12 : 8;
    const Vec3 color = cloth * 1.06f;
    const auto front =
        pattern.node(center + fit.normal * .001f, color, weights);
    const auto back =
        pattern.node(center - fit.normal * .0008f, color, weights);
    std::vector<std::uint32_t> front_ring(count), back_ring(count);
    const auto disk_uv = [&](int i) {
      const float a = 2 * pi * float(i) / count;
      return character_surface_uv(S::Rubber, .5f + .45f * std::cos(a),
                                  .5f + .45f * std::sin(a));
    };
    const Vec2 center_uv = character_surface_uv(S::Rubber, .5f, .5f);
    for (int i = 0; i < count; ++i) {
      const float a = 2 * pi * float(i) / count;
      front_ring[i] =
          pattern.node(center + across * (.0021f * std::cos(a)) +
                           up * (.0021f * std::sin(a)) + fit.normal * .00035f,
                       color, weights);
      back_ring[i] =
          pattern.node(center + across * (.0020f * std::cos(a)) +
                           up * (.0020f * std::sin(a)) - fit.normal * .00045f,
                       color, weights);
    }
    for (int i = 0; i < count; ++i) {
      const int k = (i + 1) % count;
      pattern.triangle(front, front_ring[i], front_ring[k], center_uv,
                       disk_uv(i), disk_uv(i + 1));
      pattern.triangle(back, back_ring[k], back_ring[i], center_uv,
                       disk_uv(i + 1), disk_uv(i));
      const float u0 = float(i) / count, u1 = float(i + 1) / count;
      pattern.quad(front_ring[i], back_ring[i], back_ring[k], front_ring[k],
                   character_surface_uv(S::Rubber, u0, .02f),
                   character_surface_uv(S::Rubber, u0, .06f),
                   character_surface_uv(S::Rubber, u1, .06f),
                   character_surface_uv(S::Rubber, u1, .02f));
    }
  }
  // Lia's narrow four-in-hand tie follows the actual shirt triangles and uses
  // their torso blend. Its rounded closed section has real edge thickness.
  const Vec3 tie_color = b.m.appearance.accent * .62f;
  const int tie_rows = b.near ? 24 : 10;
  b.grid(b.near ? 12 : 8, tie_rows, [&](float u, float v) {
    const float y = lerp(.112f, .338f, v);
    const float a = 2 * pi * u;
    const float width = (.0010f + .0072f * smooth(v / .09f)) *
                        (1 - .23f * smooth((v - .45f) / .55f)) *
                        (1 - .48f * smooth((v - .88f) / .12f));
    const auto fit = front_fit(0, y);
    const Vec3 across = normalize(cross(Vec3{0, 1, 0}, fit.normal));
    const Vec3 center =
        fit.point + fit.normal * (.0051f + .0014f * std::sin(pi * v));
    return P{center + across * (width * std::cos(a)) +
                 fit.normal * (.0010f * std::sin(a)),
             tie_color, fit.weights, character_surface_uv(S::Shirt, u, v)};
  });
  // Tapered knot sits beneath the collar leaves and hides the tie's upper cut.
  b.grid(b.near ? 16 : 8, b.near ? 8 : 4, [&](float u, float v) {
    const float a = 2 * pi * u, y = lerp(.334f, .356f, v);
    const float width =
        lerp(.0043f, .0070f, smooth(v)) + .0007f * std::sin(pi * v);
    const float z = front_point(0, std::min(y, .341f)).z + .0060f;
    return P{{width * std::cos(a) + .00045f * std::sin(pi * v), y,
              z + .0032f * std::sin(a)},
             tie_color * (.95f + .05f * v),
             wt(B::Chest),
             character_surface_uv(S::Shirt, u, v)};
  });
  // Small role badge: fitted rounded rectangle, dark frame, pale inset and
  // restrained inscription marks. The geometry is distinct from a floating box.
  auto badge_shell = [&](float half_x, float half_y, float z_offset,
                         float depth, Vec3 color, S surface) {
    b.grid(b.near ? 24 : 12, 2, [&](float u, float v) {
      const float a = u == 1.f ? 0.f : -2 * pi * u;
      const float x =
          -.056f +
          half_x * std::copysign(std::pow(std::fabs(std::cos(a)), .34f),
                                 std::cos(a));
      const float y =
          .258f + half_y * std::copysign(std::pow(std::fabs(std::sin(a)), .42f),
                                         std::sin(a));
      const auto fit = front_fit(x, y);
      return P{fit.point + fit.normal * (z_offset + depth * v), color,
               fit.weights, character_surface_uv(surface, u, v)};
    });
  };
  badge_shell(.022f, .0096f, .0008f, .0022f, {.08f, .09f, .09f}, S::Rubber);
  badge_shell(.0197f, .0074f, .0031f, .0006f, {.75f, .76f, .68f}, S::Shirt);
  if (b.near) {
    for (int row = 0; row < 2; ++row) {
      const float y = .2604f - .0044f * row;
      const float half = row == 0 ? .013f : .0085f;
      b.grid(
          1, 1,
          [&](float u, float v) {
            const float x = -.056f + lerp(half, -half, u);
            const float yy = y + lerp(-.00045f, .00045f, v);
            const auto fit = front_fit(x, yy);
            const Vec3 p = fit.point + fit.normal * .00385f;
            return P{p,
                     {.12f, .13f, .12f},
                     fit.weights,
                     character_surface_uv(S::Rubber, u, v)};
          },
          false, false);
    }
  }

  if (b.near)
    for (float side : {-1.f, 1.f})
      b.ellipsoid({side * .190f, .015f, .026f}, {.0013f, .0022f, .0022f},
                  cloth * 1.07f, S::Rubber,
                  side < 0 ? B::LeftForearm : B::RightForearm, 6, 2);
  pattern.finish();
}
} // namespace fury
