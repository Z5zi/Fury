#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/player_body_anatomy.hpp"
#include "fury/generated/player_body_hands.hpp"
#include "fury/material_sampling.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <new>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
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
TEST_NOINLINE void operator delete(void *p) noexcept {
  std::free(p);
}
TEST_NOINLINE void operator delete[](void *p) noexcept {
  std::free(p);
}
TEST_NOINLINE void operator delete(void *p, std::size_t) noexcept {
  std::free(p);
}
TEST_NOINLINE void operator delete[](void *p, std::size_t) noexcept {
  std::free(p);
}
namespace {
  using namespace fury;
  constexpr std::array<float, 3> heights{
    {
      1.35f, 1.75f, 2.10f
    }
  };
  constexpr std::size_t surface_count =
  static_cast<std::size_t>(CharacterSurface::Count);
  void require(bool condition, const std::string &message) {
    if (!condition)
    throw std::runtime_error(message);
  }
  // Dense-asset validation visits millions of vertices; construct diagnostic
  // strings only for a failed element so sanitizer runs stay inexpensive.
  void require_element(bool condition, const std::string &context,
  const char *message, std::size_t element) {
    if (!condition)
    throw std::runtime_error(context + message + std::to_string(element));
  }
  bool finite(Vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
  }
  bool exact(float a, float b) {
    return std::memcmp(&a, &b, sizeof(a)) == 0;
  }
  bool exact(Vec3 a, Vec3 b) {
    return exact(a.x, b.x) && exact(a.y, b.y) && exact(a.z, b.z);
  }
  bool exact(const CharacterInfluence &a, const CharacterInfluence &b) {
    return a.first == b.first && a.second == b.second && a.third == b.third &&
    exact(a.first_weight, b.first_weight) &&
    exact(a.third_weight, b.third_weight);
  }
  bool exact(const Vertex &a, const Vertex &b) {
    return exact(a.position, b.position) && exact(a.normal, b.normal) &&
    exact(a.color, b.color) && exact(a.uv.x, b.uv.x) &&
    exact(a.uv.y, b.uv.y) && exact(a.opacity, b.opacity);
  }
  bool exact(const Mesh &a, const Mesh &b) {
    if (a.indices != b.indices || a.vertices.size() != b.vertices.size())
    return false;
    for (std::size_t i = 0; i < a.vertices.size(); ++i)
    if (!exact(a.vertices[i], b.vertices[i]))
    return false;
    return true;
  }
  bool exact(const CharacterRig &a, const CharacterRig &b) {
    if (!exact(a.height, b.height))
    return false;
    for (std::size_t i = 0; i < character_bone_count; ++i)
    if (a.joints[i].parent != b.joints[i].parent ||
    !exact(a.joints[i].bind_position, b.joints[i].bind_position))
    return false;
    return true;
  }
  bool exact(const CharacterAppearance &a, const CharacterAppearance &b) {
    return exact(a.skin, b.skin) && exact(a.hair, b.hair) &&
    exact(a.eyes, b.eyes) && exact(a.shirt, b.shirt) &&
    exact(a.trousers, b.trousers) && exact(a.shoes, b.shoes) &&
    exact(a.accent, b.accent) &&
    exact(a.shoulder_scale, b.shoulder_scale) &&
    exact(a.waist_scale, b.waist_scale) &&
    exact(a.face_scale, b.face_scale) && a.hair_style == b.hair_style &&
    a.face_style == b.face_style;
  }
  bool exact(const CharacterModel &a, const CharacterModel &b) {
    if (!exact(a.bind_mesh, b.bind_mesh) || !exact(a.rig, b.rig) ||
    !exact(a.appearance, b.appearance) || a.seed != b.seed ||
    a.role != b.role || a.lod != b.lod ||
    a.dual_quaternion_skinning != b.dual_quaternion_skinning ||
    a.influences.size() != b.influences.size())
    return false;
    for (std::size_t i = 0; i < a.influences.size(); ++i)
    if (!exact(a.influences[i], b.influences[i]))
    return false;
    return true;
  }
  struct Bounds {
    Vec3 low{
      1000, 1000, 1000
    }, high{
      -1000, -1000, -1000
    };
    void add(Vec3 p) {
      low = {
        std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)
      };
      high = {
        std::max(high.x, p.x), std::max(high.y, p.y),
        std::max(high.z, p.z)
      };
    }
  };
  int surface_at(Vec2 uv) {
    if (!std::isfinite(uv.x) || !std::isfinite(uv.y))
    return -1;
    for (std::size_t i = 0; i < surface_count; ++i) {
      const auto s = static_cast<CharacterSurface>(i);
      const auto lo = character_surface_uv(s, 0, 0),
      hi = character_surface_uv(s, 1, 1);
      if (uv.x >= lo.x - 1.e-7f && uv.x <= hi.x + 1.e-7f &&
      uv.y >= lo.y - 1.e-7f && uv.y <= hi.y + 1.e-7f)
      return static_cast<int>(i);
    }
    return -1;
  }
  std::string label(const CharacterModel &m) {
    return std::string(m.lod == CharacterLod::Near ? "near" : "far") +
    " height " + std::to_string(m.rig.height);
  }
  void check_geometry(const CharacterProfile &profile) {
    const auto &m = profile.model;
    const auto &mesh = m.bind_mesh;
    const float h = m.rig.height;
    const auto context = label(m) + ": ";
    require(profile.individualized && m.role == CharacterRole::Player &&
    m.seed == 0x724a6d0cu && m.seed == character_seed("PlayerBody"),
    context + "actual PlayerBody identity/role");
    require(!mesh.vertices.empty() && !mesh.indices.empty() &&
    mesh.indices.size() % 3 == 0,
    context + "nonempty indexed triangles");
    require(m.influences.size() == mesh.vertices.size(),
    context + "one influence per vertex");
    Bounds bounds;
    std::array<std::size_t, surface_count> surfaces{};
    std::array<bool, character_bone_count> used_bones{};
    std::array<float, 2> soles{
      {
        h, h
      }
    };
    std::vector<int> vertex_surfaces;
    vertex_surfaces.reserve(mesh.vertices.size());
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
      const auto &v = mesh.vertices[i];
      const auto &w = m.influences[i];
      const auto where = context + "vertex " + std::to_string(i);
      require(finite(v.position) && finite(v.normal) && finite(v.color),
      where + " is finite");
      require(std::fabs(length(v.normal) - 1.f) < .0002f,
      where + " has a unit normal");
      require(v.color.x >= 0 && v.color.y >= 0 && v.color.z >= 0 &&
      v.color.x <= 1 && v.color.y <= 1 && v.color.z <= 1 &&
      exact(v.opacity, 1.f),
      where + " has bounded opaque material values");
      const int s = surface_at(v.uv);
      require(s >= 0,
      where + " UV is inside an atlas content region, beyond its gutter");
      vertex_surfaces.push_back(s);
      ++surfaces[static_cast<std::size_t>(s)];
      require(w.first < character_bone_count && w.second < character_bone_count &&
      w.third < character_bone_count &&
      std::isfinite(w.first_weight) &&
      std::isfinite(w.third_weight) && w.first_weight >= 0 &&
      w.third_weight >= 0 && w.first_weight + w.third_weight <= 1.f,
      where + " has valid normalized bone weights");
      if (w.first_weight > 0)
      used_bones[w.first] = true;
      if (w.first_weight + w.third_weight < 1)
      used_bones[w.second] = true;
      if (w.third_weight > 0)
      used_bones[w.third] = true;
      if (s == static_cast<int>(CharacterSurface::HairShortStrands))
      require(w.first == bone_index(CharacterBone::Head) &&
      w.first_weight == 1 && w.third_weight == 0,
      where + " strand card remains rigidly attached to the skull");
      if (w.first == bone_index(CharacterBone::LeftFoot))
      soles[0] = std::min(soles[0], v.position.y);
      if (w.first == bone_index(CharacterBone::RightFoot))
      soles[1] = std::min(soles[1], v.position.y);
      bounds.add(v.position);
    }
    // PlayerBody's wardrobe and groom have an explicit material contract; adding an
    // unrelated atlas region must not require every actor to use that region.
    for (auto surface : {
      CharacterSurface::Face,
      (player_source_hair_enabled() ? CharacterSurface::HairSwept : CharacterSurface::Hair),
      (player_source_hair_enabled() ? (player_short_tail_enabled() ? CharacterSurface::HairBundle : CharacterSurface::HairSourceFold) : CharacterSurface::HairCrop),
      CharacterSurface::Skin, CharacterSurface::Shirt,
      CharacterSurface::Pants, CharacterSurface::Leather,
      CharacterSurface::Eyes, CharacterSurface::Rubber,
      (player_source_hair_enabled() ? CharacterSurface::HairBundle : CharacterSurface::HairShortStrands)
    }) {
      const auto i = static_cast<std::size_t>(surface);
      require(surfaces[i] > 0,
      context + "modeled atlas surface " + std::to_string(i));
    }
    const auto card_vertices =
    surfaces[static_cast<std::size_t>(player_source_hair_enabled() ? CharacterSurface::HairBundle : CharacterSurface::HairShortStrands)];
    require(card_vertices > 0,
    context + "both LODs retain real strand cutout geometry");
    for (bool used : used_bones)
    require(used, context + "every rig joint deforms actual vertices");
    const Vec3 extent = (bounds.high - bounds.low) * (1.f / h);
    require(extent.x > .28f && extent.x < .50f && extent.z > .12f &&
    extent.z < .25f,
    context + "sensible nonzero body, fingers, vest and shoe extents");
    require(std::fabs(bounds.low.y / h + .5f) < .00001f &&
    std::fabs(bounds.high.y / h - .5f) < .002f,
    context + "requested height is preserved from ground to hair crown");
    for (float sole : soles)
    require(std::fabs(sole / h + .5f) < .00001f,
    context + "both feet have grounded sole geometry");
    double volume = 0;
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
      const auto a = mesh.indices[i], b = mesh.indices[i + 1],
      c = mesh.indices[i + 2];
      const auto where = context + "triangle " + std::to_string(i / 3);
      require(a < mesh.vertices.size() && b < mesh.vertices.size() &&
      c < mesh.vertices.size(),
      where + " indices in range");
      require(a != b && a != c && b != c,
      where + " three distinct vertex indices");
      require(vertex_surfaces[a] == vertex_surfaces[b] &&
      vertex_surfaces[b] == vertex_surfaces[c],
      where + " cannot interpolate across material atlas regions");
      const auto &va = mesh.vertices[a];
      const auto &vb = mesh.vertices[b];
      const auto &vc = mesh.vertices[c];
      const auto face =
      cross(vb.position - va.position, vc.position - va.position);
      require(length(face) > h * h * 1.e-12f, where + " nondegenerate geometry");
      require(dot(face, va.normal + vb.normal + vc.normal) > 0,
      where + " winding disagrees with vertex normals, material " +
      std::to_string(vertex_surfaces[a]) + " at " +
      std::to_string(va.position.x / h) + "," +
      std::to_string(va.position.y / h) + "," +
      std::to_string(va.position.z / h));
      volume += dot(va.position, cross(vb.position, vc.position)) / 6.;
    }
    // This sums authored overlapping shells, not a boolean union/body-volume
    // measurement.
    require(volume > h * h * h * .003 && volume < h * h * h * .07,
    context +
    "outward shells enclose a positive plausible signed volume");
    for (std::size_t i = 0; i < character_bone_count; ++i) {
      const auto &joint = m.rig.joints[i];
      require(joint.parent >= -1 && joint.parent < static_cast<int>(i) &&
      finite(joint.bind_position),
      context + "finite acyclic parent-first rig");
    }
    std::cout << context << mesh.vertices.size() << " vertices / "
    << mesh.indices.size() / 3 << " triangles, extent " << extent.x
    << "," << extent.y << "," << extent.z
    << ", signed shell volume/h^3 " << volume / (h * h * h) << '\n';
  }
  void check_anatomical_head(const CharacterModel &model) {
    const float h = model.rig.height;
    const auto context = label(model) + " anatomical head: ";
    require(
    length(model.rig.joints[bone_index(CharacterBone::Head)].bind_position -
    player_body_anatomy::head_center * h) < h * 1.e-6f &&
    length(
    model.rig.joints[bone_index(CharacterBone::Neck)].bind_position -
    player_body_anatomy::neck_center * h) < h * 1.e-6f,
    context + "head and neck rig pivots use the morphed source landmarks");
    std::map<std::array<float, 3>, std::uint32_t> source_positions;
    for (std::uint32_t i = 0; i < player_body_anatomy::positions.size(); ++i) {
      const auto p = player_body_anatomy::positions[i] * h;
      source_positions.emplace(std::array<float, 3>{
        {
          p.x, p.y, p.z
        }
      }, i);
    }
    require(source_positions.size() == player_body_anatomy::positions.size(),
    context + "source geometry has unambiguous canonical positions");
    std::vector<std::uint32_t> canonical(model.bind_mesh.vertices.size(),
    UINT32_MAX);
    std::vector<std::uint32_t> first_copy(player_body_anatomy::positions.size(),
    UINT32_MAX);
    std::vector<bool> retained(player_body_anatomy::positions.size(), false);
    std::array<Bounds, 2> eye_bounds;
    std::array<std::size_t, 2> eye_vertices{};
    for (std::size_t i = 0; i < model.bind_mesh.vertices.size(); ++i) {
      const auto &v = model.bind_mesh.vertices[i];
      const auto surface = surface_at(v.uv);
      if (surface == static_cast<int>(CharacterSurface::Face)) {
        const auto found =
        source_positions.find({
          {
            v.position.x, v.position.y, v.position.z
          }
        });
        require(found != source_positions.end(),
        context + "every face vertex preserves a source position");
        canonical[i] = found->second;
        retained[found->second] = true;
        auto &first = first_copy[found->second];
        if (first == UINT32_MAX)
        first = static_cast<std::uint32_t>(i);
        else {
          require(length(v.normal - model.bind_mesh.vertices[first].normal) <
          1.e-5f &&
          exact(model.influences[i], model.influences[first]),
          context + "UV seam copies preserve smooth normals and "
          "identical skin weights");
        }
        if (player_body_anatomy::head_region[found->second]) {
          const auto &influence = model.influences[i];
          require(influence.first == bone_index(CharacterBone::Head) &&
          influence.first_weight == 1,
          context + "facial features above the neck blend remain rigidly "
          "attached to the skull");
        }
      } else if (surface == static_cast<int>(CharacterSurface::Eyes)) {
        const auto side = v.position.x > 0 ? 0u : 1u;
        eye_bounds[side].add(v.position);
        ++eye_vertices[side];
        require(model.influences[i].first == bone_index(CharacterBone::Head) &&
        model.influences[i].first_weight == 1,
        context + "eyeballs and iris geometry follow the same head "
        "transform as sockets");
      }
    }
    for (bool exists : retained)
    require(exists,
    context + "no source head position is dropped in either LOD");
    std::vector<std::uint32_t> actual_indices;
    const auto face_uv_min = character_surface_uv(CharacterSurface::Face, 0, 0);
    const auto face_uv_max = character_surface_uv(CharacterSurface::Face, 1, 1);
    for (std::size_t i = 0; i < model.bind_mesh.indices.size(); i += 3) {
      const auto a = model.bind_mesh.indices[i],
      b = model.bind_mesh.indices[i + 1],
      c = model.bind_mesh.indices[i + 2];
      if (canonical[a] == UINT32_MAX)
      continue;
      require(canonical[b] != UINT32_MAX && canonical[c] != UINT32_MAX,
      context + "anatomy triangles use only preserved anatomy positions");
      const float u0 = model.bind_mesh.vertices[a].uv.x,
      u1 = model.bind_mesh.vertices[b].uv.x,
      u2 = model.bind_mesh.vertices[c].uv.x;
      require_element(
      std::max({
        u0, u1, u2
      }) - std::min({
        u0, u1, u2
      }) <=
      (face_uv_max.x - face_uv_min.x) * .5f,
      context,
      "source UV chart triangle stays within half the Face region: ", i / 3);
      const auto source_corner = actual_indices.size();
      require(source_corner + 2 < player_body_anatomy::uv_indices.size(),
      context + "no added anatomy triangle");
      const std::uint32_t corners[]{
        a, b, c
      };
      for (std::size_t corner = 0; corner < 3; ++corner) {
        const auto uv =
        player_body_anatomy::uv_coordinates[player_body_anatomy::uv_indices[source_corner +
        corner]];
        const auto expected =
        character_surface_uv(CharacterSurface::Face, uv.x, uv.y);
        const auto actual = model.bind_mesh.vertices[corners[corner]].uv;
        require(exact(actual.x, expected.x) && exact(actual.y, expected.y),
        context + "original per-corner UV seams survive atlas mapping");
      }
      actual_indices.insert(actual_indices.end(),
      {
        canonical[a], canonical[b], canonical[c]
      });
    }
    require(
    actual_indices.size() == player_body_anatomy::indices.size() &&
    std::equal(actual_indices.begin(), actual_indices.end(),
    player_body_anatomy::indices.begin()),
    context +
    "all source triangles survive intact and in their original winding");
    for (std::size_t side = 0; side < 2; ++side) {
      require(eye_vertices[side] > 0,
      context + "both source eye sockets contain actual eye geometry");
      const auto center = player_body_anatomy::eye_centers[side] * h;
      const auto helper = player_body_anatomy::eye_helper_half_extents[side] * h;
      const auto &bounds = eye_bounds[side];
      const auto middle = (bounds.low + bounds.high) * .5f;
      require(std::fabs(middle.x - center.x) < h * .002f &&
      std::fabs(middle.y - center.y) < h * .0015f,
      context + "eyeballs remain in the independent source helper "
      "neighborhood; actual rims are tested separately");
      require(std::fabs(bounds.low.x - (center.x - helper.x)) < h * .0015f &&
      std::fabs(bounds.low.y - (center.y - helper.y)) < h * .0015f &&
      std::fabs(bounds.low.z - (center.z - helper.z)) < h * .004f &&
      bounds.high.z > center.z &&
      bounds.high.z < center.z + helper.z + h * .002f,
      context +
      "globe size and iris layers fit the source helper volume");
    }
  }
  // These modeled material regions are closed authored shells. Weld only
  // floating-point coordinate drift at UV seams, then require every edge to
  // close. This catches non-periodic angular noise that opens or overlaps the
  // u=0/1 seam.
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
  struct Component {
    std::vector<unsigned> vertices;
    std::vector<std::size_t> triangles;
    std::vector<std::pair<unsigned, unsigned>> copies;
    std::vector<std::pair<unsigned, unsigned>> boundary;
    double volume{};
    int euler{};
  };
  // Infer components from actual positions, irrespective of construction order,
  // texture corners or current tessellation. Physical seams are quantized only
  // below a micrometre at canonical height, not across garment thickness.
  std::vector<Component> components(const CharacterModel &model,
  CharacterSurface surface) {
    const auto &m = model.bind_mesh;
    const float h = model.rig.height;
    std::vector<unsigned> parent(m.vertices.size()),
    canonical(m.vertices.size(), UINT32_MAX);
    for (unsigned i = 0; i < parent.size(); ++i)
    parent[i] = i;
    auto root = [&](unsigned i) {
      while (parent[i] != i) {
        parent[i] = parent[parent[i]];
        i = parent[i];
      }
      return i;
    };
    auto join = [&](unsigned a, unsigned b) {
      parent[root(b)] = root(a);
    };
    using Key = std::array<long long, 3>;
    std::map<Key, unsigned> unique;
    for (unsigned i = 0; i < m.vertices.size(); ++i)
    if (surface_at(m.vertices[i].uv) == int(surface)) {
      auto p = m.vertices[i].position * (1 / h);
      Key k{
        {
          std::llround(p.x * 1e7), std::llround(p.y * 1e7),
          std::llround(p.z * 1e7)
        }
      };
      canonical[i] = unique.emplace(k, i).first->second;
      join(i, canonical[i]);
    }
    for (std::size_t t = 0; t < m.indices.size(); t += 3)
    if (canonical[m.indices[t]] != UINT32_MAX) {
      join(m.indices[t], m.indices[t + 1]);
      join(m.indices[t], m.indices[t + 2]);
    }
    std::map<unsigned, Component> result;
    for (unsigned i = 0; i < m.vertices.size(); ++i)
    if (canonical[i] != UINT32_MAX) {
      auto &c = result[root(i)];
      c.vertices.push_back(i);
      if (canonical[i] != i)
      c.copies.push_back({
        i, canonical[i]
      });
    }
    for (std::size_t t = 0; t < m.indices.size(); t += 3)
    if (canonical[m.indices[t]] != UINT32_MAX)
    result[root(m.indices[t])].triangles.push_back(t);
    std::vector<Component> out;
    for (auto &entry : result) {
      auto &c = entry.second;
      std::map<std::pair<unsigned, unsigned>, std::pair<int, int>> edges;
      std::set<unsigned> verts;
      for (auto t : c.triangles) {
        unsigned ids[3];
        for (int k = 0; k < 3; ++k) {
          ids[k] = canonical[m.indices[t + k]];
          verts.insert(ids[k]);
        }
        require(ids[0] != ids[1] && ids[0] != ids[2] && ids[1] != ids[2],
        "Physical garment weld cannot collapse triangles");
        auto a = m.vertices[m.indices[t]].position,
        b = m.vertices[m.indices[t + 1]].position,
        d = m.vertices[m.indices[t + 2]].position;
        c.volume += dot(a, cross(b, d)) / 6.;
        for (int k = 0; k < 3; ++k) {
          unsigned a = ids[k], b = ids[(k + 1) % 3];
          auto &e = edges[std::minmax(a, b)];
          ++e.first;
          e.second += a < b ? 1 : -1;
        }
      }
      for (auto e : edges) {
        require(e.second.first <= 2,
        "Actual garment component is manifold surface=" +
        std::to_string(int(surface)) +
        " edge=" + std::to_string(e.first.first) + "," +
        std::to_string(e.first.second) +
        " count=" + std::to_string(e.second.first));
        if (e.second.first == 2)
        require(e.second.second == 0,
        "Actual sewn faces have consistent winding surface=" +
        std::to_string(int(surface)));
        else
        c.boundary.push_back(e.first);
      }
      c.euler = int(verts.size()) - int(edges.size()) + int(c.triangles.size());
      out.push_back(std::move(c));
    }
    std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) {
      return a.triangles.size() > b.triangles.size();
    });
    return out;
  }
  void check_hands(const CharacterModel &m) {
    const float h = m.rig.height;
    std::array<unsigned, 2> retained{};
    for (float side : {
      -1.f, 1.f
    }) {
      const auto bone = bone_index(side > 0 ? CharacterBone::RightHand
      : CharacterBone::LeftHand);
      std::map<std::array<float, 3>, unsigned> source;
      const float a = side * (68.f * 3.14159265358979323846f / 180.f),
      cs = std::cos(a), sn = std::sin(a);
      for (unsigned i = 0; i < player_body_hands::positions.size(); ++i) {
        Vec3 p = player_body_hands::positions[i];
        p.x *= side;
        p = {
          p.x * cs + p.z * sn, p.y, -p.x * sn + p.z * cs
        };
        p += Vec3{
          side * .173f, -.039f, .008f
        };
        p = p * h;
        source[{
          {
            p.x, p.y, p.z
          }
        }] = i;
      }
      std::set<unsigned> used;
      unsigned triangles = 0;
      for (std::size_t t = 0; t < m.bind_mesh.indices.size(); t += 3) {
        std::array<unsigned, 3> ids{};
        bool hand = true;
        for (int k = 0; k < 3; ++k) {
          unsigned v = m.bind_mesh.indices[t + k];
          auto p = m.bind_mesh.vertices[v].position;
          const auto &w = m.influences[v];
          auto found = source.find({
            {
              p.x, p.y, p.z
            }
          });
          if (found == source.end() ||
          surface_at(m.bind_mesh.vertices[v].uv) !=
          int(CharacterSurface::Skin) ||
          w.first != bone || w.first_weight != 1) {
            hand = false;
            break;
          }
          ids[k] = found->second;
        }
        if (hand) {
          ++triangles;
          for (auto i : ids)
          used.insert(i);
        }
      }
      const auto expected = m.lod == CharacterLod::Near
      ? player_body_hands::near_indices.size() / 3
      : player_body_hands::far_indices.size() / 3;
      require(
      triangles == expected,
      "Every expected connected hand triangle survives rigid fit; actual=" +
      std::to_string(triangles) +
      " expected=" + std::to_string(expected));
      for (auto i : player_body_hands::wrist_boundary_indices)
      require(used.count(i), "Complete anatomical wrist retained in each LOD");
      retained[side > 0 ? 1 : 0] = used.size();
    }
    require(retained[0] == retained[1] && retained[0] > 200,
    "Both source-correspondent hands have matching topology");
  }
  void generic_identity_fallback() {
    struct Identity {
      const char *id;
      CharacterRole role;
    };
    using R = CharacterRole;
    // Unknown identities exercise every role without making an assertion that
    // another named roster actor must remain generic in future individual work.
    constexpr Identity roster[]{
      {
        "unregistered_commuter", R::Commuter
      },
      {
        "unregistered_market", R::Market
      },
      {
        "unregistered_dock", R::Player
      },
      {
        "unregistered_security", R::Security
      },
      {
        "unregistered_fence", R::Fence
      },
      {
        "unregistered_scout", R::CrewScout
      },
      {
        "unregistered_tech", R::CrewTech
      },
      {
        "unregistered_enforcer", R::Enforcer
      },
      {
        "unregistered_player", R::Player
      },
      {
        "unregistered_ghost", R::Ghost
      },
      {
        "unregistered_staff", R::BankStaff
      },
      {
        "playerbody", R::Player
      },
      {
        "PlayerBody_extra", R::Player
      },
      {
        "", R::Player
      }
    };
    std::size_t cases = 0;
    auto check = [&](float h, R role, std::string_view id, CharacterLod lod) {
      const auto profile = make_character_profile(h, role, id, lod);
      const auto generic = make_character_model(h, role, character_seed(id), lod);
      require(
      !profile.individualized && profile.lod_distance == 0 &&
      !profile.material.textures,
      "Unregistered identity/role retains default generic profile behavior");
      require(exact(profile.model, generic),
      "Generic fallback preserves every geometry/rig/weight/appearance "
      "bit: " +
      std::string(id) + " " + character_role_name(role));
      ++cases;
    };
    for (float h : heights)
    for (auto lod : {
      CharacterLod::Near, CharacterLod::Far
    }) {
      for (const auto &identity : roster)
      check(h, identity.role, identity.id, lod);
      for (unsigned r = 0; r < static_cast<unsigned>(R::Count); ++r)
      if (static_cast<R>(r) != R::Player)
      check(h, static_cast<R>(r), "PlayerBody", lod);
      check(h, R::Player, std::string_view("PlayerBody\0extra", 13), lod);
    }
    std::cout << cases
    << " bit-exact generic identity/role/height/LOD fallback cases\n";
  }
  void check_materials(const CharacterProfile &near,const CharacterProfile &far) {
    const auto &a=near.model,&b=far.model;
    require(exact(a.appearance,character_appearance(character_seed("PlayerBody"))),"Every original palette and identity parameter is exact");
    require(exact(a.rig,b.rig)&&exact(a.appearance,b.appearance),"LOD identity and rig are exact");
    require(a.bind_mesh.indices.size()/3<=40000&&b.bind_mesh.indices.size()/3<=16000,"Strict 40000/16000 triangle cap");
    require(near.material.textures&&near.material.textures==far.material.textures,"LOD shares immutable character surfaces");
    require(near.material.alpha_cutoff==.4f&&far.material.alpha_cutoff==.4f,"Hair mask contract");
    require(near.lod_distance==12&&far.lod_distance==12,"Authored LOD threshold");
  }
  void check_sewn_topology(const CharacterModel &m) {
    for(auto surface:{
      CharacterSurface::Shirt,CharacterSurface::Pants
    }) {
      const auto c=components(m,surface);
      require(!c.empty(),"Actual garment exists");
      const auto &g=c.front();
      std::map<unsigned,std::vector<unsigned>> edges;
      for(auto e:g.boundary){
        edges[e.first].push_back(e.second);
        edges[e.second].push_back(e.first);
      }
      std::set<unsigned> unseen;
      for(auto e:edges){
        require(e.second.size()==2,"Unbranched garment boundary");
        unseen.insert(e.first);
      }
      unsigned loops=0;
      while(!unseen.empty()) {
        ++loops;
        std::vector<unsigned> stack{
          *unseen.begin()
        };
        unseen.erase(stack[0]);
        while(!stack.empty()){
          auto p=stack.back();
          stack.pop_back();
          for(auto q:edges[p])if(unseen.erase(q))stack.push_back(q);
        }
      }
      require(loops==(surface==CharacterSurface::Shirt?4u:3u),"Sewn shirt has neck/hem/cuffs and trousers waist/ankles only");
      for(auto pair:g.copies)require(exact(m.influences[pair.first],m.influences[pair.second]),"Physical garment seams retain exact weights");
    }
    const auto leather=components(m,CharacterSurface::Leather);
    bool left_shoe=false,right_shoe=false,sling=false,pouch=false;
    for(const auto &c:leather)if(c.boundary.empty()){
      Bounds b;
      for(auto i:c.vertices)b.add(m.bind_mesh.vertices[i].position*(1/m.rig.height));
      if(b.high.y<-.40f&&b.high.z-b.low.z>.10f){
        require(c.volume>0,"Closed shoe has outward positive volume");
        if(b.high.x<0)left_shoe=true;
        else if(b.low.x>0)right_shoe=true;
      }
      if(b.low.y>.05f&&b.high.y>.32f&&b.high.z-b.low.z>.12f){
        require(c.volume>0&&c.euler==0,"Sling is an outward closed loop");
        sling=true;
      }
      if(b.low.y>.10f&&b.high.y<.29f&&b.high.x-b.low.x>.10f&&b.high.z<-.04f){
        require(c.volume>0&&c.euler==2,"Pouch is an outward closed shell");
        pouch=true;
      }
    }
    require(left_shoe&&right_shoe&&sling&&pouch,"Both shoes, complete sling and compact pouch have closed construction");
  }
  void check_animation_and_jaw(const CharacterModel &m) {
    const auto &bind=m.bind_mesh;
    Mesh posed=bind;
    const auto *vertices=posed.vertices.data(),*dummy=vertices;
    (void)dummy;
    const auto *indices=posed.indices.data();
    const auto id=posed.geometry_identity;
    std::map<std::array<float,3>,unsigned> source;
    for(unsigned i=0;i<player_body_anatomy::positions.size();++i){
      auto p=player_body_anatomy::positions[i]*m.rig.height;
      source[{
        p.x,p.y,p.z
      }]=i;
    }
    std::vector<unsigned> rigid;
    for(unsigned i=0;i<bind.vertices.size();++i)if(surface_at(bind.vertices[i].uv)==int(CharacterSurface::Face)) {
      auto p=bind.vertices[i].position;
      auto found=source.find({
        p.x,p.y,p.z
      });
      require(found!=source.end(),"Source head position known");
      if(player_body_anatomy::head_region[found->second])rigid.push_back(i);
    }
    require(rigid.size()>4000,"Complete external head, jaw, mouth and sockets covered");
    unsigned samples=0;
    for(int mode=0;mode<6;++mode)for(int phase=0;phase<33;++phase) {
      CharacterAnimationSample state{};
      state.seed=m.seed;
      state.phase_cycles=float(phase)/32;
      state.idle_time=float(phase)/10;
      state.speed=mode==1?m.rig.height*.85f:mode==2?m.rig.height*2.6f:mode==5?m.rig.height*.55f:0;
      state.move_weight=(mode==1||mode==2||mode==5)?1:0;
      state.crouch_weight=mode>=4?1:0;
      state.talk_weight=mode==3?1:0;
      state.attention_weight=mode==3?1:0;
      state.attention_yaw=mode==3?(phase%2?.9f:-.9f):0;
      state.attention_pitch=mode==3?(phase%3?.4f:-.4f):0;
      const auto pose=sample_character_animation(m.rig,state);
      allocations=0;
      count_allocations=true;
      apply_character_pose(m,pose,posed);
      count_allocations=false;
      require(allocations==0,"Production pose makes no allocation");
      require(vertices==posed.vertices.data()&&indices==posed.indices.data()&&id==posed.geometry_identity,"Stable mesh/storage ownership");
      const auto rev=posed.geometry_revision;
      require(!apply_character_pose(m,pose,posed)&&posed.geometry_revision==rev,"Repeated pose is clean");
      for(unsigned i=0;i<posed.vertices.size();++i){
        const auto &v=posed.vertices[i];
        require_element(finite(v.position)&&finite(v.normal)&&std::fabs(length(v.normal)-1)<.0002f,"pose","Finite vertex and unit normal",i);
        require_element(exact(v.color,bind.vertices[i].color)&&exact(v.uv.x,bind.vertices[i].uv.x)&&exact(v.uv.y,bind.vertices[i].uv.y),"pose","Material identity preserved",i);
      }
      for(std::size_t t=0;t<posed.indices.size();t+=3){
        const auto a=posed.vertices[posed.indices[t]].position,
        b=posed.vertices[posed.indices[t+1]].position,
        c=posed.vertices[posed.indices[t+2]].position;
        require_element(length(cross(b-a,c-a))>m.rig.height*m.rig.height*1e-13f,"pose","Actual posed triangle remains noncollapsed",t/3);
      }
      for(unsigned i:rigid) {
        const auto expected=transform_point(pose.skin_matrices[bone_index(CharacterBone::Head)],bind.vertices[i].position);
        require_element(length(posed.vertices[i].position-expected)<m.rig.height*2e-6f,"pose","Full jaw/mouth follows Head",i);
      }
      ++samples;
    }
    apply_character_pose(m,CharacterPose{},posed);
    for(unsigned i=0;i<posed.vertices.size();++i)require_element(length(posed.vertices[i].position-bind.vertices[i].position)<m.rig.height*2e-6f,"reset","Bind reset preserves source positions",i);
    std::cout<<label(m)<<": "<<samples<<" full production poses and jaw/mouth transforms validated\n";
  }
} // namespace
int main(int argc,char **argv) {
  bool quick=argc>1&&std::string(argv[1])=="--quick";
  int failures=0;
  auto run=[&](std::string label,auto fn){
    try{
      fn();
    }catch(const std::exception&e){
      ++failures;
      std::cerr<<label<<": "<<e.what()<<'\n';
    }
  };
  run("fallback",[]{
    generic_identity_fallback();
  });
  for(float h:heights) {
    if(quick&&h!=1.75f)continue;
    auto near=make_character_profile(h,CharacterRole::Player,"PlayerBody",CharacterLod::Near),far=make_character_profile(h,CharacterRole::Player,"PlayerBody",CharacterLod::Far);
    run("materials",[&]{
      check_materials(near,far);
    });
    for(const auto *p:{
      &near,&far
    }) {
      const auto &m=p->model;
      run(label(m)+" geometry",[&]{
        check_geometry(*p);
      });
      run(label(m)+" source",[&]{
        check_anatomical_head(m);
      });
      run(label(m)+" hands",[&]{
        check_hands(m);
      });
      run(label(m)+" garments",[&]{
        check_sewn_topology(m);
      });
      run(label(m)+" repeat",[&]{
        auto other=make_character_profile(h,CharacterRole::Player,"PlayerBody",m.lod);require(exact(m,other.model),"Repeat construction is deterministic");
      });
      run(label(m)+" pose",[&]{
        check_animation_and_jaw(m);
      });
    }
  }
  return failures?1:0;
}
