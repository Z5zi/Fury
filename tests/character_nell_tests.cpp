#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/nell_anatomy.hpp"
#include "fury/generated/nell_hands.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
using namespace fury;
namespace {
void require(bool ok, const std::string &why) {
  if (!ok)
    throw std::runtime_error(why);
}
bool finite(Vec3 p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
int surface_at(Vec2 uv) {
  for (int i = 0; i < int(CharacterSurface::Count); ++i) {
    auto lo = character_surface_uv(CharacterSurface(i), 0, 0),
         hi = character_surface_uv(CharacterSurface(i), 1, 1);
    if (uv.x >= lo.x - 1e-7f && uv.x <= hi.x + 1e-7f && uv.y >= lo.y - 1e-7f &&
        uv.y <= hi.y + 1e-7f)
      return i;
  }
  return -1;
}
bool same(Vec3 a, Vec3 b, float tolerance = 1e-6f) {
  return length(a - b) <= tolerance;
}
void geometry(const CharacterProfile &p) {
  const auto &m = p.model;
  const auto &mesh = m.bind_mesh;
  float h = m.rig.height;
  require(p.individualized && m.seed == 0xd9d5f382 &&
              m.role == CharacterRole::Market,
          "Nell stable identity/role");
  require(m.dual_quaternion_skinning && p.lod_distance == 12,
          "Nell rig/LOD contract");
  require(mesh.vertices.size() == m.influences.size(),
          "All vertices have weights");
  require(mesh.indices.size() / 3 <=
              (m.lod == CharacterLod::Near ? 40000 : 16000),
          "Nell explicit triangle budget");
  require(same(m.appearance.skin, {.73f, .49f, .33f}) &&
              same(m.appearance.hair, {.055f, .042f, .033f}) &&
              same(m.appearance.eyes, {.13f, .08f, .045f}) &&
              same(m.appearance.shirt, {.53f, .52f, .43f}) &&
              same(m.appearance.trousers, {.12f, .16f, .20f}) &&
              same(m.appearance.accent, {.24f, .44f, .43f}) &&
              same(m.appearance.shoes, {.075f, .085f, .095f}),
          "Original independent palettes");
  float min_y = 100, max_y = -100;
  std::size_t reversed = 0, tiny = 0;
  std::array<unsigned, std::size_t(CharacterSurface::Count)> counts{};
  for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
    const auto &v = mesh.vertices[i];
    const auto &w = m.influences[i];
    require(finite(v.position) && finite(v.normal) && finite(v.color),
            "finite geometry");
    require(std::fabs(length(v.normal) - 1) < .0003f, "unit normal");
    int s = surface_at(v.uv);
    require(s >= 0, "UV inside content region");
    ++counts[s];
    require(w.first < character_bone_count && w.second < character_bone_count &&
                w.third < character_bone_count && w.first_weight >= 0 &&
                w.third_weight >= 0 &&
                w.first_weight + w.third_weight <= 1.000001f,
            "normalized weights");
    min_y = std::min(min_y, v.position.y);
    max_y = std::max(max_y, v.position.y);
  }
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
    auto ia = mesh.indices[t], ib = mesh.indices[t + 1],
         ic = mesh.indices[t + 2];
    require(ia < mesh.vertices.size() && ib < mesh.vertices.size() &&
                ic < mesh.vertices.size(),
            "indices in range");
    auto &a = mesh.vertices[ia];
    auto &b = mesh.vertices[ib];
    auto &c = mesh.vertices[ic];
    auto n = cross(b.position - a.position, c.position - a.position);
    require(surface_at(a.uv) == surface_at(b.uv) &&
                surface_at(a.uv) == surface_at(c.uv),
            "triangle cannot cross atlas regions");
    if (dot(n, n) < 1e-20f)
      ++tiny;
    if (dot(n, a.normal + b.normal + c.normal) < -1e-8f)
      ++reversed;
  }
  std::cout << (m.lod == CharacterLod::Near ? "Near" : "Far") << " triangles "
            << mesh.indices.size() / 3 << ", vertices " << mesh.vertices.size()
            << ", tiny " << tiny << ", reversed " << reversed << '\n';
  require(tiny == 0, "No degenerate triangles");
  require(reversed == 0, "Winding agrees with averaged normals");
  require(std::fabs(min_y / h + .5f) < 1e-5f &&
              std::fabs(max_y / h - .5f) < 1e-5f,
          "Height preserved exactly");
  for (auto s : {CharacterSurface::Face, CharacterSurface::Skin,
                 CharacterSurface::Eyes, CharacterSurface::HairCrop,
                 CharacterSurface::Hair, CharacterSurface::Shirt,
                 CharacterSurface::Pants, CharacterSurface::Jacket,
                 CharacterSurface::Rubber, CharacterSurface::Leather})
    require(counts[int(s)] > 0, "Required actual surface present");
}
void head(const CharacterModel &m) {
  const auto &mesh = m.bind_mesh;
  std::size_t cursor = 0;
  std::vector<bool> seen(nell_anatomy::positions.size());
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
    if (surface_at(mesh.vertices[mesh.indices[t]].uv) !=
        int(CharacterSurface::Face))
      continue;
    require(cursor + 2 < nell_anatomy::indices.size(),
            "No added face triangles");
    for (int k = 0; k < 3; ++k) {
      auto i = nell_anatomy::indices[cursor + k];
      auto &v = mesh.vertices[mesh.indices[t + k]];
      auto uv =
          nell_anatomy::uv_coordinates[nell_anatomy::uv_indices[cursor + k]];
      auto expected = character_surface_uv(CharacterSurface::Face, uv.x, uv.y);
      require(
          same(v.position, nell_anatomy::positions[i] * m.rig.height, 1e-7f) &&
              std::fabs(v.uv.x - expected.x) < 1e-7f &&
              std::fabs(v.uv.y - expected.y) < 1e-7f,
          "Original head position/corner UV/winding correspondence");
      seen[i] = true;
    }
    cursor += 3;
  }
  require(cursor == nell_anatomy::indices.size() &&
              std::all_of(seen.begin(), seen.end(), [](bool a) { return a; }),
          "Every source face retained");
  require(same(m.rig.joints[bone_index(CharacterBone::Head)].bind_position,
               nell_anatomy::head_center * m.rig.height),
          "Head joint belongs to Nell source");
}
struct Bounds {
  Vec3 low{1000, 1000, 1000}, high{-1000, -1000, -1000};
  void add(Vec3 p) {
    low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
    high = {std::max(high.x, p.x), std::max(high.y, p.y),
            std::max(high.z, p.z)};
  }
};
std::string label(const CharacterModel &m) {
  return m.lod == CharacterLod::Near ? "Near" : "Far";
}
bool proper_segment_triangle(Vec3 p, Vec3 q, Vec3 a, Vec3 b, Vec3 c, float h) {
  const auto direction = q - p, e1 = b - a, e2 = c - a;
  const auto cross_direction = cross(direction, e2);
  const float determinant = dot(e1, cross_direction);
  if (std::fabs(determinant) < h * h * h * 1.e-12f)
    return false;
  const auto offset = p - a, cross_offset = cross(offset, e1);
  const float u = dot(offset, cross_direction) / determinant,
              v = dot(direction, cross_offset) / determinant,
              t = dot(e2, cross_offset) / determinant;
  // Boundary contact and shared edges are not proper interior intersections.
  constexpr float epsilon = 1.e-5f;
  return u > epsilon && v > epsilon && u + v < 1 - epsilon && t > epsilon &&
         t < 1 - epsilon;
}
std::array<std::vector<Vec3>, 2> source_eye_openings() {
  std::map<std::pair<std::uint32_t, std::uint32_t>, unsigned> charts;
  for (std::size_t i = 0; i < nell_anatomy::indices.size(); i += 3) {
    const auto uv = nell_anatomy::uv_coordinates[nell_anatomy::uv_indices[i]];
    const unsigned chart = uv.x < .08f ? 2 : uv.x < .16f ? 1 : 0;
    for (int c = 0; c < 3; ++c) {
      auto a = nell_anatomy::indices[i + c],
           b = nell_anatomy::indices[i + (c + 1) % 3];
      if (a > b)
        std::swap(a, b);
      charts[{a, b}] |= 1u << chart;
    }
  }
  std::array<std::set<std::uint32_t>, 2> corners;
  for (const auto &e : charts)
    for (unsigned side = 0; side < 2; ++side)
      if (e.second == (1u | (1u << (side + 1)))) {
        corners[side].insert(e.first.first);
        corners[side].insert(e.first.second);
      }
  std::array<std::vector<Vec3>, 2> result;
  for (unsigned side = 0; side < 2; ++side) {
    require(corners[side].size() >= 20,
            "Both independent socket-to-skin seams are recovered");
    for (auto i : corners[side])
      result[side].push_back(nell_anatomy::positions[i]);
  }
  return result;
}

float projected_front(Vec3 p, const Mesh &mesh,
                      const std::vector<std::size_t> &triangles) {
  float front = -std::numeric_limits<float>::infinity();
  for (auto t : triangles) {
    const auto a = mesh.vertices[mesh.indices[t]].position;
    const auto b = mesh.vertices[mesh.indices[t + 1]].position;
    const auto c = mesh.vertices[mesh.indices[t + 2]].position;
    const float d = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (std::fabs(d) < 1e-14f)
      continue;
    const float u = ((p.x - a.x) * (c.y - a.y) - (p.y - a.y) * (c.x - a.x)) / d;
    const float v = ((b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x)) / d;
    if (u >= -1e-5f && v >= -1e-5f && u + v <= 1.00001f)
      front = std::max(front, a.z + u * (b.z - a.z) + v * (c.z - a.z));
  }
  return front;
}

void check_socket_fit(const CharacterModel &model) {
  const auto openings = source_eye_openings();
  std::vector<std::size_t> eyes, face;
  for (std::size_t t = 0; t < model.bind_mesh.indices.size(); t += 3) {
    const int surface =
        surface_at(model.bind_mesh.vertices[model.bind_mesh.indices[t]].uv);
    if (surface == int(CharacterSurface::Eyes))
      eyes.push_back(t);
    if (surface == int(CharacterSurface::Face))
      face.push_back(t);
  }
  float nearest_gap = std::numeric_limits<float>::infinity(),
        largest_protrusion = -nearest_gap;
  for (const auto &ring : openings) {
    Vec3 center{};
    for (auto p : ring)
      center += p;
    center = center * (1.f / ring.size());
    // Every rim point and inward spoke has real globe/iris geometry behind it.
    for (auto rim : ring)
      for (float fraction : {0.f, .25f, .5f, .75f, 1.f}) {
        const auto p =
            (center * (1 - fraction) + rim * fraction) * model.rig.height;
        const float front = projected_front(p, model.bind_mesh, eyes);
        require(std::isfinite(front),
                label(model) +
                    "eye mesh covers every socket rim and aperture spoke");
        if (fraction < 1) {
          const float skin_front = projected_front(p, model.bind_mesh, face);
          require(front > skin_front + model.rig.height * .0001f,
                  label(model) +
                      "eye remains visible through the actual socket aperture");
        }
        if (fraction == 1) {
          const float delta = (front - p.z) / model.rig.height;
          nearest_gap = std::min(nearest_gap, delta);
          largest_protrusion = std::max(largest_protrusion, delta);
        }
      }
  }
  std::cout << label(model) << ": actual socket-rim eye depth/h " << nearest_gap
            << " .. " << largest_protrusion << '\n';
  require(nearest_gap > -.003f && largest_protrusion < .002f,
          label(model) + " actual eye rim range/h " +
              std::to_string(nearest_gap) + ".." +
              std::to_string(largest_protrusion) + " exceeds [-.003,.002]");
}

void check_crop_coverage(const CharacterModel &model) {
  const auto &mesh = model.bind_mesh;
  const float h = model.rig.height;
  struct Triangle {
    std::array<Vec3, 3> p;
    Bounds bounds;
    std::size_t offset;
  };
  std::vector<Triangle> face, cap;
  using Point = std::tuple<float, float, float>;
  using Edge = std::pair<Point, Point>;
  std::map<Edge, unsigned> cap_edges;
  const auto point = [](Vec3 p) { return Point{p.x, p.y, p.z}; };
  const auto vector = [](Point p) {
    return Vec3{std::get<0>(p), std::get<1>(p), std::get<2>(p)};
  };
  Vec3 crown{0, -1000, 0};
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
    const int surface = surface_at(mesh.vertices[mesh.indices[t]].uv);
    if (surface != int(CharacterSurface::Face) &&
        surface != int(CharacterSurface::HairCrop))
      continue;
    Triangle triangle{{}, {}, t / 3};
    for (unsigned i = 0; i < 3; ++i) {
      const auto vertex = mesh.indices[t + i];
      triangle.p[i] = mesh.vertices[vertex].position * (1.f / h);
      triangle.bounds.add(triangle.p[i]);
      if (surface == int(CharacterSurface::HairCrop)) {
        const auto &w = model.influences[vertex];
        require(w.first == bone_index(CharacterBone::Head) &&
                    w.first_weight == 1 && w.third_weight == 0,
                "Opaque cap follows the same rigid Head transform as scalp");
        if (triangle.p[i].y > crown.y)
          crown = triangle.p[i];
      }
    }
    if (surface == int(CharacterSurface::Face)) {
      face.push_back(triangle);
    } else {
      cap.push_back(triangle);
      for (unsigned i = 0; i < 3; ++i) {
        auto a = point(triangle.p[i]), b = point(triangle.p[(i + 1) % 3]);
        if (b < a)
          std::swap(a, b);
        ++cap_edges[{a, b}];
      }
    }
  }
  require(!cap.empty() && face.size() == nell_anatomy::indices.size() / 3,
          "Coverage uses every actual anatomical Face and opaque cap triangle");
  std::vector<std::pair<Vec3, Vec3>> root;
  float root_low = 1;
  for (const auto &edge : cap_edges) {
    require(edge.second <= 2, "Opaque cap has no nonmanifold geometric edge");
    if (edge.second == 1) {
      const auto a = vector(edge.first.first), b = vector(edge.first.second);
      root.emplace_back(a, b);
      root_low = std::min(root_low, std::min(a.y, b.y));
      require(a.y < .48f && b.y < .48f,
              "Opaque cap's sole open boundary is its actual hairline root");
    }
  }
  require(root.size() >= 24 && root_low > .390f,
          "The actual cap root encloses the rigid upper scalp");

  std::size_t crossings = 0;
  std::pair<std::size_t, std::size_t> first_crossing{};
  for (const auto &a : face) {
    if (a.bounds.high.y < root_low)
      continue;
    for (const auto &b : cap) {
      if (a.bounds.high.x < b.bounds.low.x ||
          b.bounds.high.x < a.bounds.low.x ||
          a.bounds.high.y < b.bounds.low.y ||
          b.bounds.high.y < a.bounds.low.y ||
          a.bounds.high.z < b.bounds.low.z || b.bounds.high.z < a.bounds.low.z)
        continue;
      for (unsigned e = 0; e < 3; ++e) {
        if (proper_segment_triangle(a.p[e], a.p[(e + 1) % 3], b.p[0], b.p[1],
                                    b.p[2], 1.f) ||
            proper_segment_triangle(b.p[e], b.p[(e + 1) % 3], a.p[0], a.p[1],
                                    a.p[2], 1.f)) {
          if (crossings == 0)
            first_crossing = {a.offset, b.offset};
          ++crossings;
        }
      }
    }
  }

  // The crown's actual X/Z position gives an interior axis, not a scalp-radius
  // approximation. Recover the root height along each source point's azimuth,
  // then ray-test cap triangles at that exact source point's height. Sampling
  // source triangle vertices, edges and interiors also rejects an entirely
  // buried cap with no proper surface crossings, and uncovered scalp holes.
  std::size_t samples = 0, missing = 0;
  float minimum_gap = 1;
  Vec3 worst{};
  for (const auto &triangle : face) {
    if (triangle.bounds.high.y < root_low)
      continue;
    for (unsigned i = 0; i <= 8; ++i)
      for (unsigned j = 0; j <= 8 - i; ++j) {
        const auto p = triangle.p[0] +
                       (triangle.p[1] - triangle.p[0]) * (float(i) / 8) +
                       (triangle.p[2] - triangle.p[0]) * (float(j) / 8);
        if (p.y < root_low)
          continue;
        const Vec3 origin{crown.x, p.y, crown.z};
        const auto d = p - origin;
        const float radius = length(d);
        if (radius < 1.e-7f)
          continue;
        float root_height = std::numeric_limits<float>::infinity();
        for (const auto &edge : root) {
          const auto a = edge.first - origin, e = edge.second - edge.first;
          const float determinant = d.x * e.z - d.z * e.x;
          if (std::fabs(determinant) < 1.e-12f)
            continue;
          const float u = (a.x * d.z - a.z * d.x) / determinant;
          const float t = (a.x * e.z - a.z * e.x) / determinant;
          if (t >= 0 && u >= -1.e-5f && u <= 1 + 1.e-5f)
            root_height = std::min(root_height, edge.first.y + u * e.y);
        }
        require(std::isfinite(root_height),
                "Every scalp azimuth intersects the actual cap root");
        if (p.y <= root_height + 1.e-6f)
          continue; // Authored strand transition below the opaque root.
        ++samples;
        float front = -std::numeric_limits<float>::infinity();
        for (const auto &candidate : cap) {
          if (p.y < candidate.bounds.low.y - 1.e-7f ||
              p.y > candidate.bounds.high.y + 1.e-7f)
            continue;
          const auto e1 = candidate.p[1] - candidate.p[0];
          const auto e2 = candidate.p[2] - candidate.p[0];
          const auto cross_direction = cross(d, e2);
          const float determinant = dot(e1, cross_direction);
          if (std::fabs(determinant) < 1.e-12f)
            continue;
          const auto offset = origin - candidate.p[0];
          const auto cross_offset = cross(offset, e1);
          const float u = dot(offset, cross_direction) / determinant;
          const float v = dot(d, cross_offset) / determinant;
          const float t = dot(e2, cross_offset) / determinant;
          if (u >= -1.e-5f && v >= -1.e-5f && u + v <= 1 + 1.e-5f && t >= 0)
            front = std::max(front, t);
        }
        if (!std::isfinite(front)) {
          ++missing;
          continue;
        }
        const float gap = (front - 1) * radius;
        if (gap < minimum_gap) {
          minimum_gap = gap;
          worst = p;
        }
      }
  }
  std::cout << label(model) << ": opaque cap " << cap.size() << " triangles, "
            << root.size() << " root edges, " << crossings
            << " crossing witnesses, " << samples << " source samples, "
            << missing << " uncovered, minimum radial gap/h " << minimum_gap
            << '\n';
  require(samples > 10000 && missing == 0 && crossings == 0 &&
              minimum_gap >= -1.e-6f,
          label(model) +
              " opaque cap penetrates or fails to cover actual scalp; "
              "crossings " +
              std::to_string(crossings) + " first Face/cap " +
              std::to_string(first_crossing.first) + "/" +
              std::to_string(first_crossing.second) + ", missing " +
              std::to_string(missing) + ", minimum gap/h " +
              std::to_string(minimum_gap) + " at " + std::to_string(worst.x) +
              "," + std::to_string(worst.y) + "," + std::to_string(worst.z));
}

CharacterAnimationSample nell_sample(int mode, int frame, float height) {
  CharacterAnimationSample s;
  s.seed = 0xd9d5f382;
  s.phase_cycles = double(frame) / 32;
  s.idle_time = double(frame) / 8;
  if (mode == 1 || mode == 2 || mode == 5) {
    s.speed = height * (mode == 2 ? 2.8f : mode == 5 ? .55f : .8f);
    s.move_weight = 1;
  }
  if (mode == 3) {
    s.talk_weight = s.attention_weight = 1;
    s.attention_yaw = .45f;
    s.attention_pitch = -.10f;
  }
  if (mode == 4 || mode == 5)
    s.crouch_weight = 1;
  return s;
}
struct ClothContactAudit {
  const CharacterModel &m;
  std::vector<std::size_t> cloth, pants;
  explicit ClothContactAudit(const CharacterModel &model, bool upper = false)
      : m(model) {
    const auto &mesh = m.bind_mesh;
    float h = m.rig.height;
    for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
      int surface = surface_at(mesh.vertices[mesh.indices[t]].uv);
      Vec3 center{};
      for (int k = 0; k < 3; ++k)
        center += mesh.vertices[mesh.indices[t + k]].position * (1 / (3 * h));
      if (upper) {
        if (surface == int(CharacterSurface::Jacket) && center.y > .10f)
          cloth.push_back(t);
        if (surface == int(CharacterSurface::Shirt) && center.y > .07f)
          pants.push_back(t);
      } else {
        if (surface == int(CharacterSurface::Jacket) && center.y < .042f &&
            center.z > -.01f)
          cloth.push_back(t);
        if (surface == int(CharacterSurface::Pants) && center.y < .055f)
          pants.push_back(t);
      }
    }
    require(cloth.size() > 100 && pants.size() > 100,
            "Cloth contact audit has actual long-apron and leg geometry");
  }
  struct Item {
    std::size_t t;
    Bounds bounds;
  };
  std::size_t crossings(const Mesh &mesh, std::string label,
                        bool report = false) const {
    std::vector<Item> a, b;
    for (int set = 0; set < 2; ++set)
      for (auto t : set ? pants : cloth) {
        Item i{t, {}};
        for (int k = 0; k < 3; ++k)
          i.bounds.add(mesh.vertices[mesh.indices[t + k]].position);
        (set ? b : a).push_back(i);
      }
    std::sort(b.begin(), b.end(),
              [](auto &x, auto &y) { return x.bounds.low.x < y.bounds.low.x; });
    std::size_t count = 0;
    for (const auto &x : a)
      for (const auto &y : b) {
        if (y.bounds.low.x > x.bounds.high.x)
          break;
        if (y.bounds.high.x < x.bounds.low.x ||
            y.bounds.high.y < x.bounds.low.y ||
            y.bounds.low.y > x.bounds.high.y ||
            y.bounds.high.z < x.bounds.low.z ||
            y.bounds.low.z > x.bounds.high.z)
          continue;
        Vec3 p[3], q[3];
        for (int k = 0; k < 3; ++k) {
          p[k] = mesh.vertices[mesh.indices[x.t + k]].position;
          q[k] = mesh.vertices[mesh.indices[y.t + k]].position;
        }
        bool hit = false;
        for (int k = 0; k < 3; ++k)
          hit |= proper_segment_triangle(p[k], p[(k + 1) % 3], q[0], q[1], q[2],
                                         m.rig.height) ||
                 proper_segment_triangle(q[k], q[(k + 1) % 3], p[0], p[1], p[2],
                                         m.rig.height);
        if (hit) {
          if (report && count == 0) {
            std::cout << label << " first proper cloth/leg crossing " << x.t / 3
                      << " / " << y.t / 3 << '\n';
            for (auto &v : p)
              std::cout << "cloth " << v.x << "," << v.y << "," << v.z << '\n';
            for (auto &v : q)
              std::cout << "pants " << v.x << "," << v.y << "," << v.z << '\n';
          }
          ++count;
        }
      }
    return count;
  }
};
void check_cloth_contacts(const CharacterModel &m, bool diagnose = false) {
  ClothContactAudit audit(m);
  Mesh mesh = m.bind_mesh;
  std::size_t total = audit.crossings(mesh, label(m) + " bind", diagnose);
  for (int mode = 0; mode < 6; ++mode)
    for (int frame = 0; frame <= 32; frame += diagnose ? 4 : 1) {
      apply_character_pose(m,
                           sample_character_animation(
                               m.rig, nell_sample(mode, frame, m.rig.height)),
                           mesh);
      const auto context = label(m) + " mode " + std::to_string(mode) +
                           " frame " + std::to_string(frame);
      auto count = audit.crossings(mesh, context, diagnose);
      if (diagnose && count)
        std::cout << context << ": " << count
                  << " proper apron/leg crossings\n";
      total += count;
    }
  if (!diagnose)
    require(total == 0, "Long apron crosses legs in actual runtime poses: " +
                            std::to_string(total));
}

void check_upper_cloth_contacts(const CharacterModel &m) {
  ClothContactAudit audit(m, true);
  Mesh posed = m.bind_mesh;
  std::size_t count = audit.crossings(posed, label(m) + " upper bind", true);
  for (int mode : {1, 2, 3, 4})
    for (int frame = 0; frame <= 32; frame += 8) {
      apply_character_pose(m,
                           sample_character_animation(
                               m.rig, nell_sample(mode, frame, m.rig.height)),
                           posed);
      count += audit.crossings(posed, label(m) + " upper posed", true);
    }
  require(count == 0,
          "Apron tapes/bib cross the actual shirt: " + std::to_string(count));
}
void check_seat_coverage(const CharacterModel &m) {
  const auto &mesh = m.bind_mesh;
  const float h = m.rig.height;
  std::size_t samples = 0;
  // Shoot through actual center-back triangles beneath the waist. An angular
  // sampling mismatch used to leave a V-shaped hole only in the Far seat.
  for (int ix = -4; ix <= 4; ++ix)
    for (int iy = 0; iy <= 6; ++iy) {
      const Vec3 origin{ix * .002f * h, (.027f + .004f * iy) * h, -.20f * h};
      const Vec3 ray{0, 0, 1};
      float nearest = 100;
      for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
        const auto &a = mesh.vertices[mesh.indices[t]];
        if (surface_at(a.uv) != int(CharacterSurface::Pants))
          continue;
        const Vec3 b = mesh.vertices[mesh.indices[t + 1]].position,
                   c = mesh.vertices[mesh.indices[t + 2]].position;
        const Vec3 e1 = b - a.position, e2 = c - a.position, q = cross(ray, e2);
        const float det = dot(e1, q);
        if (std::fabs(det) < 1e-12f)
          continue;
        const Vec3 off = origin - a.position, r = cross(off, e1);
        const float u = dot(off, q) / det, v = dot(ray, r) / det,
                    d = dot(e2, r) / det;
        if (u >= -1e-6f && v >= -1e-6f && u + v <= 1.000001f && d > 0)
          nearest = std::min(nearest, d);
      }
      require(nearest < .18f * h,
              "Actual trouser seat has an uncovered center-back sample");
      ++samples;
    }
  std::cout << label(m) << ": " << samples
            << " actual seat coverage samples pass\n";
}

void check_neck_contacts(const CharacterModel &m) {
  const auto &bind = m.bind_mesh;
  std::vector<std::size_t> neck, cloth;
  for (std::size_t t = 0; t < bind.indices.size(); t += 3) {
    int s = surface_at(bind.vertices[bind.indices[t]].uv);
    Vec3 low{1, 1, 1}, high{-1, -1, -1};
    for (int k = 0; k < 3; ++k) {
      auto p = bind.vertices[bind.indices[t + k]].position * (1 / m.rig.height);
      low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
      high = {std::max(high.x, p.x), std::max(high.y, p.y),
              std::max(high.z, p.z)};
    }
    if (s == int(CharacterSurface::Face) && low.y > .334f && high.y < .39f)
      neck.push_back(t);
    if (s == int(CharacterSurface::Shirt) && high.y > .33f &&
        std::fabs((low.x + high.x) / 2) < .070f)
      cloth.push_back(t);
  }
  require(neck.size() > 100 && cloth.size() > 100,
          "Source-neck/actual collar contact regions are populated");
  Mesh mesh = bind;
  for (int yaw = -3; yaw <= 3; ++yaw)
    for (int pitch = -2; pitch <= 2; ++pitch) {
      auto sample = nell_sample(3, 9, m.rig.height);
      sample.attention_yaw = yaw * .30f;
      sample.attention_pitch = pitch * .20f;
      apply_character_pose(m, sample_character_animation(m.rig, sample), mesh);
      std::size_t count = 0;
      for (auto a : neck) {
        Bounds ba;
        Vec3 p[3];
        for (int k = 0; k < 3; ++k) {
          p[k] = mesh.vertices[mesh.indices[a + k]].position;
          ba.add(p[k]);
        }
        for (auto b : cloth) {
          Bounds bb;
          Vec3 q[3];
          for (int k = 0; k < 3; ++k) {
            q[k] = mesh.vertices[mesh.indices[b + k]].position;
            bb.add(q[k]);
          }
          if (ba.high.x < bb.low.x || bb.high.x < ba.low.x ||
              ba.high.y < bb.low.y || bb.high.y < ba.low.y ||
              ba.high.z < bb.low.z || bb.high.z < ba.low.z)
            continue;
          for (int k = 0; k < 3; ++k)
            if (proper_segment_triangle(p[k], p[(k + 1) % 3], q[0], q[1], q[2],
                                        m.rig.height) ||
                proper_segment_triangle(q[k], q[(k + 1) % 3], p[0], p[1], p[2],
                                        m.rig.height)) {
              ++count;
              break;
            }
        }
      }
      require(count == 0, "Neck/collar crossing in actual head turn yaw " +
                              std::to_string(yaw) + " pitch " +
                              std::to_string(pitch) + " count " +
                              std::to_string(count));
    }
  std::cout << label(m)
            << ":35 actual head turns clear the original source neck/collar\n";
}

void check_animation(const CharacterModel &m) {
  const auto &bind = m.bind_mesh;
  Mesh posed = bind;
  const auto *storage = posed.vertices.data();
  const auto *indices = posed.indices.data();
  const auto identity = posed.geometry_identity;
  std::size_t poses = 0;
  float min_area_ratio = 1;
  using Key = std::tuple<float, float, float, std::uint8_t, std::uint8_t, float,
                         std::uint8_t, float>;
  std::map<Key, std::size_t> first;
  std::vector<std::pair<std::size_t, std::size_t>> seams;
  for (std::size_t i = 0; i < bind.vertices.size(); ++i) {
    auto p = bind.vertices[i].position;
    auto w = m.influences[i];
    Key k{p.x,     p.y,           p.z, w.first, w.second, w.first_weight,
          w.third, w.third_weight};
    auto pair = first.emplace(k, i);
    if (!pair.second)
      seams.emplace_back(pair.first->second, i);
  }
  std::vector<float> areas;
  for (std::size_t t = 0; t < bind.indices.size(); t += 3) {
    auto a = bind.vertices[bind.indices[t]].position,
         b = bind.vertices[bind.indices[t + 1]].position,
         c = bind.vertices[bind.indices[t + 2]].position;
    areas.push_back(length(cross(b - a, c - a)));
  }
  for (int mode = 0; mode < 6; ++mode)
    for (int frame = 0; frame <= 32; ++frame) {
      auto pose = sample_character_animation(
          m.rig, nell_sample(mode, frame, m.rig.height));
      apply_character_pose(m, pose, posed);
      require(posed.vertices.data() == storage &&
                  posed.indices.data() == indices &&
                  posed.geometry_identity == identity,
              "Stable per-frame mesh storage and identity");
      for (std::size_t i = 0; i < posed.vertices.size(); ++i) {
        auto &a = posed.vertices[i];
        auto &b = bind.vertices[i];
        require(finite(a.position) && finite(a.normal) &&
                    std::fabs(length(a.normal) - 1) < .001f,
                "Finite actual runtime-skinned geometry");
        require(std::memcmp(&a.color, &b.color, sizeof(Vec3)) == 0 &&
                    std::memcmp(&a.uv, &b.uv, sizeof(Vec2)) == 0,
                "Skinning preserves source UV/color");
      }
      for (auto [a, b] : seams)
        require(
            same(posed.vertices[a].position, posed.vertices[b].position, 1e-7f),
            "Actual pose opens a sewn seam");
      for (std::size_t t = 0; t < posed.indices.size(); t += 3) {
        auto a = posed.vertices[posed.indices[t]].position,
             b = posed.vertices[posed.indices[t + 1]].position,
             c = posed.vertices[posed.indices[t + 2]].position;
        float ratio = length(cross(b - a, c - a)) / areas[t / 3];
        min_area_ratio = std::min(min_area_ratio, ratio);
        require(ratio > .001f, "Actual pose collapses triangle at mode " +
                                   std::to_string(mode) + " frame " +
                                   std::to_string(frame) + " triangle " +
                                   std::to_string(t / 3));
      }
      ++poses;
    }
  for (int yaw = -3; yaw <= 3; ++yaw)
    for (int pitch = -2; pitch <= 2; ++pitch) {
      auto s = nell_sample(3, 9, m.rig.height);
      s.attention_yaw = yaw * .30f;
      s.attention_pitch = pitch * .2f;
      auto pose = sample_character_animation(m.rig, s);
      apply_character_pose(m, pose, posed);
      for (auto [a, b] : seams)
        require(
            same(posed.vertices[a].position, posed.vertices[b].position, 1e-7f),
            "Head turn opens source/cloth seam");
      ++poses;
    }
  std::cout << label(m) << " height " << m.rig.height << ": " << poses
            << " poses, " << seams.size()
            << " welded corner pairs, minimum area ratio " << min_area_ratio
            << '\n';
}

void check_cloth_shells(const CharacterModel &m) {
  const auto &mesh = m.bind_mesh;
  using Key = std::tuple<float, float, float>;
  std::map<Key, unsigned> nodes;
  std::vector<Vec3> points;
  std::vector<std::array<unsigned, 3>> triangles;
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
    if (surface_at(mesh.vertices[mesh.indices[t]].uv) !=
        int(CharacterSurface::Jacket))
      continue;
    std::array<unsigned, 3> out{};
    for (int k = 0; k < 3; ++k) {
      Vec3 p = mesh.vertices[mesh.indices[t + k]].position * (1 / m.rig.height);
      Key key{p.x, p.y, p.z};
      auto found = nodes.find(key);
      if (found == nodes.end()) {
        unsigned i = points.size();
        points.push_back(p);
        found = nodes.emplace(key, i).first;
      }
      out[k] = found->second;
    }
    triangles.push_back(out);
  }
  std::vector<unsigned> parent(points.size());
  for (unsigned i = 0; i < parent.size(); ++i)
    parent[i] = i;
  auto root = [&](unsigned i) {
    while (parent[i] != i) {
      parent[i] = parent[parent[i]];
      i = parent[i];
    }
    return i;
  };
  std::map<std::pair<unsigned, unsigned>, unsigned> edges;
  for (auto t : triangles)
    for (int k = 0; k < 3; ++k) {
      auto a = t[k], b = t[(k + 1) % 3];
      require(a != b, "Cloth shell has coincident boundary sides");
      parent[root(a)] = root(b);
      if (a > b)
        std::swap(a, b);
      ++edges[{a, b}];
    }
  for (auto [edge, count] : edges)
    require(count == 2, "Closed apron/ribbon shell has a boundary or "
                        "nonmanifold geometric edge");
  struct Volume {
    double volume = 0, area = 0;
    unsigned triangles = 0;
  };
  std::map<unsigned, Volume> volumes;
  for (auto t : triangles) {
    auto &v = volumes[root(t[0])];
    const auto a = points[t[0]], b = points[t[1]], c = points[t[2]];
    v.volume += double(dot(a, cross(b, c))) / 6;
    v.area += double(length(cross(b - a, c - a))) / 2;
    ++v.triangles;
  }
  double largest_area = 0, depth = 0;
  for (auto [component, v] : volumes)
    if (v.area > largest_area) {
      largest_area = v.area;
      depth = 2 * std::fabs(v.volume) / v.area;
    }
  std::cout << label(m) << " shell components " << volumes.size()
            << " largest area " << largest_area << " effective depth " << depth
            << std::endl;
  require(volumes.size() >= 9 && largest_area > .08 && depth > .0007 &&
              depth < .0017,
          "Largest apron shell retains a real positive normal thickness");
  std::cout << label(m) << ": " << volumes.size()
            << " closed cloth/trim components; apron volume/area thickness/h "
            << depth << '\n';
}
void check_negative_controls(const CharacterModel &m) {
  auto reject = [&](auto test, const char *message) {
    bool rejected = false;
    try {
      test();
    } catch (const std::exception &) {
      rejected = true;
    }
    require(rejected, message);
  };
  auto cap = m;
  float cz = nell_anatomy::crown_joint.z * m.rig.height;
  for (auto &v : cap.bind_mesh.vertices)
    if (surface_at(v.uv) == int(CharacterSurface::HairCrop)) {
      v.position.x *= .6f;
      v.position.z = cz + (v.position.z - cz) * .6f;
    }
  reject([&] { check_crop_coverage(cap); },
         "Actual cap coverage must reject a buried scalp cap");
  auto eyes = m;
  for (auto &v : eyes.bind_mesh.vertices)
    if (surface_at(v.uv) == int(CharacterSurface::Eyes))
      v.position.z += m.rig.height * .015f;
  reject([&] { check_socket_fit(eyes); },
         "Actual socket fit must reject protruding eyes");
  auto shell = m;
  for (auto &v : shell.bind_mesh.vertices)
    if (surface_at(v.uv) == int(CharacterSurface::Jacket))
      v.position.z = 0;
  reject([&] { check_cloth_shells(shell); },
         "Actual closed-shell volume must reject zero-thickness cloth");
  std::cout << label(m)
            << ":3 independent geometry negative controls rejected\n";
}

void hand_source_contract(const CharacterModel &m) {
  const auto &mesh = m.bind_mesh;
  const float h = m.rig.height;
  for (float side : {-1.f, 1.f}) {
    auto expected = [&](auto const &indices, auto const &uvs) {
      std::multiset<std::tuple<float, float, float, float, float>> corners;
      for (std::size_t k = 0; k < indices.size(); ++k) {
        auto p = nell_hands::positions[indices[k]];
        p.x *= side;
        p += Vec3{side * .151f, -.043f, .008f};
        p = p * h;
        auto uv = nell_hands::uv_coordinates[uvs[k]];
        uv = character_surface_uv(CharacterSurface::Skin, uv.x, uv.y);
        corners.emplace(p.x, p.y, p.z, uv.x, uv.y);
      }
      return corners;
    };
    const auto need =
        m.lod == CharacterLod::Near
            ? expected(nell_hands::near_indices, nell_hands::near_uv_indices)
            : expected(nell_hands::far_indices, nell_hands::far_uv_indices);
    std::multiset<std::tuple<float, float, float, float, float>> actual;
    for (auto i : mesh.indices) {
      auto &v = mesh.vertices[i];
      const auto &w = m.influences[i];
      if (surface_at(v.uv) == int(CharacterSurface::Skin) &&
          w.first == bone_index(side > 0 ? CharacterBone::RightHand
                                         : CharacterBone::LeftHand) &&
          w.first_weight == 1)
        actual.emplace(v.position.x, v.position.y, v.position.z, v.uv.x,
                       v.uv.y);
    }
    for (auto &k : need) {
      auto found = actual.find(k);
      require(found != actual.end(),
              "Actual runtime hand loses source position/corner UV");
      actual.erase(found);
    }
  }
}

void report_motion_radius(const CharacterModel &m) {
  Mesh posed = m.bind_mesh;
  for (int mode : {0, 1, 2, 3, 4, 5, 6, 7, 8}) {
    float maximum = 0;
    int witness_frame = 0;
    std::size_t witness_vertex = 0;
    for (int frame = 0; frame <= 128; ++frame) {
      auto sample = nell_sample(mode, 0, m.rig.height);
      if (mode >= 6) {
        sample.speed = 1.9f;
        sample.move_weight = 1;
        sample.turn_rate = mode == 7 ? -6.f : mode == 8 ? 6.f : 0.f;
      }
      sample.phase_cycles = double(frame) / 128;
      sample.idle_time = double(frame) / 32;
      apply_character_pose(m, sample_character_animation(m.rig, sample), posed);
      for (std::size_t i = 0; i < posed.vertices.size(); ++i) {
        const auto point = posed.vertices[i].position;
        const float radius = std::sqrt(point.x * point.x + point.z * point.z);
        if (radius > maximum) {
          maximum = radius;
          witness_frame = frame;
          witness_vertex = i;
        }
      }
    }
    std::cout << label(m) << " radius mode " << mode << " max_m " << maximum
              << " phase " << double(witness_frame) / 128 << " vertex "
              << witness_vertex << '\n';
  }
}

void dump(const CharacterModel &m, const std::string &path) {
  std::ofstream f(path, std::ios::binary);
  std::uint32_t nv = m.bind_mesh.vertices.size(),
                ni = m.bind_mesh.indices.size();
  f.write((char *)&nv, 4);
  f.write((char *)&ni, 4);
  for (std::size_t i = 0; i < nv; ++i) {
    const auto &v = m.bind_mesh.vertices[i];
    float p[11] = {v.position.x, v.position.y, v.position.z, v.normal.x,
                   v.normal.y,   v.normal.z,   v.color.x,    v.color.y,
                   v.color.z,    v.uv.x,       v.uv.y};
    f.write((char *)p, sizeof(p));
    const auto &w = m.influences[i];
    float q[5] = {float(w.first), float(w.second), w.first_weight,
                  float(w.third), w.third_weight};
    f.write((char *)q, sizeof(q));
  }
  f.write((char *)m.bind_mesh.indices.data(), ni * 4);
  require(bool(f), "Mesh diagnostic write");
}
} // namespace
int main(int argc, char **argv) {
  try {
    for (auto lod : {CharacterLod::Near, CharacterLod::Far}) {
      auto p = make_character_profile(1.75f, CharacterRole::Market, "NpcCivAsh",
                                      lod);
      if (argc == 3 && std::string(argv[1]) == "--dump") {
        dump(p.model,
             std::string(argv[2]) +
                 (lod == CharacterLod::Near ? "-near.bin" : "-far.bin"));
        continue;
      }
      if (argc == 2 && std::string(argv[1]) == "--cap") {
        check_crop_coverage(p.model);
        continue;
      }
      if (argc == 2 && std::string(argv[1]) == "--contacts") {
        check_cloth_contacts(p.model, true);
        continue;
      }
      if (argc == 2 && std::string(argv[1]) == "--neck") {
        check_neck_contacts(p.model);
        continue;
      }
      if (argc == 2 && std::string(argv[1]) == "--radius") {
        report_motion_radius(p.model);
        continue;
      }
      if (argc == 2 && std::string(argv[1]) == "--seat") {
        check_seat_coverage(p.model);
        continue;
      }
      if (argc == 2 && std::string(argv[1]) == "--upper-cloth") {
        check_upper_cloth_contacts(p.model);
        check_seat_coverage(p.model);
        continue;
      }
      geometry(p);
      check_upper_cloth_contacts(p.model);
      check_seat_coverage(p.model);
      head(p.model);
      hand_source_contract(p.model);
      check_socket_fit(p.model);
      check_cloth_shells(p.model);
      check_negative_controls(p.model);
      check_crop_coverage(p.model);
      check_animation(p.model);
      check_cloth_contacts(p.model);
      check_neck_contacts(p.model);
    }
    if (argc == 1)
      for (float height : {1.35f, 2.10f})
        for (auto lod : {CharacterLod::Near, CharacterLod::Far}) {
          auto p = make_character_profile(height, CharacterRole::Market,
                                          "NpcCivAsh", lod);
          if (argc == 2 && std::string(argv[1]) == "--contacts") {
            check_cloth_contacts(p.model, true);
            continue;
          }
          if (argc == 2 && std::string(argv[1]) == "--neck") {
            check_neck_contacts(p.model);
            continue;
          }
          geometry(p);
          head(p.model);
          check_animation(p.model);
        }
    std::cout << "Nell checks passed\n";
    return 0;
  } catch (std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
