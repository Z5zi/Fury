#pragma once
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <map>
#include <stdexcept>
#include <vector>

namespace fury::character_detail {
inline constexpr float pi = 3.14159265358979323846f;
using B = CharacterBone;
using S = CharacterSurface;
inline float sq(float x) { return x * x; }
inline float g(float x, float center, float width) {
  return std::exp(-sq((x - center) / width));
}
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float smooth(float t) {
  t = std::clamp(t, 0.f, 1.f);
  return t * t * (3 - 2 * t);
}
inline CharacterInfluence wt(B a, B b, float first) {
  return {std::uint8_t(a), std::uint8_t(b), first};
}
inline CharacterInfluence wt(B a) { return wt(a, a, 1); }
struct P {
  Vec3 p;
  Vec3 c;
  CharacterInfluence w;
  Vec2 uv;
};
struct Sculpt {
  CharacterModel &m;
  float h;
  bool near;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> smooth_seams;
  std::vector<std::vector<std::uint32_t>> normal_groups;
  std::uint32_t vertex(const P &p) {
    auto i = std::uint32_t(m.bind_mesh.vertices.size());
    m.bind_mesh.vertices.push_back({p.p * h, {}, p.c, p.uv});
    m.influences.push_back(p.w);
    return i;
  }
  void tri(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    auto &v = m.bind_mesh.indices;
    v.insert(v.end(), {a, b, c});
  }
  void grid(int columns, int rows, const std::function<P(float, float)> &point,
            bool seam = true, bool caps = true) {
    auto base = std::uint32_t(m.bind_mesh.vertices.size());
    auto sample = [&](int i, float v) {
      P p = point(float(i) / columns, v);
      if (seam && i == columns) {
        P first = point(0, v);
        Vec3 drift = p.p - first.p;
        if (dot(drift, drift) > 1e-12f)
          throw std::invalid_argument(
              "Nonperiodic sculpt seam at vertex " + std::to_string(base) +
              " row " + std::to_string(v) + " drift " +
              std::to_string(std::sqrt(dot(drift, drift))));
        // Preserve the distinct U=1 texture corner while making the physical
        // seam exact. Only floating-point trigonometric drift is accepted.
        p.p = first.p;
        p.c = first.c;
        p.w = first.w;
      }
      return p;
    };
    for (int j = 0; j <= rows; ++j)
      for (int i = 0; i <= columns; ++i)
        vertex(sample(i, float(j) / rows));
    for (int j = 0; j < rows; ++j)
      for (int i = 0; i < columns; ++i) {
        auto a = base + j * (columns + 1) + i, b = a + columns + 1;
        tri(a, b, b + 1);
        tri(a, b + 1, a + 1);
      }
    if (seam)
      for (int j = 0; j <= rows; ++j)
        smooth_seams.push_back(
            {base + j * (columns + 1), base + j * (columns + 1) + columns});
    if (caps)
      for (int end = 0; end < 2; ++end) {
        P center = point(0, float(end));
        center.p = {};
        for (int i = 0; i < columns; ++i)
          center.p += point(float(i) / columns, float(end)).p;
        center.p = center.p * (1.f / float(columns));
        auto c = vertex(center),
             ring = std::uint32_t(m.bind_mesh.vertices.size());
        for (int i = 0; i <= columns; ++i)
          vertex(sample(i, float(end)));
        for (int i = 0; i < columns; ++i) {
          if (end)
            tri(c, ring + i + 1, ring + i);
          else
            tri(c, ring + i, ring + i + 1);
        }
      }
  }
  void ellipsoid(Vec3 center, Vec3 radius, Vec3 color, S surface, B bone,
                 int columns = 20, int rows = 10) {
    grid(columns, rows, [&](float u, float v) {
      float a = 2 * pi * u, e = lerp(-pi * .49f, pi * .49f, v);
      return P{center + Vec3{radius.x * std::cos(a) * std::cos(e),
                             radius.y * std::sin(e),
                             radius.z * std::sin(a) * std::cos(e)},
               color, wt(bone), character_surface_uv(surface, u, v)};
    });
  }
  void tube(const std::vector<Vec3> &path, const std::vector<float> &radii,
            float flatten, Vec3 color, S surface,
            const std::function<CharacterInfluence(float)> &weights,
            int sides = 8, Vec3 preferred_axis = {}) {
    if (path.size() < 2 || path.size() != radii.size())
      throw std::invalid_argument("Invalid sculpt curve");
    const int rows = int(path.size()) - 1;
    std::vector<Vec3> first_axis(path.size()), second_axis(path.size());
    for (int j = 0; j <= rows; ++j) {
      const Vec3 tangent =
          normalize(path[std::min(rows, j + 1)] - path[std::max(0, j - 1)]);
      Vec3 reference = j ? first_axis[j - 1]
                         : (dot(preferred_axis, preferred_axis) > 0.f
                                ? preferred_axis
                                : (std::fabs(tangent.y) < .8f ? Vec3{0, 1, 0}
                                                              : Vec3{1, 0, 0}));
      Vec3 projected = reference - tangent * dot(reference, tangent);
      if (dot(projected, projected) < 1e-12f) {
        reference = std::fabs(tangent.x) < .8f ? Vec3{1, 0, 0} : Vec3{0, 0, 1};
        projected = reference - tangent * dot(reference, tangent);
      }
      first_axis[j] = normalize(projected);
      second_axis[j] = normalize(cross(first_axis[j], tangent));
    }
    grid(sides, rows, [&](float u, float v) {
      int j = std::min(rows, int(std::lround(v * rows)));
      float angle = 2 * pi * u;
      return P{path[j] + first_axis[j] * (std::cos(angle) * radii[j]) +
                   second_axis[j] * (std::sin(angle) * radii[j] * flatten),
               color, weights(v), character_surface_uv(surface, u, v)};
    });
  }
  void curve(const std::vector<Vec3> &points, float radius, Vec3 color,
             S surface, B bone, int sides = 6, float flatten = 1.f) {
    tube(
        points, std::vector<float>(points.size(), radius), flatten, color,
        surface, [&](float) { return wt(bone); }, sides);
  }
  void finish() {
    auto &mesh = m.bind_mesh;
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
      auto &a = mesh.vertices[mesh.indices[i]];
      auto &b = mesh.vertices[mesh.indices[i + 1]];
      auto &c = mesh.vertices[mesh.indices[i + 2]];
      auto n = cross(b.position - a.position, c.position - a.position);
      a.normal += n;
      b.normal += n;
      c.normal += n;
    }
    for (auto pair : smooth_seams) {
      auto n =
          mesh.vertices[pair.first].normal + mesh.vertices[pair.second].normal;
      mesh.vertices[pair.first].normal = n;
      mesh.vertices[pair.second].normal = n;
    }
    for (const auto &group : normal_groups) {
      Vec3 sum{};
      for (auto index : group)
        sum += mesh.vertices[index].normal;
      for (auto index : group)
        mesh.vertices[index].normal = sum;
    }
    for (auto &v : mesh.vertices) {
      const float largest =
          std::max({std::fabs(v.normal.x), std::fabs(v.normal.y),
                    std::fabs(v.normal.z)});
      if (largest > 0.f && std::isfinite(largest)) {
        Vec3 scaled = v.normal * (1.f / largest);
        v.normal = scaled * (1.f / std::sqrt(dot(scaled, scaled)));
      }
    }
  }
};
} // namespace fury::character_detail
