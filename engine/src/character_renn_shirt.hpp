#pragma once

#include "character_sculpt.hpp"
#include <limits>
#include <tuple>

namespace fury::renn_shirt_detail {
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

inline CharacterInfluence
blend_weights(const std::array<CharacterInfluence, 3> &input,
              const std::array<float, 3> &factor) {
  std::array<float, character_bone_count> total{};
  for (int k = 0; k < 3; ++k) {
    auto w = input[k];
    total[w.first] += w.first_weight * factor[k];
    total[w.second] += (1 - w.first_weight - w.third_weight) * factor[k];
    total[w.third] += w.third_weight * factor[k];
  }
  std::array<unsigned, character_bone_count> order{};
  for (unsigned k = 0; k < order.size(); ++k)
    order[k] = k;
  std::stable_sort(order.begin(), order.end(),
                   [&](unsigned a, unsigned b) { return total[a] > total[b]; });
  float sum = std::max(1e-8f,
                       total[order[0]] + total[order[1]] + total[order[2]]),
        a = std::clamp(total[order[0]] / sum, 0.f, 1.f),
        c = std::clamp(total[order[2]] / sum, 0.f, 1 - a);
  return {std::uint8_t(order[0]), std::uint8_t(order[1]), a,
          std::uint8_t(order[2]), c};
}

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
  const float rx = section(y, {.083f, .092f, .099f, .121f, .126f, .123f});
  const float rz = section(y, {.027f, .055f, .063f, .076f, .075f, .064f});
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

inline Vec3 armhole_point(float a, float y, float half_angle = 3 * pi / 16) {
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
  const Vec3 round{side * (.110f + .016f * V + .009f * U * U),
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
} // namespace fury::renn_shirt_detail

namespace fury {
// Original Renn uniform-shirt fit, developed from Fury's sewn-armhole
// method. All dimensions are normalized by height, +Z faces forward and the
// pelvis is the origin. No reference mesh, texture or commercial garment
// pattern is used. The supplied query is the source neck radial intersection,
// measured from (0,y,neck_center.z).
struct RennUniformSupport {
  std::size_t begin, end;
};
inline RennUniformSupport
make_renn_shirt(character_detail::Sculpt &b,
                const std::function<float(float, float, float)> &anatomy_radius,
                Vec3 neck_center) {
  using namespace character_detail;
  using namespace renn_shirt_detail;
  Pattern pattern{b, {}, {}, {}};
  const Vec3 cloth{.16f, .22f, .28f};
  const int columns = b.near ? 64 : 24;
  const int below = b.near ? 8 : 3, hole_rows = b.near ? 8 : 3;
  const int rows = below + hole_rows, half_hole = b.near ? 6 : 3;
  const auto angle = [&](int column) {
    return pi * .5f + 2 * pi * float(column % columns) / columns;
  };
  const auto height = [&](int row) {
    // The coarse waist rings land on the actual pelvis/spine/chest blend
    // boundaries. A nearly coincident .140666 ring introduces a minute Chest
    // weight into the lower vest through interpolation and makes its hard-edge
    // copies refresh differently in the production pose cache.
    constexpr float far_waist[]{.012f, .070f, .140f, .205f};
    return row <= below ? (b.near ? lerp(.012f, .205f, float(row) / below)
                                  : far_waist[row])
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
          pattern.node(armhole_point(a, y, b.near ? 3 * pi / 16 : pi / 4),
                       cloth, torso_weight(a, y));
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
      const float neck =
          smooth((v - .35f) / .65f) * smooth((p.y - .333f) / .032f);
      if (neck > 0.f)
        weights = {std::uint8_t(B::Neck), std::uint8_t(B::Chest), neck};
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
          fitted_weights = blend_weights({b.m.influences[mesh.indices[i]],
                                          b.m.influences[mesh.indices[i + 1]],
                                          b.m.influences[mesh.indices[i + 2]]},
                                         {1 - u - v, u, v});
        }
      }
    }
    if (highest < 0 && required) {
      // The coarse yoke outline is authoritative. Project an intended collar
      // point to the nearest actual triangle edge when it lies just beyond a
      // polygon chord; retain that edge's actual skin weights and X/Z as well.
      float distance = std::numeric_limits<float>::infinity();
      P nearest{};
      for (std::size_t i = body_triangle_end; i < yoke_triangle_end; i += 3)
        for (int k = 0; k < 3; ++k) {
          unsigned ia = mesh.indices[i + k], ib = mesh.indices[i + (k + 1) % 3];
          Vec3 a = mesh.vertices[ia].position * (1 / b.h),
               c = mesh.vertices[ib].position * (1 / b.h), d = c - a;
          float dd = d.x * d.x + d.z * d.z;
          if (dd < 1e-12f)
            continue;
          float t =
              std::clamp(((x - a.x) * d.x + (z - a.z) * d.z) / dd, 0.f, 1.f);
          Vec3 q = a + d * t;
          float d2 = sq(q.x - x) + sq(q.z - z);
          if (d2 < distance) {
            distance = d2;
            nearest = {q,
                       cloth,
                       blend_weights({b.m.influences[ia], b.m.influences[ib],
                                      b.m.influences[ib]},
                                     {1 - t, t, 0}),
                       {}};
          }
        }
      if (distance > .000225f)
        throw std::runtime_error("Renn collar exceeds actual yoke support by "
                                 "more than .015 height units");
      return nearest;
    }
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
      throw std::runtime_error("Renn placket must fit an actual body panel");
    return FrontFit{{x, y, front}, normal, fitted};
  };
  const auto front_point = [&](float x, float y) {
    return front_fit(x, y).point;
  };
  const auto cloth_clearance = [&](Vec3 point, float margin) {
    const Vec3 origin{0, .265f, .012f};
    const Vec3 delta = point - origin, direction = normalize(delta);
    float distance = -1;
    CharacterInfluence weights =
        wt(B::Neck, B::Chest, smooth((point.y - .333f) / .032f));
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
          u + v > 1 + edge_tolerance || d <= 0.f || d <= distance)
        continue;
      distance = d;
      weights = blend_weights(
          {b.m.influences[ia], b.m.influences[ib], b.m.influences[ic]},
          {1 - u - v, u, v});
    }
    if (distance > 0 && length(delta) < distance + margin)
      point = origin + direction * (distance + margin);
    return P{point, cloth, weights, {}};
  };

  // Renn's service collar is sewn to a complete fitted stand. The leaves have
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
      next[i] = pattern.node(
          {r * std::cos(a), y, neck_center.z + r * std::sin(a)}, cloth,
          wt(B::Neck, B::Chest, smooth((y - .333f) / .032f)));
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
  const int leaf_rows = b.near ? 6 : 2;
  std::vector<Vec3> resting(columns);
  for (int i = collar_first; i <= collar_last; ++i) {
    const float a = angle(i);
    const Vec3 base = pattern.points[stand[i]].p;
    const Vec3 inner = pattern.points[neckline_nodes[i]].p;
    const Vec3 shoulder = pattern.points[torso.back()[i]].p;
    const float inner_radius =
        length(Vec3{inner.x, 0, inner.z - neck_center.z});
    const float outer_radius =
        length(Vec3{shoulder.x, 0, shoulder.z - neck_center.z});
    const float r = std::min(
        outer_radius - .002f,
        std::max(inner_radius + .004f,
                 length(Vec3{base.x, 0, base.z - neck_center.z}) + .014f));
    Vec3 edge{r * std::cos(a), 0, neck_center.z + r * std::sin(a)};
    const P support = yoke_sample(edge.x, edge.z);
    edge = support.p + Vec3{0, .0018f, 0};
    resting[i] = edge;
  }
  const int point_span = b.near ? 8 : 4;
  for (int end = 0; end < 2; ++end) {
    const int endpoint = end ? collar_last : collar_first;
    const int direction = end ? -1 : 1;
    const Vec3 anchor = resting[endpoint + direction * point_span];
    const float side = end ? 1.f : -1.f;
    const Vec3 tip = front_point(side * .027f, .318f) + Vec3{0, 0, .0028f};
    for (int j = 0; j < point_span; ++j) {
      const float t = float(j) / point_span;
      resting[endpoint + direction * j] = tip * (1 - t) + anchor * t;
    }
  }
  std::vector<std::vector<std::uint32_t>> outer(
      leaf_rows + 1, std::vector<std::uint32_t>(columns));
  outer[0] = stand;
  // The rolled collar is its own finite cloth shell. Coincident attachment
  // positions and identical weights preserve the sewn root, while separate
  // normal corners prevent a three-face normal weld with the shirt stand.
  for (int i = collar_first; i <= collar_last; ++i) {
    const P root = pattern.points[stand[i]];
    outer[0][i] = pattern.node(root.p, cloth * .94f, root.w);
  }
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
      CharacterInfluence weights =
          blend_weights({pattern.points[stand[i]].w, support.w, support.w},
                        {1 - smooth(t), smooth(t), 0});
      outer[j][i] = pattern.node(p, cloth * .94f, weights);
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
  // Close the hidden root too; all four boundaries of the lined fold have
  // finite thickness. This is a sewn attachment, not an unclosed inner sheet.
  for (int i = collar_first; i < collar_last; ++i) {
    const float u0 = .34f * (i - collar_first) / (collar_last - collar_first),
                u1 = .34f * (i + 1 - collar_first) /
                     (collar_last - collar_first);
    pattern.quad(corner_copy(outer[0][i + 1]), corner_copy(inner[0][i + 1]),
                 corner_copy(inner[0][i]), corner_copy(outer[0][i]), uv(u1, 0),
                 uv(u1, .002f), uv(u0, .002f), uv(u0, 0));
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
    const int cap_rows = b.near ? 4 : 2, lower_rows = b.near ? 8 : 4;
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
          const Vec3 end{side * (.149f + .034f * cs), .216f + .0035f * cs,
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
          float y = lerp(.216f, -.014f, t);
          if (b.near && j - cap_rows == lower_rows - 2)
            y = .030f;
          if (b.near && j - cap_rows == lower_rows - 1)
            y = .017f;
          const float elbow_t = smooth((.216f - y) / .073f);
          const float wrist_t = smooth((.143f - y) / .145f);
          const float x = lerp(.150f, .165f, elbow_t) + .020f * wrist_t;
          const float z = lerp(.006f, .008f, elbow_t) + .010f * wrist_t;
          const float front = sq(std::max(0.f, sn));
          const float back = sq(std::max(0.f, -sn));
          const float fold = .0022f * g(y, .114f + .014f * cs, .020f) * front -
                             .0016f * g(y, .140f - .010f * cs, .018f) * front +
                             .0018f * g(y, .080f - .013f * cs, .017f) * front +
                             .0024f * g(y, .127f, .037f) * back;
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

  // Only the exposed short placket below the vest hem is modeled; hidden
  // buttons and a second full chest closure add cost without a visible role.
  b.grid(
      2, 4,
      [&](float u, float v) {
        float x = lerp(-.006f, .006f, u), y = lerp(.068f, .095f, v);
        auto fit = front_fit(x, y);
        return P{fit.point + fit.normal * .0012f, cloth * .97f, fit.weights,
                 character_surface_uv(S::Shirt, u, v)};
      },
      false, false);
  pattern.finish();
  return {body_triangle_begin, yoke_triangle_end};
}
} // namespace fury
