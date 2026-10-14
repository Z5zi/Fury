#pragma once

#include "character_sculpt.hpp"
#include <tuple>

namespace fury::cass_jacket_detail {
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

// Fit an overlay's complete triangles, not merely its grid vertices, above
// the actual support triangles in +Z. Clipping in XY finds every extremum of
// the difference between two linear facets. Raising all three corners by the
// maximum deficit preserves the overlay's shared edges and source topology.
inline void fit_front_overlay(
    Sculpt &b, const std::vector<std::array<unsigned, 3>> &triangles,
    const std::function<Vec3(unsigned)> &point,
    const std::function<void(unsigned, float)> &lift,
    std::size_t support_begin, std::size_t support_end, float clearance,
    bool teal_only = false) {
  const auto &mesh = b.m.bind_mesh;
  const auto lo = character_surface_uv(S::Jacket, 0, 0);
  const auto hi = character_surface_uv(S::Jacket, 1, 1);
  const auto cross2 = [](Vec3 a, Vec3 c) { return a.x * c.y - a.y * c.x; };
  struct Support { std::array<Vec3, 3> p; float x0, x1, y0, y1, den; };
  std::vector<Support> supports;
  for (std::size_t i = support_begin; i < support_end; i += 3) {
    const auto &v = mesh.vertices[mesh.indices[i]];
    if (v.uv.x < lo.x || v.uv.x > hi.x || v.uv.y < lo.y || v.uv.y > hi.y ||
        (teal_only && v.color.z > 1.2f * v.color.y)) continue;
    Support s{};
    for (int k = 0; k < 3; ++k) s.p[k] = mesh.vertices[mesh.indices[i + k]].position * (1 / b.h);
    s.den = cross2(s.p[1] - s.p[0], s.p[2] - s.p[0]);
    if (std::fabs(s.den) < 1e-12f) continue;
    s.x0 = std::min({s.p[0].x, s.p[1].x, s.p[2].x});
    s.x1 = std::max({s.p[0].x, s.p[1].x, s.p[2].x});
    s.y0 = std::min({s.p[0].y, s.p[1].y, s.p[2].y});
    s.y1 = std::max({s.p[0].y, s.p[1].y, s.p[2].y});
    supports.push_back(s);
  }
  std::map<unsigned, float> lifts;
  for (const auto &ids : triangles) {
    const std::array<Vec3, 3> p{point(ids[0]), point(ids[1]), point(ids[2])};
    const float x0 = std::min({p[0].x, p[1].x, p[2].x}), x1 = std::max({p[0].x, p[1].x, p[2].x});
    const float y0 = std::min({p[0].y, p[1].y, p[2].y}), y1 = std::max({p[0].y, p[1].y, p[2].y});
    float needed = 0;
    for (const auto &s : supports) {
      if (x1 < s.x0 || s.x1 < x0 || y1 < s.y0 || s.y1 < y0) continue;
      std::vector<Vec3> polygon{p.begin(), p.end()};
      const float sign = s.den > 0 ? 1.f : -1.f;
      for (int edge = 0; edge < 3 && !polygon.empty(); ++edge) {
        std::vector<Vec3> clipped;
        const Vec3 a = s.p[edge], d = s.p[(edge + 1) % 3] - a;
        for (std::size_t j = 0; j < polygon.size(); ++j) {
          const Vec3 first = polygon[j], last = polygon[(j + 1) % polygon.size()];
          const float f = sign * cross2(d, first - a), l = sign * cross2(d, last - a);
          const bool inside_first = f >= 0, inside_last = l >= 0;
          if (inside_first) clipped.push_back(first);
          if (inside_first != inside_last) clipped.push_back(first + (last - first) * (f / (f - l)));
        }
        polygon = std::move(clipped);
      }
      for (const Vec3 p : polygon) {
        const float u = cross2(p - s.p[0], s.p[2] - s.p[0]) / s.den;
        const float v = cross2(s.p[1] - s.p[0], p - s.p[0]) / s.den;
        const float z = s.p[0].z + u * (s.p[1].z - s.p[0].z) + v * (s.p[2].z - s.p[0].z);
        needed = std::max(needed, z + clearance - p.z);
      }
    }
    if (needed > 0) for (auto id : ids) lifts[id] = std::max(lifts[id], needed + .00002f);
  }
  for (const auto &entry : lifts) lift(entry.first, entry.second);
}

inline CharacterInfluence body_weight(float y) {
  if (y < .14f)
    return wt(B::Spine, B::Pelvis, smooth((y - .025f) / .115f));
  return wt(B::Chest, B::Spine, smooth((y - .14f) / .095f));
}

inline float section(float y, const std::array<float, 6> &values) {
  constexpr std::array<float, 6> levels{.012f, .07f, .14f, .22f, .28f, .333f};
  for (int k = 0; k < 5; ++k)
    if (y <= levels[k + 1])
      return lerp(values[k], values[k + 1],
                  smooth((y - levels[k]) / (levels[k + 1] - levels[k])));
  return values.back();
}

inline Vec3 body_point(float a, float y) {
  const float cs = std::cos(a), sn = std::sin(a);
  const float rx = section(y, {.107f, .101f, .091f, .104f, .111f, .108f});
  const float rz = section(y, {.074f, .071f, .060f, .067f, .069f, .063f});
  // Local broad pulls, rather than equally spaced circumferential corrugation.
  const float front = std::max(0.f, sn), back = std::max(0.f, -sn);
  const float fold = .0018f * g(y, .075f + .039f * cs, .027f) * front * front -
                     .0012f * g(y, .115f + .031f * cs, .023f) * front * front +
                     .0015f * g(y, .205f + .055f * cs, .029f) * front * front +
                     .0012f * g(y, .162f - .033f * cs, .047f) * back * back +
                     .0010f * g(y, .035f, .024f) * std::sin(3 * a + .7f);
  return {(rx + fold) * cs, y,
          (rz + fold) * sn + .006f * front * front +
          .004f * g(y, .069f, .052f) * front * front - .002f * back * back};
}

inline Vec3 armhole_point(float a, float y) {
  constexpr float half_angle = 3 * pi / 16;
  const float side = std::cos(a) < 0 ? -1.f : 1.f;
  const float u = std::sin(a) / std::sin(half_angle);
  const float v = (y - .269f) / .064f;
  const float uc = std::clamp(u, -1.f, 1.f);
  const float vc = std::clamp(v, -1.f, 1.f);
  const float U = uc * std::sqrt(1 - .5f * vc * vc);
  const float V = vc * std::sqrt(1 - .5f * uc * uc);
  float ac = std::asin(uc * std::sin(half_angle));
  if (side < 0)
    ac = pi - ac;
  const Vec3 original = body_point(ac, .269f + .064f * vc);
  const Vec3 round{side * (.094f + .015f * V + .009f * U * U),
                   .269f + .064f * V, .047f * U + .002f};
  const float support = (1 - smooth((std::fabs(u) - 1) / .72f)) *
                        (1 - smooth((std::fabs(v) - 1) / .65f));
  return body_point(a, y) + (round - original) * support;
}

inline CharacterInfluence torso_weight(float a, float y) {
  constexpr float half_angle = 3 * pi / 16;
  const float u = std::sin(a) / std::sin(half_angle);
  const float v = (y - .269f) / .064f;
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
} // namespace fury::cass_jacket_detail

namespace fury {
// Original Cass garment construction. All dimensions are normalized by height,
// +Z faces forward and the pelvis is the origin. No reference mesh, texture or
// commercial garment pattern is used. The supplied query is the source neck
// radial intersection, measured from (0,y,neck_center.z).
inline void make_cass_jacket(
    character_detail::Sculpt &b,
    const std::function<float(float, float, float)> &anatomy_radius,
    Vec3 neck_center) {
  using namespace character_detail;
  using namespace cass_jacket_detail;
  Pattern pattern{b, {}, {}, {}};
  const Vec3 cloth = b.m.appearance.shirt;
  const int columns = b.near ? 64 : 32;
  const int below = b.near ? 12 : 5, hole_rows = b.near ? 8 : 4;
  const int rows = below + hole_rows, half_hole = b.near ? 6 : 3;
  const auto angle = [&](int column) {
    return pi * .5f + 2 * pi * float(column % columns) / columns;
  };
  const auto height = [&](int row) {
    return row <= below ? lerp(.012f, .205f, float(row) / below)
                        : lerp(.205f, .333f, float(row - below) / hole_rows);
  };
  const auto uv = [&](float metres_u, float metres_v) {
    return character_surface_uv(S::Jacket, .025f + metres_u / .75f,
                                .025f + metres_v / .70f);
  };
  std::vector<std::vector<std::uint32_t>> torso(
      rows + 1, std::vector<std::uint32_t>(columns));
  for (int j = 0; j <= rows; ++j)
    for (int i = 0; i < columns; ++i) {
      const float a = angle(i);
      float y = height(j);
      // Rounded front quarters give the seated thighs room under the hem.
      // This is authored garment cut, not pose-dependent cloth correction.
      if (j <= below)
        y = lerp(.012f + .040f * sq(std::max(0.f, std::sin(a))),
                 .205f, float(j) / below);
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
    const float y = neck_center.y - .002f - .007f * front +
                    .003f * std::max(0.f, -std::sin(a));
    const float r = anatomy_radius(y, a, neck_center.z) + .0058f;
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
        p += radial * (std::max(0.f, skin + .0048f - r) / r);
      }
      auto weights = torso_weight(a, .333f);
      // The clavicle-side yoke supports the turned collar and follows the
      // chest. Transfer sleeve influence through the outer shoulder only;
      // retaining it under the collar makes carrying-bind retargets deform
      // the coarse support triangle differently from its fitted cloth rim.
      weights.third_weight *= 1 - smooth(v / .5f);
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
  const auto yoke_sample = [&](float x, float z) {
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
    if (highest < 0)
      throw std::runtime_error(
          "Cass turned collar must rest above the actual yoke");
    return P{{x, highest, z}, cloth, fitted_weights, {}};
  };

  // A turned, open-front collar, with a narrow stand and a finite folded edge.
  // Query below the jaw; a collar is supported by the chest, not the head.
  const int collar_first = b.near ? 3 : 2;
  const int collar_last = columns - collar_first;
  const int collar_rows = b.near ? 5 : 3;
  std::vector<Vec3> collar_rest(columns);
  std::vector<CharacterInfluence> collar_rest_weights(columns);
  for (int i = collar_first; i <= collar_last; ++i) {
    const float a = angle(i);
    const Vec3 base = pattern.points[neckline_nodes[i]].p;
    const float radius = length(Vec3{base.x, 0, base.z - neck_center.z});
    const float tip = g(float(i), float(collar_first), 2.5f) +
                      g(float(i), float(collar_last), 2.5f);
    const float outer_radius = radius + .014f + .003f * tip;
    Vec3 edge{outer_radius * std::cos(a), 0,
              neck_center.z + outer_radius * std::sin(a)};
    const P support = yoke_sample(edge.x, edge.z);
    edge.y = support.p.y + .0033f;
    collar_rest[i] = edge;
    collar_rest_weights[i] = support.w;
  }
  std::array<std::vector<std::uint32_t>, 2> collar_ends;
  std::vector<std::vector<std::uint32_t>> collar_rings{previous};
  collar_ends[0].push_back(previous[collar_first]);
  collar_ends[1].push_back(previous[collar_last]);
  for (int j = 1; j <= collar_rows; ++j) {
    const float t = float(j) / collar_rows;
    std::vector<std::uint32_t> next = previous;
    for (int i = collar_first; i <= collar_last; ++i) {
      const float a = angle(i);
      const Vec3 base = pattern.points[neckline_nodes[i]].p;
      const Vec3 radial{std::cos(a), 0, std::sin(a)};
      const Vec3 edge = collar_rest[i];
      const Vec3 control1 = base + radial * .0008f + Vec3{0, .011f, 0};
      const Vec3 control2 = edge - radial * .006f + Vec3{0, .003f, 0};
      const float s = 1 - t;
      const Vec3 p = base * (s * s * s) + control1 * (3 * s * s * t) +
                     control2 * (3 * s * t * t) + edge * (t * t * t);
      auto weights = collar_rest_weights[i];
      weights.third_weight *= smooth(t);
      weights.first_weight = 1 - weights.third_weight;
      next[i] = pattern.node(
          p, (b.m.appearance.accent * .67f) * (.96f + .04f * (1 - t)), weights);
    }
    for (int i = collar_first; i < collar_last; ++i) {
      const float u0 = float(i - collar_first) / (collar_last - collar_first);
      const float u1 =
          float(i + 1 - collar_first) / (collar_last - collar_first);
      const float v0 = float(j - 1) / collar_rows;
      pattern.quad(previous[i], next[i], next[i + 1], previous[i + 1],
                   uv(.40f * u0, .053f * v0), uv(.40f * u0, .053f * t),
                   uv(.40f * u1, .053f * t), uv(.40f * u1, .053f * v0));
    }
    collar_ends[0].push_back(next[collar_first]);
    collar_ends[1].push_back(next[collar_last]);
    collar_rings.push_back(next);
    previous = std::move(next);
  }
  // Turn the exposed collar edge back under itself, including both cut ends.
  std::map<std::pair<int, int>, std::uint32_t> collar_inside;
  const auto collar_inner = [&](int row, int column) {
    // The finite cut edge tapers into its sewn root; an offset root would
    // penetrate the yoke during a strongly retargeted shoulder pose.
    if (row == 0) return collar_rings[0][column];
    const auto key = std::make_pair(row, column);
    const auto found = collar_inside.find(key);
    if (found != collar_inside.end())
      return found->second;
    const P p = pattern.points[collar_rings[row][column]];
    const Vec3 along =
        pattern.points[collar_rings[row][std::min(collar_last, column + 1)]].p -
        pattern.points[collar_rings[row][std::max(collar_first, column - 1)]].p;
    const Vec3 across =
        pattern.points[collar_rings[std::min(collar_rows, row + 1)][column]].p -
        pattern.points[collar_rings[std::max(0, row - 1)][column]].p;
    const auto result =
        pattern.node(p.p - normalize(cross(across, along)) * .0012f,
                     (b.m.appearance.accent * .67f) * .89f, p.w);
    collar_inside.emplace(key, result);
    return result;
  };
  std::vector<std::uint32_t> collar_edge;
  for (int i = collar_first; i <= collar_last; ++i)
    collar_edge.push_back(previous[i]);
  std::uint32_t collar_last_inner = 0;
  for (std::size_t i = 0; i < collar_edge.size(); ++i) {
    const auto inner = collar_inner(collar_rows, collar_first + int(i));
    if (i) {
      const float u0 = .40f * float(i - 1) / (collar_edge.size() - 1);
      const float u1 = .40f * float(i) / (collar_edge.size() - 1);
      pattern.quad(collar_edge[i - 1], collar_last_inner, inner, collar_edge[i],
                   uv(u0, .053f), uv(u0, .055f), uv(u1, .055f), uv(u1, .053f));
    }
    collar_last_inner = inner;
  }
  for (int end = 0; end < 2; ++end) {
    const auto &edge = collar_ends[end];
    std::uint32_t last_inner = 0;
    for (std::size_t i = 0; i < edge.size(); ++i) {
      const auto inner =
          collar_inner(int(i), end == 0 ? collar_first : collar_last);
      if (i) {
        const float v0 = .053f * float(i - 1) / collar_rows;
        const float v1 = .053f * float(i) / collar_rows;
        if (i == 1) {
          if (end == 0)
            pattern.triangle(edge[0], inner, edge[i], uv(0, v0), uv(.002f, v1), uv(0, v1));
          else
            pattern.triangle(edge[i], inner, edge[0], uv(0, v1), uv(.002f, v1), uv(0, v0));
        } else if (end == 0)
          pattern.quad(edge[i - 1], last_inner, inner, edge[i], uv(0, v0),
                       uv(.002f, v0), uv(.002f, v1), uv(0, v1));
        else
          pattern.quad(edge[i], inner, last_inner, edge[i - 1], uv(0, v1),
                       uv(.002f, v1), uv(.002f, v0), uv(0, v0));
      }
      last_inner = inner;
    }
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
      phase[i] = std::atan2((p.z - .002f) / .047f, (p.y - .269f) / .064f);
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
    const int cap_rows = b.near ? 3 : 2, lower_rows = b.near ? 12 : 4;
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
          const Vec3 end{side * (.130f + .033f * cs), .216f + .0035f * cs,
                         .006f + .036f * sn};
          p = start.p * (1 - t) + end * t;
          p.x += side * .014f * std::sin(pi * t) * (.5f + .5f * cs);
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
          const float y = lerp(.216f, -.040f, t);
          const float elbow_t = smooth((.216f - y) / .096f);
          const float wrist_t = smooth((.12f - y) / .16f);
          const float x = lerp(.130f, .139f, elbow_t) + .024f * wrist_t;
          const float z = lerp(.006f, .008f, elbow_t) + .010f * wrist_t;
          const float front = sq(std::max(0.f, sn));
          const float back = sq(std::max(0.f, -sn));
          const float fold = .0028f * g(y, .114f + .014f * cs, .020f) * front -
                             .0017f * g(y, .140f - .010f * cs, .018f) * front +
                             .0018f * g(y, .080f - .013f * cs, .017f) * front +
                             .0014f * g(y, .127f, .037f) * back;
          float r = lerp(.033f, .027f, elbow_t) - .006f * wrist_t + fold;
          r += .0009f * smooth((-.017f - y) / .012f);
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
          weights = wt(fore, upper, smooth((.163f - y) / .081f));
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
  // Keep the physically sewn hem points exact but crease their rendering
  // normals where the outer cloth turns sharply into the narrow return.
  for (auto &node : previous) {
    const P p = pattern.points[node];
    node = pattern.node(p.p, p.c, p.w);
  }
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
    if (row == 1) for (auto &node : previous) {
      const P p = pattern.points[node];
      node = pattern.node(p.p, p.c, p.w);
    }
  }

  const auto front_point = [&](float x, float y) {
    // Fit to the actual LOD triangles, not just their smooth generating
    // function: a coarse panel may bridge above the analytic surface.
    float front = -1.f;
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
      if (u >= -1e-6f && v >= -1e-6f && u + v <= 1.000001f)
        front = std::max(front, a.z + u * (c.z - a.z) + v * (d.z - a.z));
    }
    if (front < -.1f)
      throw std::runtime_error(
          "Cass pocket/closure must fit an actual body panel");
    return Vec3{x, y, front};
  };
  // Cloth closure overlaps the continuous underlap. Its edge rolls toward the
  // body and its color remains the identity blue-gray, with no painted shadow.
  const int closure_rows = b.near ? 12 : 6;
  const auto closure_y = [&](int row) {
    return lerp(.054f, .173f, float(row) / closure_rows);
  };
  constexpr std::array<float, 5> closure_x{.008f, .004f, 0.f, -.006f, -.012f};
  std::vector<std::array<std::uint32_t, 5>> closure(closure_rows + 1);
  for (int j = 0; j <= closure_rows; ++j)
    for (int i = 0; i <= 4; ++i) {
      const float y = closure_y(j);
      const float u = float(i) / 4;
      Vec3 p = front_point(closure_x[i], y);
      p.z += .0009f + .0012f * std::sin(pi * u);
      closure[j][i] = pattern.node(p, cloth * .97f, body_weight(y));
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
  // Two shallow, softly rounded utility pockets. Three edges lie against the
  // body; the mouth stands away with a turned lip and real interior depth.
  for (float side : {-1.f, 1.f}) {
    const int nx = b.near ? 12 : 6, ny = b.near ? 3 : 2;
    std::vector<std::vector<std::uint32_t>> pocket(
        ny + 1, std::vector<std::uint32_t>(nx + 1));
    for (int j = 0; j <= ny; ++j)
      for (int i = 0; i <= nx; ++i) {
        const float u = float(i) / nx, v = float(j) / ny;
        const float round = .001f * sq(2 * u - 1) * (1 - v);
        const float x = side * lerp(.026f + round, .084f - round, u);
        const float y = lerp(.094f + .008f * u, .103f + .008f * u, v);
        Vec3 p = front_point(x, y);
        p.z += (b.near ? .0015f : .0030f) +
               .0016f * std::sin(pi * u) * smooth(v / .45f);
        pocket[j][i] = pattern.node(p, cloth * .98f, body_weight(y));
      }
    for (int j = 0; j < ny; ++j)
      for (int i = 0; i < nx; ++i) {
        const float u0 = .10f * float(i) / nx, u1 = .10f * float(i + 1) / nx;
        const float v0 = .15f * float(j) / ny, v1 = .15f * float(j + 1) / ny;
        if (side < 0)
          pattern.quad(pocket[j][i], pocket[j + 1][i], pocket[j + 1][i + 1],
                       pocket[j][i + 1], uv(u0, v0), uv(u0, v1), uv(u1, v1),
                       uv(u1, v0));
        else
          pattern.quad(pocket[j][i + 1], pocket[j + 1][i + 1], pocket[j + 1][i],
                       pocket[j][i], uv(u1, v0), uv(u1, v1), uv(u0, v1),
                       uv(u0, v0));
      }
    // The mouth folds sharply inward. Give its return its own normal corners
    // at the exact same position/influence, rather than averaging the broad
    // outward pocket panel through a greater-than-90-degree cloth turn.
    std::vector<std::uint32_t> inner(nx + 1), lip_outer(nx + 1);
    for (int i = 0; i <= nx; ++i) {
      const P p = pattern.points[pocket.back()[i]];
      lip_outer[i] = pattern.node(p.p, p.c, p.w);
      Vec3 q = p.p + Vec3{0, -.002f * std::sin(pi * float(i) / nx), -.0010f};
      if (i == 0 || i == nx) {
        q = front_point(p.p.x, p.p.y);
        q.z += b.near ? .00025f : .0015f;
      }
      inner[i] = pattern.node(q, cloth * .92f, p.w);
    }
    for (int i = 0; i < nx; ++i) {
      const float u0 = .10f * float(i) / nx, u1 = .10f * float(i + 1) / nx;
      if (side < 0)
        pattern.quad(lip_outer[i], inner[i], inner[i + 1], lip_outer[i + 1],
                     uv(u0, .15f), uv(u0, .155f), uv(u1, .155f), uv(u1, .15f));
      else
        pattern.quad(lip_outer[i + 1], inner[i + 1], inner[i], lip_outer[i],
                     uv(u1, .15f), uv(u1, .155f), uv(u0, .155f), uv(u0, .15f));
    }
    std::vector<std::uint32_t> sewn_edge;
    for (int j = ny; j >= 0; --j)
      sewn_edge.push_back(pocket[j][nx]);
    for (int i = nx - 1; i >= 0; --i)
      sewn_edge.push_back(pocket[0][i]);
    for (int j = 1; j <= ny; ++j)
      sewn_edge.push_back(pocket[j][0]);
    if (side > 0)
      std::reverse(sewn_edge.begin(), sewn_edge.end());
    std::uint32_t last_inner = 0;
    float length_u = 0;
    for (std::size_t i = 0; i < sewn_edge.size(); ++i) {
      const P p = pattern.points[sewn_edge[i]];
      Vec3 foot = front_point(p.p.x, p.p.y);
      foot.z += b.near ? .00025f : .0015f;
      const auto inner_node = sewn_edge[i] == pocket.back()[0] ? inner[0]
                              : sewn_edge[i] == pocket.back()[nx]
                                  ? inner[nx]
                                  : pattern.node(foot, cloth * .97f, p.w);
      if (i) {
        const float next_u =
            length_u + length(p.p - pattern.points[sewn_edge[i - 1]].p) * b.h;
        pattern.quad(sewn_edge[i - 1], last_inner, inner_node, sewn_edge[i],
                     uv(length_u, 0), uv(length_u, .003f), uv(next_u, .003f),
                     uv(next_u, 0));
        length_u = next_u;
      }
      last_inner = inner_node;
    }
  }
  // Small, restrained fasteners have the same continuously blended torso
  // weights as the underlying cloth, including at spine/chest transitions.
  for (float y : {.116f}) {
    const Vec3 center = front_point(-.002f, y) + Vec3{0, 0, .0040f};
    const auto weights = body_weight(y);
    const int count = b.near ? 12 : 8;
    const Vec3 color = cloth * .31f;
    const auto front = pattern.node(center + Vec3{0, 0, .001f}, color, weights);
    const auto back = pattern.node(center - Vec3{0, 0, .0008f}, color, weights);
    std::vector<std::uint32_t> front_ring(count), back_ring(count);
    const auto disk_uv = [&](int i) {
      const float a = 2 * pi * float(i) / count;
      return character_surface_uv(S::Rubber, .5f + .45f * std::cos(a),
                                  .5f + .45f * std::sin(a));
    };
    const Vec2 center_uv = character_surface_uv(S::Rubber, .5f, .5f);
    for (int i = 0; i < count; ++i) {
      const float a = 2 * pi * float(i) / count;
      front_ring[i] = pattern.node(
          center + Vec3{.0027f * std::cos(a), .0027f * std::sin(a), .00035f},
          color, weights);
      back_ring[i] = pattern.node(
          center + Vec3{.00255f * std::cos(a), .00255f * std::sin(a), -.00045f},
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

  // Original rolled lapels: a quiet mauve facing with real thickness and a
  // bowed roll ridge. Each rim returns under the facing into the teal chest.
  for (float side : {-1.f, 1.f}) {
    const int nx = b.near ? 6 : 4, ny = b.near ? 18 : 6;
    std::array<std::vector<std::vector<unsigned>>, 2> layer;
    for (auto &q : layer)
      q.resize(ny + 1, std::vector<unsigned>(nx + 1));
    const Vec3 facing = b.m.appearance.accent * .67f;
    for (int j = 0; j <= ny; ++j)
      for (int i = 0; i <= nx; ++i) {
        const float v = float(j) / ny, u = float(i) / nx;
        const float y = lerp(.177f, .339f, v) - .017f * u * smooth((v - .68f) / .32f);
        const float inner = lerp(.011f, .019f, v);
        const float width = .0015f + .033f * std::sin(pi * .70f * v);
        const float x = side * (inner + width * u);
        Vec3 p = front_point(x, y);
        const float roll = .0038f * std::sin(pi * u) * std::sin(pi * .86f * v);
        p.z += (b.near ? .0058f : .0065f) + roll;
        const auto w = body_weight(y);
        layer[0][j][i] = pattern.node(p, facing, w);
        p.z -= .00125f;
        layer[1][j][i] = pattern.node(p, facing * .93f, w);
      }
    std::vector<std::array<unsigned, 3>> support_faces;
    std::map<unsigned, unsigned> paired;
    for (int j = 0; j <= ny; ++j) for (int i = 0; i <= nx; ++i)
      paired[layer[1][j][i]] = layer[0][j][i];
    for (int j = 0; j < ny; ++j) for (int i = 0; i < nx; ++i) {
      const auto &q = layer[1];
      support_faces.push_back({q[j][i], q[j][i + 1], q[j + 1][i + 1]});
      support_faces.push_back({q[j][i], q[j + 1][i + 1], q[j + 1][i]});
    }
    fit_front_overlay(b, support_faces,
        [&](unsigned id) { return pattern.points[id].p; },
        [&](unsigned id, float dz) {
          pattern.points[id].p.z += dz;
          pattern.points[paired.at(id)].p.z += dz;
        }, body_triangle_begin, yoke_triangle_end, .0045f);
    const auto tex = [&](int i, int j) {
      return character_surface_uv(S::Jacket, .10f + .15f * float(i) / nx,
                                  .05f + .5f * float(j) / ny);
    };
    for (int j = 0; j < ny; ++j)
      for (int i = 0; i < nx; ++i)
        for (int k = 0; k < 2; ++k) {
          auto &q = layer[k];
          bool flip = (side > 0) != (k == 1);
          if (flip)
            pattern.quad(q[j][i], q[j][i + 1], q[j + 1][i + 1], q[j + 1][i],
                         tex(i, j), tex(i + 1, j), tex(i + 1, j + 1),
                         tex(i, j + 1));
          else
            pattern.quad(q[j][i], q[j + 1][i], q[j + 1][i + 1], q[j][i + 1],
                         tex(i, j), tex(i, j + 1), tex(i + 1, j + 1),
                         tex(i + 1, j));
        }
    std::vector<std::pair<int, int>> edge;
    for (int i = 0; i < nx; ++i)
      edge.push_back({0, i});
    for (int j = 0; j < ny; ++j)
      edge.push_back({j, nx});
    for (int i = nx; i > 0; --i)
      edge.push_back({ny, i});
    for (int j = ny; j > 0; --j)
      edge.push_back({j, 0});
    if (side < 0)
      std::reverse(edge.begin(), edge.end());
    for (std::size_t t = 0; t < edge.size(); ++t) {
      auto [j, i] = edge[t];
      auto [jj, ii] = edge[(t + 1) % edge.size()];
      const auto crease = [&](unsigned id) {
        const auto p = pattern.points[id];
        return pattern.node(p.p, p.c, p.w);
      };
      pattern.quad(crease(layer[0][j][i]), crease(layer[1][j][i]),
                   crease(layer[1][jj][ii]), crease(layer[0][jj][ii]),
                   tex(i, j), tex(i, j), tex(ii, jj), tex(ii, jj));
    }
  }

  // A subdued raised rear center seam terminates at the weighted hem. It is
  // a closed finite cord sunk into the cloth, not an open floating rectangle.
  std::vector<Vec3> rear_seam;
  std::vector<float> rear_radius;
  for (int j = 0; j <= (b.near ? 14 : 7); ++j) {
    float t = float(j) / (b.near ? 14 : 7), y = lerp(.017f, .248f, t);
    Vec3 p = body_point(pi * 1.5f, y);
    p.z -= .00045f;
    rear_seam.push_back(p);
    rear_radius.push_back(.00055f * (.4f + .6f * std::sin(pi * t)));
  }
  b.tube(
      rear_seam, rear_radius, .6f, cloth * .94f, S::Jacket,
      [&](float t) { return body_weight(lerp(.017f, .248f, t)); },
      b.near ? 6 : 4);
  pattern.finish();
}
} // namespace fury
