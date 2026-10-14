#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/renn_anatomy.hpp"
#include "fury/generated/renn_hands.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <new>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
namespace {
bool count_allocations = false;
std::size_t allocations = 0;
} // namespace
#if defined(_MSC_VER)
#define TEST_NOINLINE __declspec(noinline)
#else
#define TEST_NOINLINE __attribute__((noinline))
#endif
TEST_NOINLINE void *operator new(std::size_t size) {
  if (count_allocations)
    ++allocations;
  if (void *p = std::malloc(size ? size : 1))
    return p;
  throw std::bad_alloc();
}
TEST_NOINLINE void *operator new[](std::size_t size) {
  return ::operator new(size);
}
// stable_sort's construction-only temporary buffer uses nothrow allocation.
// Route it through the same allocation probe so ASan sees a matching malloc /
// free pair and a future per-frame nothrow allocation cannot evade the guard.
TEST_NOINLINE void *operator new(std::size_t size,
                                 const std::nothrow_t &) noexcept {
  try {
    return ::operator new(size);
  } catch (...) {
    return nullptr;
  }
}
TEST_NOINLINE void *operator new[](std::size_t size,
                                   const std::nothrow_t &) noexcept {
  try {
    return ::operator new[](size);
  } catch (...) {
    return nullptr;
  }
}
TEST_NOINLINE void operator delete(void *p, const std::nothrow_t &) noexcept {
  std::free(p);
}
TEST_NOINLINE void operator delete[](void *p, const std::nothrow_t &) noexcept {
  std::free(p);
}
TEST_NOINLINE void operator delete(void *p) noexcept { std::free(p); }
TEST_NOINLINE void operator delete[](void *p) noexcept { std::free(p); }
TEST_NOINLINE void operator delete(void *p, std::size_t) noexcept {
  std::free(p);
}
TEST_NOINLINE void operator delete[](void *p, std::size_t) noexcept {
  std::free(p);
}

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
  require(p.individualized && m.seed == 0xb242d872 &&
              m.role == CharacterRole::Security,
          "Renn stable identity/role");
  require(m.dual_quaternion_skinning && p.lod_distance == 12,
          "Renn rig/LOD contract");
  require(mesh.vertices.size() == m.influences.size(),
          "All vertices have weights");
  require(mesh.indices.size() / 3 <=
              (m.lod == CharacterLod::Near ? 40000 : 16000),
          "Renn explicit triangle budget");
  require(same(m.appearance.skin, {.35f, .205f, .14f}) &&
              same(m.appearance.hair, {.055f, .042f, .033f}) &&
              same(m.appearance.eyes, {.13f, .08f, .045f}) &&
              same(m.appearance.shoes, {.075f, .085f, .095f}),
          "Original Renn skin/hair/eye/shoe palette");
  float min_y = 100, max_y = -100;
  std::size_t reversed = 0, tiny = 0;
  std::array<unsigned, std::size_t(CharacterSurface::Count)> counts{};
  for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
    const auto &v = mesh.vertices[i];
    const auto &w = m.influences[i];
    require(finite(v.position) && finite(v.normal) && finite(v.color),
            "finite geometry");
    require(std::fabs(length(v.normal) - 1) < .0003f,
            "unit normal vertex " + std::to_string(i));
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
    if (dot(n, a.normal + b.normal + c.normal) <= 0.f)
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
  std::vector<bool> seen(renn_anatomy::positions.size());
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
    if (surface_at(mesh.vertices[mesh.indices[t]].uv) !=
        int(CharacterSurface::Face))
      continue;
    require(cursor + 2 < renn_anatomy::indices.size(),
            "No added face triangles");
    for (int k = 0; k < 3; ++k) {
      auto i = renn_anatomy::indices[cursor + k];
      auto &v = mesh.vertices[mesh.indices[t + k]];
      auto uv =
          renn_anatomy::uv_coordinates[renn_anatomy::uv_indices[cursor + k]];
      auto expected = character_surface_uv(CharacterSurface::Face, uv.x, uv.y);
      require(
          same(v.position, renn_anatomy::positions[i] * m.rig.height, 1e-7f) &&
              std::fabs(v.uv.x - expected.x) < 1e-7f &&
              std::fabs(v.uv.y - expected.y) < 1e-7f,
          "Original head position/corner UV/winding correspondence");
      seen[i] = true;
    }
    cursor += 3;
  }
  require(cursor == renn_anatomy::indices.size() &&
              std::all_of(seen.begin(), seen.end(), [](bool a) { return a; }),
          "Every source face retained");
  require(same(m.rig.joints[bone_index(CharacterBone::Head)].bind_position,
               renn_anatomy::head_center * m.rig.height),
          "Head joint belongs to Renn source");
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
  // Double intermediates distinguish a shared corner from a real crossing at
  // nearly parallel, millimetre-scale collar faces. Input positions are still
  // the actual float32 renderer mesh; no geometry or tolerance is rounded away.
  using D = std::array<double, 3>;
  auto sub = [](Vec3 x, Vec3 y) {
    return D{double(x.x) - y.x, double(x.y) - y.y, double(x.z) - y.z};
  };
  auto crossd = [](D x, D y) {
    return D{x[1] * y[2] - x[2] * y[1], x[2] * y[0] - x[0] * y[2],
             x[0] * y[1] - x[1] * y[0]};
  };
  auto dotd = [](D x, D y) { return x[0] * y[0] + x[1] * y[1] + x[2] * y[2]; };
  auto direction = sub(q, p), e1 = sub(b, a), e2 = sub(c, a),
       cross_direction = crossd(direction, e2);
  const double determinant = dotd(e1, cross_direction);
  if (std::fabs(determinant) < double(h) * h * h * 1.e-12)
    return false;
  auto offset = sub(p, a), cross_offset = crossd(offset, e1);
  const double u = dotd(offset, cross_direction) / determinant,
               v = dotd(direction, cross_offset) / determinant,
               t = dotd(e2, cross_offset) / determinant;
  constexpr double epsilon = 1.e-5;
  return u > epsilon && v > epsilon && u + v < 1 - epsilon && t > epsilon &&
         t < 1 - epsilon;
}

void contact_predicate_controls() {
  // Test the near-parallel domain directly, not only the deliberately deep
  // injected garment penetration. All test points are binary32 renderer input.
  for (float h : {1.35f, 1.82f, 2.10f}) {
    const Vec3 origin{.013f * h, .294f * h, -.008f * h};
    const auto point = [&](float x, float y, float z) {
      return origin + Vec3{x, y, z} * h;
    };
    const Vec3 a = point(0, 0, 0), b = point(.004f, 0, 0),
               c = point(0, .004f, 0);
    require(proper_segment_triangle(point(.001f, .001f, .0000002f),
                                    point(.002f, .001f, -.0000002f), a, b, c,
                                    h),
            "Shallow but real float32 crossing must be detected");
    require(
        !proper_segment_triangle(a, point(.002f, .001f, .0000002f), a, b, c, h),
        "A near-parallel shared endpoint is not a proper crossing");
    require(!proper_segment_triangle(point(.001f, .001f, .0000002f),
                                     point(.002f, .001f, .0000002f), a, b, c,
                                     h),
            "Parallel separated triangles remain separate");
    require(!proper_segment_triangle(point(.005f, .001f, .0000002f),
                                     point(.006f, .001f, -.0000002f), a, b, c,
                                     h),
            "A shallow plane crossing outside the triangle is not a contact");
  }
}

std::array<std::vector<Vec3>, 2> source_eye_openings() {
  std::map<std::pair<std::uint32_t, std::uint32_t>, unsigned> charts;
  for (std::size_t i = 0; i < renn_anatomy::indices.size(); i += 3) {
    const auto uv = renn_anatomy::uv_coordinates[renn_anatomy::uv_indices[i]];
    const unsigned chart = uv.x < .08f ? 2 : uv.x < .16f ? 1 : 0;
    for (int c = 0; c < 3; ++c) {
      auto a = renn_anatomy::indices[i + c],
           b = renn_anatomy::indices[i + (c + 1) % 3];
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
      result[side].push_back(renn_anatomy::positions[i]);
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
  require(!cap.empty() && face.size() == renn_anatomy::indices.size() / 3,
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

CharacterAnimationSample renn_sample(int mode, int frame, float height) {
  CharacterAnimationSample s;
  s.seed = 0xb242d872;
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
void check_animation(const CharacterModel &m) {
  const auto &bind = m.bind_mesh;
  Mesh posed = bind;
  const auto *storage = posed.vertices.data();
  const auto *indices = posed.indices.data();
  const auto identity = posed.geometry_identity;
  posed.gpu_vao = 81;
  posed.gpu_vbo = 82;
  posed.gpu_ibo = 83;
  std::size_t poses = 0;
  float min_area_ratio = 1;
  float max_cached_seam_gap = 0;
  Mesh exact = bind;
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
          m.rig, renn_sample(mode, frame, m.rig.height));
      const auto before_allocations = allocations;
      count_allocations = true;
      apply_character_pose(m, pose, posed);
      count_allocations = false;
      require(allocations == before_allocations,
              "Runtime pose allocates memory");
      require(posed.vertices.data() == storage &&
                  posed.indices.data() == indices &&
                  posed.geometry_identity == identity && posed.gpu_vao == 81 &&
                  posed.gpu_vbo == 82 && posed.gpu_ibo == 83,
              "Stable per-frame mesh storage and identity");
      for (std::size_t i = 0; i < posed.vertices.size(); ++i) {
        auto &a = posed.vertices[i];
        auto &b = bind.vertices[i];
        require(finite(a.position) && finite(a.normal) &&
                    std::fabs(length(a.normal) - 1) < .001f,
                "Finite actual runtime-skinned geometry at vertex " +
                    std::to_string(i));
        if (m.influences[i].first == bone_index(CharacterBone::LeftFoot) ||
            m.influences[i].first == bone_index(CharacterBone::RightFoot))
          require(a.position.y >= -.5001f * m.rig.height,
                  "Actual shoe geometry penetrates the ground at vertex " +
                      std::to_string(i) + " mode " + std::to_string(mode) +
                      " frame " + std::to_string(frame) + " y/h " +
                      std::to_string(a.position.y / m.rig.height) +
                      " bind_y/h " +
                      std::to_string(b.position.y / m.rig.height));
        require(std::memcmp(&a.color, &b.color, sizeof(Vec3)) == 0 &&
                    std::memcmp(&a.uv, &b.uv, sizeof(Vec2)) == 0,
                "Skinning preserves source UV/color");
      }
      // The production pose cache intentionally keeps positions within
      // 1 micrometre/component when neither position nor corner normal changes.
      // Hard edge normals can therefore refresh one sewn copy earlier than
      // another. Separately certify uncached equality without hiding that gap.
      for (auto &v : exact.vertices)
        v.position = {10000, 10000, 10000};
      apply_character_pose(m, pose, exact);
      for (auto [a, b] : seams) {
        require(
            same(exact.vertices[a].position, exact.vertices[b].position, 1e-7f),
            "Uncached actual pose opens a sewn seam");
        const Vec3 d = posed.vertices[a].position - posed.vertices[b].position;
        max_cached_seam_gap = std::max(max_cached_seam_gap, length(d));
        require(std::fabs(d.x) <= 2e-6f && std::fabs(d.y) <= 2e-6f &&
                    std::fabs(d.z) <= 2e-6f,
                "Incremental pose seam exceeds the two-corner cache bound");
      }
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
      auto s = renn_sample(3, 9, m.rig.height);
      s.attention_yaw = yaw * .30f;
      s.attention_pitch = pitch * .2f;
      auto pose = sample_character_animation(m.rig, s);
      apply_character_pose(m, pose, posed);
      for (auto [a, b] : seams)
        require(
            same(posed.vertices[a].position, posed.vertices[b].position, 1e-7f),
            "Head turn opens source/cloth seam vertices " + std::to_string(a) +
                "/" + std::to_string(b) + " gap " +
                std::to_string(length(posed.vertices[a].position -
                                      posed.vertices[b].position)) +
                " yaw " + std::to_string(yaw) + " pitch " +
                std::to_string(pitch));
      ++poses;
    }
  std::cout << label(m) << " height " << m.rig.height << ": " << poses
            << " poses, " << seams.size()
            << " welded corner pairs, maximum cached seam gap/m "
            << max_cached_seam_gap << ", minimum area ratio " << min_area_ratio
            << '\n';
}

void hand_source_contract(const CharacterModel &m) {
  const auto &mesh = m.bind_mesh;
  const float h = m.rig.height;
  for (float side : {-1.f, 1.f}) {
    auto expected = [&](auto const &indices, auto const &uvs) {
      std::multiset<std::tuple<float, float, float, float, float>> corners;
      for (std::size_t k = 0; k < indices.size(); ++k) {
        auto p = renn_hands::positions[indices[k]];
        p.x *= side;
        const float a = side * 1.05f, c = std::cos(a), sn = std::sin(a);
        p = {c * p.x + sn * p.z, p.y, -sn * p.x + c * p.z};
        p += Vec3{side * .185f, -.043f, .018f};
        p = p * h;
        auto uv = renn_hands::uv_coordinates[uvs[k]];
        uv = character_surface_uv(CharacterSurface::Skin, uv.x, uv.y);
        corners.emplace(p.x, p.y, p.z, uv.x, uv.y);
      }
      return corners;
    };
    const auto need =
        m.lod == CharacterLod::Near
            ? expected(renn_hands::near_indices, renn_hands::near_uv_indices)
            : expected(renn_hands::far_indices, renn_hands::far_uv_indices);
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

struct ContactSet {
  std::vector<std::size_t> vest, shirt, collar, badge, neck, hands, pouch,
      trousers, left_knee, right_knee, radio, boots;
  explicit ContactSet(const CharacterModel &m) {
    const auto &mesh = m.bind_mesh;
    for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
      const auto &v = mesh.vertices[mesh.indices[t]];
      const int surface = surface_at(v.uv);
      Vec3 center{};
      for (int k = 0; k < 3; ++k)
        center += mesh.vertices[mesh.indices[t + k]].position *
                  (1 / (3 * m.rig.height));
      if (surface == int(CharacterSurface::Jacket))
        vest.push_back(t);
      const bool upper_cloth = surface == int(CharacterSurface::Shirt) &&
                               center.y > .290f && center.y < .390f;
      bool collar_color = false;
      for (float k : {1.f, .94f})
        collar_color |=
            upper_cloth && std::fabs(v.color.x - .16f * .94f * k) < 1e-5f;
      if (surface == int(CharacterSurface::Shirt) && v.color.x >= .1519f &&
          center.y > -.022f && center.y < .4f)
        shirt.push_back(t);
      if (collar_color)
        collar.push_back(t);
      if (surface == int(CharacterSurface::Metal) && center.y > .230f &&
          center.y < .280f)
        badge.push_back(t);
      if (surface == int(CharacterSurface::Face) && center.y > .355f)
        neck.push_back(t);
      if (surface == int(CharacterSurface::Skin) && center.y < .01f)
        hands.push_back(t);
      if (surface == int(CharacterSurface::Leather) && center.x < -.082f &&
          center.y > -.022f && center.y < .066f)
        pouch.push_back(t);
      if (surface == int(CharacterSurface::Rubber) && v.color.x < .07f &&
          center.y > .21f && center.y < .32f)
        radio.push_back(t);
      if (center.y < -.4f && (surface == int(CharacterSurface::Rubber) ||
                              surface == int(CharacterSurface::Leather) ||
                              surface == int(CharacterSurface::Shirt)))
        boots.push_back(t);
      if (surface == int(CharacterSurface::Pants))
        trousers.push_back(t);
      if (surface == int(CharacterSurface::Pants) && center.y < -.15f &&
          center.y > -.32f)
        (center.x < 0 ? left_knee : right_knee).push_back(t);
    }
    require(!vest.empty() && !shirt.empty() && !collar.empty() &&
                !badge.empty() && !hands.empty() && !pouch.empty() &&
                !left_knee.empty() && !right_knee.empty() && !radio.empty() &&
                !boots.empty(),
            "All contact classes contain real triangles");
  }
  std::size_t count(const Mesh &mesh, float h,
                    const std::vector<std::size_t> &aa,
                    const std::vector<std::size_t> &bb,
                    std::pair<std::size_t, std::size_t> &first) const {
    struct T {
      std::array<Vec3, 3> p;
      Bounds bounds;
      std::size_t offset;
    };
    auto triangles = [&](const auto &ids) {
      std::vector<T> out;
      for (auto i : ids) {
        T q{{}, {}, i};
        for (int k = 0; k < 3; ++k) {
          q.p[k] = mesh.vertices[mesh.indices[i + k]].position;
          q.bounds.add(q.p[k]);
        }
        out.push_back(q);
      }
      return out;
    };
    auto a = triangles(aa), b = triangles(bb);
    using Key = std::tuple<int, int, int>;
    std::map<Key, std::vector<unsigned>> grid;
    auto range = [&](const Bounds &q) {
      const float cell = .025f * h;
      std::array<int, 6> r{};
      r = {int(std::floor(q.low.x / cell)),  int(std::floor(q.low.y / cell)),
           int(std::floor(q.low.z / cell)),  int(std::floor(q.high.x / cell)),
           int(std::floor(q.high.y / cell)), int(std::floor(q.high.z / cell))};
      return r;
    };
    for (unsigned i = 0; i < b.size(); ++i) {
      auto r = range(b[i].bounds);
      for (int x = r[0]; x <= r[3]; ++x)
        for (int y = r[1]; y <= r[4]; ++y)
          for (int z = r[2]; z <= r[5]; ++z)
            grid[{x, y, z}].push_back(i);
    }
    std::size_t total = 0;
    for (const auto &p : a) {
      auto r = range(p.bounds);
      std::set<unsigned> candidates;
      for (int x = r[0]; x <= r[3]; ++x)
        for (int y = r[1]; y <= r[4]; ++y)
          for (int z = r[2]; z <= r[5]; ++z) {
            auto it = grid.find({x, y, z});
            if (it != grid.end())
              candidates.insert(it->second.begin(), it->second.end());
          }
      for (auto i : candidates) {
        const auto &q = b[i];
        if (p.bounds.low.x > q.bounds.high.x ||
            q.bounds.low.x > p.bounds.high.x ||
            p.bounds.low.y > q.bounds.high.y ||
            q.bounds.low.y > p.bounds.high.y ||
            p.bounds.low.z > q.bounds.high.z ||
            q.bounds.low.z > p.bounds.high.z)
          continue;
        bool crossed = false;
        for (unsigned k = 0; k < 3; ++k)
          crossed |= proper_segment_triangle(p.p[k], p.p[(k + 1) % 3], q.p[0],
                                             q.p[1], q.p[2], h) ||
                     proper_segment_triangle(q.p[k], q.p[(k + 1) % 3], p.p[0],
                                             p.p[1], p.p[2], h);
        if (crossed) {
          if (!total)
            first = {p.offset / 3, q.offset / 3};
          ++total;
        }
      }
    }
    return total;
  }
};

void check_contacts(const CharacterModel &m,
                    bool include_full_crouch_walk = true) {
  ContactSet c(m);
  Mesh posed = m.bind_mesh;
  const std::array<std::pair<const std::vector<std::size_t> *,
                             const std::vector<std::size_t> *>,
                   15>
      pairs{{{&c.vest, &c.shirt},
             {&c.collar, &c.shirt},
             {&c.badge, &c.vest},
             {&c.neck, &c.shirt},
             {&c.hands, &c.pouch},
             {&c.left_knee, &c.right_knee},
             {&c.hands, &c.shirt},
             {&c.hands, &c.vest},
             {&c.hands, &c.trousers},
             {&c.radio, &c.vest},
             {&c.radio, &c.shirt},
             {&c.hands, &c.radio},
             {&c.hands, &c.boots},
             {&c.trousers, &c.boots},
             {&c.neck, &c.collar}}};
  const std::array<const char *, 15> names{
      {"vest/shirt", "collar/shirt", "badge/vest", "visible neck/shirt",
       "hands/hip pouch", "left/right knee", "hands/shirt", "hands/vest",
       "hands/trousers", "radio/vest", "radio/shirt", "hands/radio",
       "hands/boots", "trousers/boots", "visible neck/collar"}};
  std::array<std::size_t, 15> worst{}, ordinary{}, stress{};
  std::array<std::string, 15> witness{};
  int poses = 0;
  for (int mode = 0; mode < 6; ++mode)
    for (int frame = 0; frame < 32; frame += 4) {
      if (mode == 5 && !include_full_crouch_walk)
        continue;
      apply_character_pose(m,
                           sample_character_animation(
                               m.rig, renn_sample(mode, frame, m.rig.height)),
                           posed);
      for (unsigned i = 0; i < pairs.size(); ++i) {
        std::pair<std::size_t, std::size_t> first{};
        const auto count = c.count(posed, m.rig.height, *pairs[i].first,
                                   *pairs[i].second, first);
        auto &group = mode == 5 ? stress : ordinary;
        group[i] = std::max(group[i], count);
        if (count > worst[i]) {
          worst[i] = count;
          witness[i] = "mode " + std::to_string(mode) + " frame " +
                       std::to_string(frame) + " triangles " +
                       std::to_string(first.first) + "/" +
                       std::to_string(first.second);
        }
      }
      ++poses;
    }
  for (int yaw = -3; yaw <= 3; ++yaw)
    for (int pitch = -2; pitch <= 2; ++pitch) {
      auto sample = renn_sample(3, 9, m.rig.height);
      sample.attention_yaw = yaw * .30f;
      sample.attention_pitch = pitch * .20f;
      apply_character_pose(m, sample_character_animation(m.rig, sample), posed);
      for (unsigned i = 0; i < pairs.size(); ++i) {
        std::pair<std::size_t, std::size_t> first{};
        const auto count = c.count(posed, m.rig.height, *pairs[i].first,
                                   *pairs[i].second, first);
        ordinary[i] = std::max(ordinary[i], count);
        if (count > worst[i]) {
          worst[i] = count;
          witness[i] = "attention yaw " + std::to_string(sample.attention_yaw) +
                       " pitch " + std::to_string(sample.attention_pitch) +
                       " triangles " + std::to_string(first.first) + "/" +
                       std::to_string(first.second);
        }
      }
      ++poses;
    }
  for (unsigned i = 0; i < pairs.size(); ++i)
    std::cout << label(m) << " " << names[i] << " " << worst[i]
              << " proper crossings in " << poses
              << " poses; ordinary/attention " << ordinary[i]
              << ", full crouch-walk stress " << stress[i] << "; " << witness[i]
              << '\n';
  auto broken = m.bind_mesh;
  bool moved = false;
  for (auto t : c.vest)
    for (int k = 0; k < 3 && !moved; ++k) {
      auto i = broken.indices[t + k];
      auto &p = broken.vertices[i].position;
      if (p.z > .07f * m.rig.height && p.y > .15f * m.rig.height &&
          p.y < .25f * m.rig.height) {
        p.z -= .05f * m.rig.height;
        moved = true;
      }
    }
  std::pair<std::size_t, std::size_t> first{};
  require(moved && c.count(broken, m.rig.height, c.vest, c.shirt, first) > 0,
          "Injected vest penetration must fail the actual contact predicate");
  std::cout << label(m) << " contact penetration negative control passed\n";
  for (unsigned i = 0; i < pairs.size(); ++i)
    require(worst[i] == 0,
            std::string(names[i]) + " penetration: " + witness[i]);
}

void check_cuff_coverage(const CharacterModel &m) {
  for (float side : {-1.f, 1.f}) {
    float skin_top = -1, cloth_low = 1;
    for (const auto &v : m.bind_mesh.vertices) {
      const Vec3 p = v.position * (1 / m.rig.height);
      if (side * p.x < .13f || p.y > .03f || p.y < -.10f)
        continue;
      if (surface_at(v.uv) == int(CharacterSurface::Skin))
        skin_top = std::max(skin_top, p.y);
      if (surface_at(v.uv) == int(CharacterSurface::Shirt))
        cloth_low = std::min(cloth_low, p.y);
    }
    require(skin_top > cloth_low + .005f,
            "Connected wrist skin must tuck at least .005 height units inside "
            "the real cuff");
  }
}

void check_uniform_cap_clearance(const CharacterModel &m) {
  std::vector<std::size_t> cap, face, hair;
  for (std::size_t t = 0; t < m.bind_mesh.indices.size(); t += 3) {
    const auto &v = m.bind_mesh.vertices[m.bind_mesh.indices[t]];
    const int surface = surface_at(v.uv);
    if (surface == int(CharacterSurface::Face))
      face.push_back(t);
    if (surface == int(CharacterSurface::HairCrop))
      hair.push_back(t);
    if (surface == int(CharacterSurface::Shirt) &&
        v.position.y > .43f * m.rig.height)
      cap.push_back(t);
  }
  require(!cap.empty() && !face.empty() && !hair.empty(),
          "Uniform cap contact classes populated");
  ContactSet classifier(m);
  std::pair<std::size_t, std::size_t> first{};
  const auto skin_hits =
      classifier.count(m.bind_mesh, m.rig.height, cap, face, first);
  require(skin_hits == 0, "Uniform cap crosses source scalp at triangles " +
                              std::to_string(first.first) + "/" +
                              std::to_string(first.second));
  const auto hair_hits =
      classifier.count(m.bind_mesh, m.rig.height, cap, hair, first);
  require(hair_hits == 0, "Uniform cap crosses opaque hair at triangles " +
                              std::to_string(first.first) + "/" +
                              std::to_string(first.second));
  // All these vertices have the identical rigid Head transform, so a bind
  // clearance is invariant over every runtime attention and locomotion pose.
  for (auto triangles : {&cap, &face, &hair})
    for (auto t : *triangles)
      for (int k = 0; k < 3; ++k) {
        const auto i = m.bind_mesh.indices[t + k];
        if (m.bind_mesh.vertices[i].position.y < .39f * m.rig.height)
          continue;
        const auto &w = m.influences[i];
        require(w.first == bone_index(CharacterBone::Head) &&
                    w.first_weight == 1,
                "Cap/scalp clearance must be invariant under rigid head pose");
      }
  std::cout << label(m) << " uniform cap: " << cap.size()
            << " crown/band/seam triangles; no source scalp or opaque hair "
               "crossing\n";
}

void check_physical_surface(const CharacterModel &m, int kind) {
  const auto &mesh = m.bind_mesh;
  const float h = m.rig.height;
  std::vector<Vec3> points;
  std::map<std::tuple<int, int, int>, std::vector<unsigned>> bins;
  auto weld = [&](Vec3 p) {
    p = p * (1 / h);
    int x = int(std::floor(p.x / .00002f)), y = int(std::floor(p.y / .00002f)),
        z = int(std::floor(p.z / .00002f));
    for (int a = -1; a <= 1; ++a)
      for (int b = -1; b <= 1; ++b)
        for (int c = -1; c <= 1; ++c) {
          auto it = bins.find({x + a, y + b, z + c});
          if (it != bins.end())
            for (auto i : it->second)
              if (length(points[i] - p) < .000005f)
                return i;
        }
    unsigned i = points.size();
    points.push_back(p);
    bins[{x, y, z}].push_back(i);
    return i;
  };
  std::map<std::pair<unsigned, unsigned>, std::pair<int, int>> edges;
  std::map<unsigned, std::set<unsigned>> adj;
  unsigned faces = 0;
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
    const auto &v = mesh.vertices[mesh.indices[t]];
    int surface = surface_at(v.uv);
    bool selected = false;
    Vec3 center{};
    for (int k = 0; k < 3; ++k)
      center += mesh.vertices[mesh.indices[t + k]].position * (1 / (3 * h));
    if (kind == 0)
      selected = surface == int(CharacterSurface::Jacket) &&
                 (std::fabs(v.color.x - .08f) < 1e-5f ||
                  std::fabs(v.color.x - .076f) < 1e-5f ||
                  std::fabs(v.color.x - .0624f) < 1e-5f);
    if (kind == 1)
      selected = surface == int(CharacterSurface::Shirt) &&
                 v.color.x >= .1519f &&
                 std::fabs(v.color.x - .16f * .97f) > .00001f &&
                 center.y < .4f && center.y > -.022f;
    if (kind == 2 || kind == 3)
      selected = surface == int(CharacterSurface::Skin) && center.y < .01f &&
                 (kind == 2 ? center.x < 0 : center.x > 0);
    if (kind == 4) {
      const bool leaf = std::fabs(v.color.x - .16f * .94f) < .00001f;
      selected =
          surface == int(CharacterSurface::Shirt) && leaf && center.y > .29f;
    }
    if (kind == 5)
      selected = surface == int(CharacterSurface::Pants) &&
                 std::fabs(v.color.x - .095f) < .00001f;
    if (kind == 6)
      selected = surface == int(CharacterSurface::Rubber) && center.y < -.4f;
    if (kind == 7)
      selected = surface == int(CharacterSurface::Leather) && center.y < -.4f &&
                 std::fabs(v.color.x - m.appearance.shoes.x) < .000001f;
    if (kind == 8)
      selected = surface == int(CharacterSurface::Shirt) && center.y > .43f &&
                 (std::fabs(v.color.x - .10f) < .000001f ||
                  std::fabs(v.color.x - .092f) < .000001f ||
                  std::fabs(v.color.x - .064f) < .000001f);
    if (kind == 9)
      selected = surface == int(CharacterSurface::Shirt) && center.y > .43f &&
                 std::fabs(v.color.x - .078f) < .000001f;
    if (kind == 10)
      selected = surface == int(CharacterSurface::Leather) && center.y > .43f;
    if (!selected)
      continue;
    std::array<unsigned, 3> q{};
    for (int k = 0; k < 3; ++k)
      q[k] = weld(mesh.vertices[mesh.indices[t + k]].position);
    require(q[0] != q[1] && q[0] != q[2] && q[1] != q[2],
            "Welded physical triangle collapses in surface " +
                std::to_string(kind));
    for (int k = 0; k < 3; ++k) {
      unsigned a = q[k], b = q[(k + 1) % 3];
      auto &e = edges[std::minmax(a, b)];
      ++e.first;
      e.second += a < b ? 1 : -1;
      adj[a].insert(b);
      adj[b].insert(a);
    }
    ++faces;
  }
  require(faces > 0, "Physical surface classification cannot be empty");
  unsigned boundary = 0, branches = 0, orientation = 0;
  std::map<unsigned, std::set<unsigned>> rim;
  for (auto e : edges) {
    branches += e.second.first > 2;
    if (e.second.first == 1) {
      ++boundary;
      rim[e.first.first].insert(e.first.second);
      rim[e.first.second].insert(e.first.first);
    } else
      orientation += e.second.second != 0;
  }
  unsigned components = 0, loops = 0;
  std::set<unsigned> remaining;
  for (auto e : adj)
    remaining.insert(e.first);
  while (!remaining.empty()) {
    ++components;
    std::vector<unsigned> todo{*remaining.begin()};
    remaining.erase(todo[0]);
    while (!todo.empty()) {
      auto a = todo.back();
      todo.pop_back();
      for (auto b : adj[a])
        if (remaining.erase(b))
          todo.push_back(b);
    }
  }
  for (auto e : rim) {
    require(e.second.size() == 2, "Surface boundary branches");
    remaining.insert(e.first);
  }
  while (!remaining.empty()) {
    ++loops;
    std::vector<unsigned> todo{*remaining.begin()};
    remaining.erase(todo[0]);
    while (!todo.empty()) {
      auto a = todo.back();
      todo.pop_back();
      for (auto b : rim[a])
        if (remaining.erase(b))
          todo.push_back(b);
    }
  }
  std::cout << label(m) << " physical surface " << kind << ": " << points.size()
            << " points, " << faces << " faces, " << boundary
            << " boundary edges/" << loops << " loops, " << components
            << " components, " << branches << " branched edges, " << orientation
            << " inconsistent edges, Euler "
            << long(points.size()) - long(edges.size()) + faces << '\n';
  require(branches == 0 && orientation == 0,
          "Surface has branched or inconsistently directed shared edges");
  if (kind == 0)
    require(boundary == 0 && components == 1 &&
                long(points.size()) - long(edges.size()) + faces == -4,
            "Protective vest closes its lining and preserves distinct neck, "
            "hem and both armholes");
  if (kind == 1)
    require(components == 1 && loops == 4,
            "Sewn security shirt has only neck, hem and two cuff openings");
  if (kind == 2 || kind == 3)
    require(components == 1 && loops == 1 && boundary == 20,
            "Source hand and wrist have only their 20-edge cuff loop");
  if (kind == 4)
    require(
        boundary == 0 && components == 1 &&
            long(points.size()) - long(edges.size()) + faces == 2,
        "The continuous rolled collar closes every root, lining and cut edge");
  if (kind == 5)
    require(components == 1 && loops == 3,
            "One connected trouser body has only waist and two ankle openings");
  if (kind == 6 || kind == 7)
    require(boundary == 0 && components == 2 &&
                long(points.size()) - long(edges.size()) + faces == 4,
            "Both service boot soles and uppers are complete closed shells");
  if (kind == 8)
    require(boundary == 0 && components == 1 &&
                long(points.size()) - long(edges.size()) + faces == 2,
            "Cap crown has a closed lining and finite root edge");
  if (kind == 9)
    require(boundary == 0 && components == 1 &&
                long(points.size()) - long(edges.size()) + faces == 0,
            "Cap woven band is a closed annular strip");
  if (kind == 10)
    require(boundary == 0 && components == 1 &&
                long(points.size()) - long(edges.size()) + faces == 2,
            "Cap visor is a closed finite-thickness shell");
}

void negative_controls(const CharacterProfile &profile) {
  const auto expect_rejection = [](const char *name, auto f) {
    bool rejected = false;
    try {
      f();
    } catch (const std::exception &) {
      rejected = true;
    }
    require(rejected, std::string("Negative control escaped: ") + name);
  };
  auto broken = profile.model;
  auto face =
      std::find_if(broken.bind_mesh.indices.begin(),
                   broken.bind_mesh.indices.end(), [&](unsigned i) {
                     return surface_at(broken.bind_mesh.vertices[i].uv) ==
                            int(CharacterSurface::Face);
                   });
  require(face != broken.bind_mesh.indices.end(),
          "Negative source fixture exists");
  const auto face_index = *face;
  broken.bind_mesh.vertices[face_index].position.x += .002f * broken.rig.height;
  expect_rejection("source position corruption", [&] { head(broken); });
  broken = profile.model;
  broken.bind_mesh.vertices[face_index].uv.x += .001f;
  expect_rejection("source UV corruption", [&] { head(broken); });
  broken = profile.model;
  for (auto &v : broken.bind_mesh.vertices)
    if (surface_at(v.uv) == int(CharacterSurface::Eyes))
      v.position.z -= .03f * broken.rig.height;
  expect_rejection("buried eyeballs", [&] { check_socket_fit(broken); });
  broken = profile.model;
  for (std::size_t t = 0; t < broken.bind_mesh.indices.size(); t += 3)
    if (surface_at(broken.bind_mesh.vertices[broken.bind_mesh.indices[t]].uv) ==
        int(CharacterSurface::HairCrop)) {
      broken.bind_mesh.indices.erase(broken.bind_mesh.indices.begin() + t,
                                     broken.bind_mesh.indices.begin() + t + 3);
      break;
    }
  expect_rejection("opaque scalp triangle hole",
                   [&] { check_crop_coverage(broken); });
  broken = profile.model;
  for (auto &v : broken.bind_mesh.vertices)
    if (surface_at(v.uv) == int(CharacterSurface::Skin) &&
        v.position.y < .01f * broken.rig.height)
      v.position.y = std::min(v.position.y, -.021f * broken.rig.height);
  expect_rejection("disconnected sleeve cuff",
                   [&] { check_cuff_coverage(broken); });
  broken = profile.model;
  bool moved_cap = false;
  for (auto &v : broken.bind_mesh.vertices)
    if (!moved_cap && surface_at(v.uv) == int(CharacterSurface::Shirt) &&
        std::fabs(v.color.x - .10f) < .000001f &&
        v.position.y > .43f * broken.rig.height) {
      v.position.x *= .5f;
      moved_cap = true;
    }
  require(moved_cap, "Uniform cap penetration fixture exists");
  expect_rejection("uniform cap embedded in scalp",
                   [&] { check_uniform_cap_clearance(broken); });
  broken = profile.model;
  bool removed_cap = false;
  for (std::size_t t = 0; t < broken.bind_mesh.indices.size(); t += 3) {
    const auto &v = broken.bind_mesh.vertices[broken.bind_mesh.indices[t]];
    if (surface_at(v.uv) == int(CharacterSurface::Shirt) &&
        std::fabs(v.color.x - .10f) < .000001f &&
        v.position.y > .43f * broken.rig.height) {
      broken.bind_mesh.indices.erase(broken.bind_mesh.indices.begin() + t,
                                     broken.bind_mesh.indices.begin() + t + 3);
      removed_cap = true;
      break;
    }
  }
  require(removed_cap, "Uniform cap closure fixture exists");
  expect_rejection("uniform cap crown hole",
                   [&] { check_physical_surface(broken, 8); });
  auto wrong = profile;
  wrong.model.bind_mesh.vertices.front().normal = {0, 0, 0};
  expect_rejection("zero normal", [&] { geometry(wrong); });
}
void fallback_contract() {
  for (auto lod : {CharacterLod::Near, CharacterLod::Far}) {
    for (const std::string id : {"unregistered_security", "unregistered_alley_security", "unregistered_metro_security",
                                 "npcGuard", "NpcDeskGuardOther"}) {
      const auto p =
          make_character_profile(1.82f, CharacterRole::Security, id, lod);
      const auto expected = make_character_model(1.82f, CharacterRole::Security,
                                                 character_seed(id), lod);
      require(!p.individualized &&
                  p.model.bind_mesh.indices == expected.bind_mesh.indices &&
                  p.model.bind_mesh.vertices.size() ==
                      expected.bind_mesh.vertices.size(),
              "Unrelated guard dispatch changed");
      for (std::size_t i = 0; i < expected.bind_mesh.vertices.size(); ++i) {
        const auto &a = p.model.bind_mesh.vertices[i],
                   &b = expected.bind_mesh.vertices[i];
        require(same(a.position, b.position, 0) &&
                    same(a.normal, b.normal, 0) && same(a.color, b.color, 0) &&
                    a.uv.x == b.uv.x && a.uv.y == b.uv.y,
                "Generic guard output changed");
      }
    }
    const auto wrong =
        make_character_profile(1.82f, CharacterRole::Dock, "NpcDeskGuard", lod);
    require(!wrong.individualized, "Renn identity cannot steal another role");
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
  const bool ordinary_only = argc == 2 && std::string(argv[1]) == "--ordinary";
  if (ordinary_only)
    std::cout << "EXPLICIT ORDINARY CONTACT SCOPE: all heights and LODs; full "
                 "crouch-walk contacts excluded; default stress assertions "
                 "remain unchanged. All 233 vertex/ground/normal animation "
                 "samples still run.\n";
  int failures = 0;
  auto run = [&](const std::string &name, auto f) {
    try {
      f();
      std::cout << "PASS " << name << '\n';
    } catch (const std::exception &e) {
      ++failures;
      std::cerr << "FAIL " << name << ": " << e.what() << '\n';
    }
  };
  run("generic guard and role dispatch", [] { fallback_contract(); });
  run("near-parallel contact negative controls",
      [] { contact_predicate_controls(); });
  for (float height : {1.35f, 1.82f, 2.10f})
    for (auto lod : {CharacterLod::Near, CharacterLod::Far}) {
      if ((argc == 3 || argc == 5) && height != 1.82f)
        continue;
      const auto p = make_character_profile(height, CharacterRole::Security,
                                            "NpcDeskGuard", lod);
      const auto &m = p.model;
      if (argc == 3 && std::string(argv[1]) == "--dump") {
        dump(m, std::string(argv[2]) +
                    (lod == CharacterLod::Near ? "-near.bin" : "-far.bin"));
        continue;
      }
      if (argc == 5 && std::string(argv[1]) == "--dump-pose") {
        auto posed = m;
        apply_character_pose(
            m,
            sample_character_animation(m.rig, renn_sample(std::stoi(argv[3]),
                                                          std::stoi(argv[4]),
                                                          m.rig.height)),
            posed.bind_mesh);
        dump(posed, std::string(argv[2]) +
                        (lod == CharacterLod::Near ? "-near.bin" : "-far.bin"));
        continue;
      }
      if (argc == 2 && std::string(argv[1]) == "--shells") {
        for (int k : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10})
          run(label(m) + " shell " + std::to_string(k),
              [&] { check_physical_surface(m, k); });
        continue;
      }
      if (argc == 2 && std::string(argv[1]) == "--contacts") {
        run(label(m) + " contacts", [&] { check_contacts(m); });
        continue;
      }
      if (argc == 2 && std::string(argv[1]) == "--cap") {
        run(label(m) + " cap", [&] { check_crop_coverage(m); });
        continue;
      }
      run(label(m) + " geometry", [&] { geometry(p); });
      if (height == 1.82f)
        for (int k : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10})
          run(label(m) + " shell " + std::to_string(k),
              [&] { check_physical_surface(m, k); });
      run(label(m) + " source head", [&] { head(m); });
      run(label(m) + " source hands", [&] { hand_source_contract(m); });
      run(label(m) + " cuff coverage", [&] { check_cuff_coverage(m); });
      run(label(m) + " sockets", [&] { check_socket_fit(m); });
      run(label(m) + " opaque scalp", [&] { check_crop_coverage(m); });
      run(label(m) + " uniform cap clearance",
          [&] { check_uniform_cap_clearance(m); });
      run(label(m) + " animation", [&] { check_animation(m); });
      if (height == 1.82f || ordinary_only)
        run(label(m) + (ordinary_only ? " ordinary garment contacts"
                                      : " garment contacts"),
            [&] { check_contacts(m, !ordinary_only); });
      if (height == 1.82f)
        run(label(m) + " independent negative controls",
            [&] { negative_controls(p); });
    }
  return failures ? 1 : 0;
}
