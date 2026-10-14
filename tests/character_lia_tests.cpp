#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/lia_anatomy.hpp"
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
TEST_NOINLINE void operator delete(void *p) noexcept { std::free(p); }
TEST_NOINLINE void operator delete[](void *p) noexcept { std::free(p); }
TEST_NOINLINE void operator delete(void *p, std::size_t) noexcept {
  std::free(p);
}
TEST_NOINLINE void operator delete[](void *p, std::size_t) noexcept {
  std::free(p);
}

namespace {
using namespace fury;
constexpr std::array<float, 3> heights{{1.35f, 1.68f, 2.10f}};
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
bool exact(float a, float b) { return std::memcmp(&a, &b, sizeof(a)) == 0; }
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
  Vec3 low{1000, 1000, 1000}, high{-1000, -1000, -1000};
  void add(Vec3 p) {
    low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
    high = {std::max(high.x, p.x), std::max(high.y, p.y),
            std::max(high.z, p.z)};
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
  require(profile.individualized && m.role == CharacterRole::BankStaff &&
              m.seed == 0x327b8688u && m.seed == character_seed("NpcTeller"),
          context + "actual Lia identity/role");
  require(!mesh.vertices.empty() && !mesh.indices.empty() &&
              mesh.indices.size() % 3 == 0,
          context + "nonempty indexed triangles");
  require(m.influences.size() == mesh.vertices.size(),
          context + "one influence per vertex");
  Bounds bounds;
  std::array<std::size_t, surface_count> surfaces{};
  std::array<bool, character_bone_count> used_bones{};
  std::array<float, 2> soles{{h, h}};
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
  // Lia's wardrobe and groom have an explicit material contract; adding an
  // unrelated atlas region must not require every actor to use that region.
  for (auto surface : {CharacterSurface::Face, CharacterSurface::Hair,
                       CharacterSurface::HairCrop, CharacterSurface::Skin,
                       CharacterSurface::Shirt, CharacterSurface::Pants,
                       CharacterSurface::Leather, CharacterSurface::Eyes,
                       CharacterSurface::Rubber, CharacterSurface::Metal,
                       CharacterSurface::HairShortStrands}) {
    const auto i = static_cast<std::size_t>(surface);
    require(surfaces[i] > 0,
            context + "modeled atlas surface " + std::to_string(i));
  }
  const auto card_vertices =
      surfaces[static_cast<std::size_t>(CharacterSurface::HairShortStrands)];
  require(card_vertices > 0,
          context + "both LODs retain real strand cutout geometry");
  for (bool used : used_bones)
    require(used, context + "every rig joint deforms actual vertices");
  const Vec3 extent = (bounds.high - bounds.low) * (1.f / h);
  require(extent.x > .28f && extent.x < .50f && extent.z > .12f &&
              extent.z < .25f,
          context +
              "sensible nonzero body, fingers, service shirt and shoe extents");
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

void check_lods_and_materials(const CharacterProfile &near,
                              const CharacterProfile &far) {
  const auto &a = near.model;
  const auto &b = far.model;
  require(a.seed == 0x327b8688u &&
              exact(a.appearance,
                    character_appearance(character_seed("NpcTeller"))) &&
              exact(a.appearance.hair, {.055f, .042f, .033f}) &&
              exact(a.appearance.shoulder_scale, 1.07f) &&
              exact(a.appearance.waist_scale, 1.04f) &&
              exact(a.appearance.face_scale, 1.02f) &&
              a.appearance.hair_style == 1,
          "Lia retains the exact seeded palette/proportions and swept-hair "
          "identity");
  require(exact(a.rig, b.rig) && exact(a.appearance, b.appearance) &&
              a.seed == b.seed && a.dual_quaternion_skinning &&
              b.dual_quaternion_skinning,
          "Near/far Lia retains bit-identical rig, identity and opt-in "
          "dual-quaternion skinning");
  // Preserve the independently frozen anatomical head within the existing
  // individual-profile CPU triangle ceilings.
  require(a.bind_mesh.indices.size() / 3 <= 40000 &&
              b.bind_mesh.indices.size() / 3 <= 16000,
          "Individual asset respects reviewed CPU cap: 40,000 near / 16,000 "
          "far triangles");
  require(b.bind_mesh.indices.size() * 2 < a.bind_mesh.indices.size() &&
              b.bind_mesh.vertices.size() * 2 < a.bind_mesh.vertices.size(),
          "Far LOD eliminates more than half of actual vertices and triangles");
  require(!exact(a.bind_mesh, b.bind_mesh), "LOD changes real geometry");
  std::array<std::size_t, surface_count> av{}, bv{};
  for (const auto &v : a.bind_mesh.vertices)
    ++av.at(static_cast<std::size_t>(surface_at(v.uv)));
  for (const auto &v : b.bind_mesh.vertices)
    ++bv.at(static_cast<std::size_t>(surface_at(v.uv)));
  require(av[static_cast<std::size_t>(CharacterSurface::Face)] ==
              bv[static_cast<std::size_t>(CharacterSurface::Face)],
          "Both LODs retain the same anatomical head instead of decimating "
          "eyelids/lips");
  for (auto surface :
       {CharacterSurface::Hair, CharacterSurface::HairCrop,
        CharacterSurface::HairShortStrands, CharacterSurface::Skin,
        CharacterSurface::Shirt, CharacterSurface::Leather}) {
    const auto i = static_cast<std::size_t>(surface);
    require(av[i] > bv[i] * 3 / 2,
            "Near LOD retains additional anatomy, hair and clothing geometry");
  }
  const auto texture = character_surface_textures();
  for (const auto *p : {&near, &far}) {
    const auto &material = p->material;
    require(material.textures == texture &&
                material.textures->base_color.valid() &&
                material.textures->normal.valid() &&
                material.textures->metallic_roughness.valid(),
            "Actual profile binds shared base color, normal and "
            "roughness/metallic maps");
    require(
        exact(material.albedo, {1, 1, 1}) && material.roughness == 1 &&
            material.metallic == 1 && material.normal_scale == 1 &&
            material.world_uv_scale == 0 && material.opacity == 1 &&
            !material.alpha_blend && material.alpha_cutoff == .4f,
        "Profile preserves vertex tint/UV0 PBR and cuts out both LOD strand "
        "masks without blending");
    require(std::isfinite(p->lod_distance) && p->lod_distance > 0 &&
                p->lod_distance == near.lod_distance,
            "Both LOD profiles agree on a finite positive individual switch "
            "distance");
  }
}

void check_anatomical_head(const CharacterModel &model) {
  const float h = model.rig.height;
  const auto context = label(model) + " anatomical head: ";
  require(
      length(model.rig.joints[bone_index(CharacterBone::Head)].bind_position -
             lia_anatomy::head_center * h) < h * 1.e-6f &&
          length(
              model.rig.joints[bone_index(CharacterBone::Neck)].bind_position -
              lia_anatomy::neck_center * h) < h * 1.e-6f,
      context + "head and neck rig pivots use the morphed source landmarks");
  std::map<std::array<float, 3>, std::uint32_t> source_positions;
  for (std::uint32_t i = 0; i < lia_anatomy::positions.size(); ++i) {
    const auto p = lia_anatomy::positions[i] * h;
    source_positions.emplace(std::array<float, 3>{{p.x, p.y, p.z}}, i);
  }
  require(source_positions.size() == lia_anatomy::positions.size(),
          context + "source geometry has unambiguous canonical positions");
  std::vector<std::uint32_t> canonical(model.bind_mesh.vertices.size(),
                                       UINT32_MAX);
  std::vector<std::uint32_t> first_copy(lia_anatomy::positions.size(),
                                        UINT32_MAX);
  std::vector<bool> retained(lia_anatomy::positions.size(), false);
  std::array<Bounds, 2> eye_bounds;
  std::array<std::size_t, 2> eye_vertices{};
  for (std::size_t i = 0; i < model.bind_mesh.vertices.size(); ++i) {
    const auto &v = model.bind_mesh.vertices[i];
    const auto surface = surface_at(v.uv);
    if (surface == static_cast<int>(CharacterSurface::Face)) {
      const auto found =
          source_positions.find({{v.position.x, v.position.y, v.position.z}});
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
      if (v.position.y > .399f * h) {
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
        std::max({u0, u1, u2}) - std::min({u0, u1, u2}) <=
            (face_uv_max.x - face_uv_min.x) * .5f,
        context,
        "source UV chart triangle stays within half the Face region: ", i / 3);
    const auto source_corner = actual_indices.size();
    require(source_corner + 2 < lia_anatomy::uv_indices.size(),
            context + "no added anatomy triangle");
    const std::uint32_t corners[]{a, b, c};
    for (std::size_t corner = 0; corner < 3; ++corner) {
      const auto uv =
          lia_anatomy::uv_coordinates[lia_anatomy::uv_indices[source_corner +
                                                              corner]];
      const auto expected =
          character_surface_uv(CharacterSurface::Face, uv.x, uv.y);
      const auto actual = model.bind_mesh.vertices[corners[corner]].uv;
      require(exact(actual.x, expected.x) && exact(actual.y, expected.y),
              context + "original per-corner UV seams survive atlas mapping");
    }
    actual_indices.insert(actual_indices.end(),
                          {canonical[a], canonical[b], canonical[c]});
  }
  require(
      actual_indices.size() == lia_anatomy::indices.size() &&
          std::equal(actual_indices.begin(), actual_indices.end(),
                     lia_anatomy::indices.begin()),
      context +
          "all source triangles survive intact and in their original winding");
  for (std::size_t side = 0; side < 2; ++side) {
    require(eye_vertices[side] > 0,
            context + "both source eye sockets contain actual eye geometry");
    const auto center = lia_anatomy::eye_centers[side] * h;
    const auto helper = lia_anatomy::eye_helper_half_extents[side] * h;
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
void check_closed_surface_seams(const CharacterModel &model,
                                CharacterSurface surface) {
  const auto &mesh = model.bind_mesh;
  std::map<std::array<std::int64_t, 3>, std::uint32_t> welded;
  std::vector<std::uint32_t> canonical(mesh.vertices.size(), UINT32_MAX);
  for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
    if (surface_at(mesh.vertices[i].uv) != static_cast<int>(surface))
      continue;
    if (surface == CharacterSurface::Skin &&
        mesh.vertices[i].position.y > .29f * model.rig.height)
      continue; // Neck continuation is checked together with its Face boundary.
    const auto p = mesh.vertices[i].position * (1.f / model.rig.height);
    const std::array<std::int64_t, 3> key{{std::llround(p.x * 1.e6),
                                           std::llround(p.y * 1.e6),
                                           std::llround(p.z * 1.e6)}};
    const auto index = static_cast<std::uint32_t>(welded.size());
    canonical[i] = welded.emplace(key, index).first->second;
  }
  struct Edge {
    unsigned count{0};
    int orientation{0};
  };
  std::map<std::pair<std::uint32_t, std::uint32_t>, Edge> edges;
  for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
    if (canonical[mesh.indices[i]] == UINT32_MAX)
      continue;
    for (int corner = 0; corner < 3; ++corner) {
      auto a = canonical[mesh.indices[i + corner]],
           b = canonical[mesh.indices[i + (corner + 1) % 3]];
      require(
          a != UINT32_MAX && b != UINT32_MAX && a != b,
          label(model) +
              " closed-shell triangle preserves distinct geometric corners");
      const int direction = a < b ? 1 : -1;
      if (b < a)
        std::swap(a, b);
      auto &edge = edges[{a, b}];
      ++edge.count;
      edge.orientation += direction;
    }
  }
  require(!edges.empty(), "Selected closed material has real shell edges");
  for (const auto &edge : edges)
    require(edge.second.count == 2 && edge.second.orientation == 0,
            label(model) + " material " +
                std::to_string(static_cast<int>(surface)) +
                " closed shell has exactly two oppositely directed faces per "
                "geometric edge");
}

// Physical periodic seams preserve tint and deformation continuity even when
// the atlas has a discontinuous U coordinate. Opaque hair sectors additionally
// require smooth normals. Closed cloth caps can deliberately carry hard
// normals.
void check_rear_collar_seam_support(const CharacterModel &model) {
  // Probe the two physical sheets at the authored outer hem chart. A ray
  // rejected exactly on the sewn center seam used to leave a 3.7 mm notch in
  // Near while both neighboring rays found the yoke. This measures the mesh,
  // rather than calling the garment's ray-query implementation again.
  const int columns = model.lod == CharacterLod::Near ? 64 : 32;
  const int first = model.lod == CharacterLod::Near ? 2 : 1;
  const float height = model.rig.height;
  std::array<float, 3> lower_y{};
  for (int offset = -1; offset <= 1; ++offset) {
    const int column = columns / 2 + offset;
    const float u = .34f * (column - first) / (columns - 2 * first);
    const Vec2 expected = character_surface_uv(
        CharacterSurface::Shirt, .025f + u / .75f, .025f + .058f / .70f);
    float low = std::numeric_limits<float>::infinity();
    float high = -std::numeric_limits<float>::infinity();
    for (const auto &v : model.bind_mesh.vertices)
      if (std::fabs(v.uv.x - expected.x) < 1e-7f &&
          std::fabs(v.uv.y - expected.y) < 1e-7f && v.position.z < 0) {
        low = std::min(low, v.position.y / height);
        high = std::max(high, v.position.y / height);
      }
    require(std::isfinite(low) && high - low > .0002f,
            label(model) + " rear collar probe finds both separated sheets");
    lower_y[offset + 1] = low;
  }
  const float dip = lower_y[1] - .5f * (lower_y[0] + lower_y[2]);
  require(dip > -.0005f,
          label(model) + " rear collar center remains supported across the "
                         "sewn yoke seam; isolated inward dip/h " +
              std::to_string(dip));
}

void check_periodic_seam_copies(const CharacterModel &model) {
  using Key = std::array<float, 3>;
  std::map<std::tuple<int, std::size_t, Key>, std::vector<std::size_t>> groups;
  const auto &mesh = model.bind_mesh;
  // Touching trims can share a point and intentionally use a darker tint.
  // Compare only seam copies within the same authored index-connected surface.
  std::vector<std::size_t> parent(mesh.vertices.size());
  for (std::size_t i = 0; i < parent.size(); ++i)
    parent[i] = i;
  auto root = [&](std::size_t i) {
    while (i != parent[i]) {
      parent[i] = parent[parent[i]];
      i = parent[i];
    }
    return i;
  };
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
    parent[root(mesh.indices[t + 1])] = root(mesh.indices[t]);
    parent[root(mesh.indices[t + 2])] = root(mesh.indices[t]);
  }
  for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
    const auto &v = mesh.vertices[i];
    const int s = surface_at(v.uv);
    if (s == int(CharacterSurface::Face) ||
        s == int(CharacterSurface::HairShortStrands))
      continue;
    const auto low = character_surface_uv(CharacterSurface(s), 0, 0);
    const auto high = character_surface_uv(CharacterSurface(s), 1, 1);
    if (std::fabs(v.uv.x - low.x) > 1e-7f && std::fabs(v.uv.x - high.x) > 1e-7f)
      continue;
    groups[{s, root(i), {{v.position.x, v.position.y, v.position.z}}}]
        .push_back(i);
  }
  std::size_t matched = 0, hair = 0;
  for (const auto &entry : groups) {
    const auto &ids = entry.second;
    for (auto a : ids) {
      bool opposite = false, compatible = false;
      for (auto b : ids) {
        const auto &va = mesh.vertices[a];
        const auto &vb = mesh.vertices[b];
        if (std::fabs(va.uv.x - vb.uv.x) < 1e-3f ||
            std::fabs(va.uv.y - vb.uv.y) > 1e-7f)
          continue;
        opposite = true;
        const bool smooth =
            (std::get<0>(entry.first) != int(CharacterSurface::Hair) &&
             std::get<0>(entry.first) != int(CharacterSurface::HairCrop)) ||
            length(va.normal - vb.normal) < .001f;
        if (smooth && length(va.color - vb.color) < .0001f &&
            exact(model.influences[a], model.influences[b]))
          compatible = true;
      }
      // Open plackets and collars can have unpaired U boundaries by design.
      if (!opposite)
        continue;
      require(compatible, label(model) + " periodic physical seam at vertex " +
                              std::to_string(a) + " surface " +
                              std::to_string(std::get<0>(entry.first)) +
                              " has matching normal, tint and weights");
      ++matched;
      if (std::get<0>(entry.first) == int(CharacterSurface::HairCrop))
        ++hair;
    }
  }
  require(matched > 30 && hair > 10,
          "Actual clothing and the HairCrop cap have tested periodic seams");
}

// A face-normal disagreement alone cannot distinguish a folded mesh from the
// approximation used to skin bind normals. Check actual triangle crossings.
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
struct KneeCrossingCheck {
  const CharacterModel &model;
  std::vector<std::size_t> triangles;
  std::vector<std::pair<std::size_t, std::size_t>> pairs;
  explicit KneeCrossingCheck(const CharacterModel &source) : model(source) {
    const auto &mesh = model.bind_mesh;
    const float h = model.rig.height;
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
      const auto a = mesh.indices[i], b = mesh.indices[i + 1],
                 c = mesh.indices[i + 2];
      if (surface_at(mesh.vertices[a].uv) !=
          static_cast<int>(CharacterSurface::Pants))
        continue;
      bool leg_geometry = false;
      for (auto vertex : {a, b, c}) {
        const auto &influence = model.influences[vertex];
        for (auto bone : {influence.first, influence.second, influence.third})
          leg_geometry |= bone == bone_index(CharacterBone::LeftThigh) ||
                          bone == bone_index(CharacterBone::LeftShin) ||
                          bone == bone_index(CharacterBone::RightThigh) ||
                          bone == bone_index(CharacterBone::RightShin);
      }
      if (!leg_geometry)
        continue; // Pelvis/upper-thigh shell joins are outside knee-fold
                  // coverage.
      const auto center =
          (mesh.vertices[a].position + mesh.vertices[b].position +
           mesh.vertices[c].position) *
          (1.f / (3.f * h));
      if (center.y > -.40f && center.y < -.065f)
        triangles.push_back(i);
    }
    for (std::size_t a = 0; a < triangles.size(); ++a)
      for (std::size_t b = a + 1; b < triangles.size(); ++b) {
        const auto ta = triangles[a], tb = triangles[b];
        if (mesh.vertices[mesh.indices[ta]].position.x *
                mesh.vertices[mesh.indices[tb]].position.x <
            0)
          continue;
        bool adjacent = false;
        for (int c = 0; c < 3; ++c)
          for (int d = 0; d < 3; ++d)
            if (length(mesh.vertices[mesh.indices[ta + c]].position -
                       mesh.vertices[mesh.indices[tb + d]].position) <
                h * 1.e-6f)
              adjacent = true;
        if (!adjacent)
          pairs.emplace_back(a, b);
      }
    require(!triangles.empty() && !pairs.empty(),
            "Both actual knee neighborhoods have candidate triangle pairs");
  }
  void check(const Mesh &mesh, const std::string &context) const {
    std::vector<Bounds> bounds(triangles.size());
    for (std::size_t i = 0; i < triangles.size(); ++i)
      for (int corner = 0; corner < 3; ++corner)
        bounds[i].add(
            mesh.vertices[mesh.indices[triangles[i] + corner]].position);
    for (const auto &pair : pairs) {
      const auto &a = bounds[pair.first];
      const auto &b = bounds[pair.second];
      if (a.high.x < b.low.x || b.high.x < a.low.x || a.high.y < b.low.y ||
          b.high.y < a.low.y || a.high.z < b.low.z || b.high.z < a.low.z)
        continue;
      Vec3 first[3], second[3];
      for (int i = 0; i < 3; ++i) {
        first[i] =
            mesh.vertices[mesh.indices[triangles[pair.first] + i]].position;
        second[i] =
            mesh.vertices[mesh.indices[triangles[pair.second] + i]].position;
      }
      for (int edge = 0; edge < 3; ++edge)
        if (proper_segment_triangle(first[edge], first[(edge + 1) % 3],
                                    second[0], second[1], second[2],
                                    model.rig.height) ||
            proper_segment_triangle(second[edge], second[(edge + 1) % 3],
                                    first[0], first[1], first[2],
                                    model.rig.height))
          throw std::runtime_error(
              context + "proper knee self-intersection between triangles " +
              std::to_string(triangles[pair.first] / 3) + " and " +
              std::to_string(triangles[pair.second] / 3));
    }
  }
};

// The eye openings are the actual seams between the external-head UV chart
// and each socket chart. Rig helpers sit inside the skull and do not establish
// where the visible lid edge is. Derive these seams independently from
// topology.
std::array<std::vector<Vec3>, 2> source_eye_openings() {
  std::map<std::pair<std::uint32_t, std::uint32_t>, unsigned> charts;
  for (std::size_t i = 0; i < lia_anatomy::indices.size(); i += 3) {
    const auto uv = lia_anatomy::uv_coordinates[lia_anatomy::uv_indices[i]];
    const unsigned chart = uv.x < .08f ? 2 : uv.x < .16f ? 1 : 0;
    for (int c = 0; c < 3; ++c) {
      auto a = lia_anatomy::indices[i + c],
           b = lia_anatomy::indices[i + (c + 1) % 3];
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
      result[side].push_back(lia_anatomy::positions[i]);
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

// Euclidean closest point uses the source triangle's actual Voronoi regions;
// a long/narrow source skull is not assumed spherical around the rig pivot.
Vec3 closest_triangle_point(Vec3 p, Vec3 a, Vec3 b, Vec3 c) {
  const auto ab = b - a, ac = c - a, ap = p - a;
  const float d1 = dot(ab, ap), d2 = dot(ac, ap);
  if (d1 <= 0 && d2 <= 0)
    return a;
  const auto bp = p - b;
  const float d3 = dot(ab, bp), d4 = dot(ac, bp);
  if (d3 >= 0 && d4 <= d3)
    return b;
  const float vc = d1 * d4 - d3 * d2;
  if (vc <= 0 && d1 >= 0 && d3 <= 0)
    return a + ab * (d1 / (d1 - d3));
  const auto cp = p - c;
  const float d5 = dot(ab, cp), d6 = dot(ac, cp);
  if (d6 >= 0 && d5 <= d6)
    return c;
  const float vb = d5 * d2 - d1 * d6;
  if (vb <= 0 && d2 >= 0 && d6 <= 0)
    return a + ac * (d2 / (d2 - d6));
  const float va = d3 * d6 - d5 * d4;
  if (va <= 0 && d4 - d3 >= 0 && d5 - d6 >= 0)
    return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
  const float inverse = 1 / (va + vb + vc);
  return a + ab * (vb * inverse) + ac * (vc * inverse);
}
void check_hair_cards(const CharacterModel &model) {
  const auto &mesh = model.bind_mesh;
  const float h = model.rig.height;
  std::vector<std::size_t> skull;
  for (std::size_t t = 0; t < lia_anatomy::indices.size(); t += 3) {
    const auto uv = lia_anatomy::uv_coordinates[lia_anatomy::uv_indices[t]];
    if (uv.x > .6f && uv.y > .14f)
      skull.push_back(t);
  }
  std::size_t count = 0;
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
    const auto &a = mesh.vertices[mesh.indices[t]],
               &b = mesh.vertices[mesh.indices[t + 1]],
               &c = mesh.vertices[mesh.indices[t + 2]];
    if (surface_at(a.uv) != int(CharacterSurface::HairShortStrands))
      continue;
    const auto center =
        (a.position + b.position + c.position) * (1.f / (3 * h));
    float best = std::numeric_limits<float>::infinity();
    Vec3 nearest{}, normal{};
    std::size_t source = 0;
    for (auto offset : skull) {
      const auto p = lia_anatomy::positions[lia_anatomy::indices[offset]];
      const auto q = lia_anatomy::positions[lia_anatomy::indices[offset + 1]];
      const auto r = lia_anatomy::positions[lia_anatomy::indices[offset + 2]];
      const auto close = closest_triangle_point(center, p, q, r);
      const auto delta = center - close;
      const float distance = dot(delta, delta);
      if (distance < best) {
        best = distance;
        nearest = close;
        normal = normalize(cross(q - p, r - p));
        source = offset / 3;
      }
    }
    const float agreement =
        dot(normalize(cross(b.position - a.position, c.position - a.position)),
            normal);
    if (agreement <= 0)
      throw std::runtime_error(
          label(model) + " hair triangle " + std::to_string(t / 3) +
          " faces against actual source triangle " + std::to_string(source) +
          ", normal dot " + std::to_string(agreement) + ", signed gap/h " +
          std::to_string(dot(center - nearest, normal)) + ", center/h " +
          std::to_string(center.x) + "," + std::to_string(center.y) + "," +
          std::to_string(center.z) + ", vertices/h " +
          std::to_string(a.position.x / h) + "," +
          std::to_string(a.position.y / h) + "," +
          std::to_string(a.position.z / h) + ";" +
          std::to_string(b.position.x / h) + "," +
          std::to_string(b.position.y / h) + "," +
          std::to_string(b.position.z / h) + ";" +
          std::to_string(c.position.x / h) + "," +
          std::to_string(c.position.y / h) + "," +
          std::to_string(c.position.z / h));
    require_element(
        dot(a.normal + b.normal + c.normal, normal) > 0, label(model),
        " hair shading faces against actual source triangle at ", t / 3);
    ++count;
  }
  require(count > 0,
          "Both LODs have actual outward-facing cutout strand geometry");
}

void check_opaque_scalp_coverage(const CharacterModel &model) {
  const auto &mesh = model.bind_mesh;
  const float h = model.rig.height;
  std::vector<std::size_t> cap;
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3)
    if (surface_at(mesh.vertices[mesh.indices[t]].uv) ==
        int(CharacterSurface::HairCrop))
      cap.push_back(t);
  require(!cap.empty(),
          "An opaque scalp surface must support the cutout groom");
  const Vec3 origin{0, .454f, .027f};
  auto scalp = [](Vec3 p) {
    return p.y > .466f || (p.y > .440f && p.z < .010f) ||
           (p.y > .456f && std::fabs(p.x) > .039f);
  };
  float minimum = 1000;
  std::size_t probes = 0;
  auto probe = [&](Vec3 skin, std::size_t source) {
    const Vec3 delta = skin - origin, direction = normalize(delta);
    float hit = -1;
    for (auto t : cap) {
      const Vec3 a = mesh.vertices[mesh.indices[t]].position * (1 / h);
      const Vec3 e1 = mesh.vertices[mesh.indices[t + 1]].position * (1 / h) - a;
      const Vec3 e2 = mesh.vertices[mesh.indices[t + 2]].position * (1 / h) - a;
      const Vec3 p = cross(direction, e2);
      const float det = dot(e1, p);
      if (std::fabs(det) < 1e-12f)
        continue;
      const Vec3 q = origin - a, c = cross(q, e1);
      const float u = dot(q, p) / det, v = dot(direction, c) / det;
      if (u >= -1e-6f && v >= -1e-6f && u + v <= 1.000001f)
        hit = std::max(hit, dot(e2, c) / det);
    }
    const float gap = hit - length(delta);
    minimum = std::min(minimum, gap);
    ++probes;
    require(gap > .00001f,
            label(model) + " opaque cap uncovers source scalp probe " +
                std::to_string(source) + " at " + std::to_string(skin.x) + "," +
                std::to_string(skin.y) + "," + std::to_string(skin.z) +
                "; radial clearance/h " + std::to_string(gap));
  };
  for (std::size_t i = 0; i < lia_anatomy::positions.size(); ++i)
    if (scalp(lia_anatomy::positions[i]))
      probe(lia_anatomy::positions[i], lia_anatomy::source_vertex_indices[i]);
  for (std::size_t t = 0; t < lia_anatomy::indices.size(); t += 3) {
    const auto a = lia_anatomy::positions[lia_anatomy::indices[t]],
               b = lia_anatomy::positions[lia_anatomy::indices[t + 1]],
               c = lia_anatomy::positions[lia_anatomy::indices[t + 2]];
    if (scalp(a) && scalp(b) && scalp(c))
      probe((a + b + c) * (1.f / 3),
            lia_anatomy::triangle_source_indices[t / 3]);
  }
  require(probes > 300,
          "Substantial original scalp geometry is checked in each LOD");
  std::cout << label(model) << ": " << probes
            << " actual scalp vertices/centroids behind opaque cap; minimum "
               "radial clearance/h "
            << minimum << "\n";
}

struct TuckedShirtCheck {
  const CharacterModel &model;
  std::vector<std::size_t> points, triangles;
  explicit TuckedShirtCheck(const CharacterModel &source) : model(source) {
    const auto &mesh = model.bind_mesh;
    const float h = model.rig.height;
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
      const auto &p = mesh.vertices[i].position;
      if (surface_at(mesh.vertices[i].uv) == int(CharacterSurface::Shirt) &&
          p.y >= .011f * h && p.y < .035f * h && std::fabs(p.x) < .12f * h)
        points.push_back(i);
    }
    for (std::size_t t = 0; t < mesh.indices.size(); t += 3)
      if (surface_at(mesh.vertices[mesh.indices[t]].uv) ==
          int(CharacterSurface::Pants))
        triangles.push_back(t);
    require(points.size() > 30 && !triangles.empty(),
            "Actual hidden shirt hem and front placket are sampled");
  }
  void check(const Mesh &mesh, const CharacterPose &pose,
             const std::string &context) const {
    const auto &pelvis = pose.skin_matrices[bone_index(CharacterBone::Pelvis)];
    const Vec3 up = normalize(transform_direction(pelvis, {0, 1, 0}));
    struct Surface {
      Vec3 a, e1, e2;
      float lo, hi;
    };
    std::vector<Surface> surfaces;
    for (auto t : triangles) {
      const Vec3 a = mesh.vertices[mesh.indices[t]].position,
                 b = mesh.vertices[mesh.indices[t + 1]].position,
                 c = mesh.vertices[mesh.indices[t + 2]].position;
      surfaces.push_back({a, b - a, c - a,
                          std::min({dot(a, up), dot(b, up), dot(c, up)}),
                          std::max({dot(a, up), dot(b, up), dot(c, up)})});
    }
    for (auto id : points) {
      const Vec3 bind = model.bind_mesh.vertices[id].position;
      const Vec3 direction = normalize(
          transform_direction(pelvis, normalize(Vec3{bind.x, 0, bind.z})));
      const Vec3 point = mesh.vertices[id].position;
      const float level = dot(point, up);
      float hit = std::numeric_limits<float>::infinity();
      for (const auto &s : surfaces) {
        if (level < s.lo || level > s.hi)
          continue;
        const Vec3 p = cross(direction, s.e2);
        const float det = dot(s.e1, p);
        if (std::fabs(det) < 1e-12f)
          continue;
        const Vec3 q = point - s.a, c = cross(q, s.e1);
        const float u = dot(q, p) / det, v = dot(direction, c) / det,
                    d = dot(s.e2, c) / det;
        if (u >= -1e-5f && v >= -1e-5f && u + v <= 1.00001f &&
            d > model.rig.height * 1e-6f)
          hit = std::min(hit, d);
      }
      require(std::isfinite(hit),
              context + " hidden shirt/placket vertex " + std::to_string(id) +
                  " protrudes beyond the actual trouser enclosure; bind/h " +
                  std::to_string(bind.x / model.rig.height) + "," +
                  std::to_string(bind.y / model.rig.height) + "," +
                  std::to_string(bind.z / model.rig.height));
    }
  }
};

struct TrouserSeamCheck {
  const CharacterModel &model;
  std::vector<std::pair<std::size_t, std::size_t>> copies;
  explicit TrouserSeamCheck(const CharacterModel &input) : model(input) {
    const auto &mesh = model.bind_mesh;
    const float h = model.rig.height;
    using Key = std::array<float, 3>;
    std::map<Key, std::size_t> nodes;
    std::vector<std::size_t> canonical(mesh.vertices.size(), SIZE_MAX);
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
      const auto &v = mesh.vertices[i];
      if (surface_at(v.uv) != int(CharacterSurface::Pants))
        continue;
      auto first =
          nodes.emplace(Key{{v.position.x, v.position.y, v.position.z}}, i)
              .first->second;
      canonical[i] = first;
      if (first != i) {
        require(exact(model.influences[i], model.influences[first]),
                "Trouser sewn UV corners retain exact source weights");
        copies.emplace_back(i, first);
      }
    }
    struct Edge {
      int count{}, direction{};
    };
    std::map<std::pair<std::size_t, std::size_t>, Edge> edges;
    std::map<std::size_t, std::set<std::size_t>> graph;
    for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
      if (canonical[mesh.indices[t]] == SIZE_MAX)
        continue;
      for (int j = 0; j < 3; ++j) {
        auto a = canonical[mesh.indices[t + j]],
             b = canonical[mesh.indices[t + (j + 1) % 3]];
        require(a != b && b != SIZE_MAX,
                "Trouser triangle has three physical corners");
        graph[a].insert(b);
        graph[b].insert(a);
        int sign = a < b ? 1 : -1;
        if (b < a)
          std::swap(a, b);
        auto &e = edges[{a, b}];
        ++e.count;
        e.direction += sign;
      }
    }
    require(!graph.empty(), "Joined trousers have real geometry");
    auto connected = [&](const auto &adjacent) {
      std::set<std::size_t> unseen;
      for (const auto &entry : adjacent)
        unseen.insert(entry.first);
      int components = 0;
      while (!unseen.empty()) {
        ++components;
        std::vector<std::size_t> pending{*unseen.begin()};
        while (!pending.empty()) {
          auto n = pending.back();
          pending.pop_back();
          if (!unseen.erase(n))
            continue;
          for (auto k : adjacent.at(n))
            pending.push_back(k);
        }
      }
      return components;
    };
    require(connected(graph) == 1, "Both trouser legs, curved crotch and "
                                   "waistband are one connected sewn surface");
    std::map<std::size_t, std::set<std::size_t>> boundaries;
    for (const auto &entry : edges) {
      const auto &e = entry.second;
      require(e.count <= 2, "No nonmanifold trouser edge or buried thigh cap");
      if (e.count == 2)
        require(e.direction == 0, "Shared trouser edges have opposite winding");
      else {
        auto a = entry.first.first, b = entry.first.second;
        boundaries[a].insert(b);
        boundaries[b].insert(a);
        const auto p = mesh.vertices[a].position * (1 / h),
                   q = mesh.vertices[b].position * (1 / h);
        require((p.y > .050f && q.y > .050f) || (p.y < -.44f && q.y < -.44f),
                "Only inner waist/cuff returns may remain open; the crotch and "
                "upper legs are sewn");
      }
    }
    for (const auto &entry : boundaries)
      require(entry.second.size() == 2,
              "Each trouser opening is a simple unbranched loop");
    require(connected(boundaries) == 3,
            "Exactly one waistband and two ankle-return loops remain");
    require(copies.size() > 20,
            "Actual independent trouser seam copies are audited");
    std::cout
        << label(model)
        << ": one connected trouser surface, three intended return loops, "
        << copies.size() << " exact UV seam copies\n";
  }
  void check(const Mesh &mesh, const std::string &context) const {
    for (auto pair : copies)
      require(exact(mesh.vertices[pair.first].position,
                    mesh.vertices[pair.second].position),
              context +
                  " trouser rise/leg/waist UV seam does not split in motion");
  }
};

struct SewnShirtCheck {
  const CharacterModel &model;
  std::vector<std::size_t> coat, neck, shoulder, attachment, skin_top,
      collar_boundary;
  std::vector<std::pair<std::size_t, std::size_t>> copies;
  explicit SewnShirtCheck(const CharacterModel &source) : model(source) {
    const auto &mesh = model.bind_mesh;
    const float h = model.rig.height;
    std::vector<std::size_t> parent(mesh.vertices.size());
    for (std::size_t i = 0; i < parent.size(); ++i)
      parent[i] = i;
    auto root = [&](std::size_t i) {
      while (parent[i] != i) {
        parent[i] = parent[parent[i]];
        i = parent[i];
      }
      return i;
    };
    using Key = std::array<float, 3>;
    std::map<Key, std::size_t> welded;
    std::vector<std::size_t> canonical(mesh.vertices.size(), SIZE_MAX);
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
      const auto &v = mesh.vertices[i];
      if (surface_at(v.uv) != int(CharacterSurface::Shirt))
        continue;
      const auto id =
          welded.emplace(Key{{v.position.x, v.position.y, v.position.z}}, i)
              .first->second;
      canonical[i] = id;
      parent[root(i)] = root(id);
    }
    for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
      const auto a = mesh.indices[t], b = mesh.indices[t + 1],
                 c = mesh.indices[t + 2];
      if (canonical[a] == SIZE_MAX)
        continue;
      parent[root(b)] = root(a);
      parent[root(c)] = root(a);
    }
    std::map<std::size_t, std::size_t> sizes;
    for (std::size_t i = 0; i < canonical.size(); ++i)
      if (canonical[i] != SIZE_MAX)
        ++sizes[root(i)];
    require(!sizes.empty(), "Actual sewn shirt has indexed geometry");
    const auto largest = std::max_element(sizes.begin(), sizes.end(),
                                          [](const auto &a, const auto &b) {
                                            return a.second < b.second;
                                          })
                             ->first;
    struct Edge {
      unsigned count{0};
      int direction{0};
    };
    std::map<std::pair<std::size_t, std::size_t>, Edge> edges;
    Bounds extent;
    std::size_t arm_seam_copies[2]{};
    for (std::size_t i = 0; i < canonical.size(); ++i) {
      if (canonical[i] == SIZE_MAX || root(i) != largest)
        continue;
      const auto &v = mesh.vertices[i];
      extent.add(v.position);
      if (canonical[i] != i) {
        const auto j = canonical[i];
        require(exact(model.influences[i], model.influences[j]),
                label(model) + " sewn garment's coincident UV corners have "
                               "exactly equal skin weights");
        if (v.position.y > .17f * h && v.position.y < .335f * h &&
            std::fabs(v.position.x) > .07f * h)
          require(length(v.normal - mesh.vertices[j].normal) < 1.e-5f,
                  label(model) +
                      " sewn armhole/shoulder UV corners have smooth normals");
        copies.emplace_back(i, j);
        if (v.position.y > .19f * h && v.position.y < .34f * h &&
            std::fabs(v.position.x) > .075f * h)
          ++arm_seam_copies[v.position.x > 0];
      }
    }
    require((extent.high.x - extent.low.x) / h > .30f &&
                (extent.high.y - extent.low.y) / h > .35f,
            "Both sleeves and complete torso belong to one welded garment");
    require(arm_seam_copies[0] >= 12 && arm_seam_copies[1] >= 12,
            "Both actual armhole/shoulder chart seams have independent "
            "duplicate-corner probes");
    for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
      const auto a = mesh.indices[t], b = mesh.indices[t + 1],
                 c = mesh.indices[t + 2];
      const auto center =
          (mesh.vertices[a].position + mesh.vertices[b].position +
           mesh.vertices[c].position) *
          (1.f / (3 * h));
      const int surface = surface_at(mesh.vertices[a].uv);
      if (surface == int(CharacterSurface::Face) && center.y < .381f)
        neck.push_back(t);
      if (canonical[a] == SIZE_MAX || root(a) != largest)
        continue;
      coat.push_back(t);
      if (center.y > .17f)
        shoulder.push_back(t);
      for (int corner = 0; corner < 3; ++corner) {
        auto x = canonical[mesh.indices[t + corner]],
             y = canonical[mesh.indices[t + (corner + 1) % 3]];
        require(x != y,
                "Welded garment triangle is not geometrically degenerate");
        int sign = x < y ? 1 : -1;
        if (y < x)
          std::swap(x, y);
        auto &e = edges[{x, y}];
        ++e.count;
        e.direction += sign;
      }
    }
    std::size_t interior = 0, boundary = 0;
    for (const auto &entry : edges) {
      const auto &e = entry.second;
      require(e.count <= 2,
              label(model) +
                  " sewn shirt has no nonmanifold edges/internal armhole caps");
      if (e.count == 2) {
        require(e.direction == 0,
                label(model) +
                    " sewn shirt edges have opposite triangle winding");
        ++interior;
      } else {
        ++boundary;
        const auto a = mesh.vertices[entry.first.first].position * (1.f / h);
        const auto b = mesh.vertices[entry.first.second].position * (1.f / h);
        // These are intentional openings/turned returns, not shoulder joins.
        const bool hem = std::max(a.y, b.y) < .024f &&
                         std::max(std::fabs(a.x), std::fabs(b.x)) < .12f;
        const bool cuff = std::max(a.y, b.y) < .020f &&
                          std::min(std::fabs(a.x), std::fabs(b.x)) > .12f;
        const bool neckline = std::min(a.y, b.y) > .327f &&
                              std::max(std::fabs(a.x), std::fabs(b.x)) < .080f;
        if (neckline) {
          collar_boundary.push_back(entry.first.first);
          collar_boundary.push_back(entry.first.second);
        }
        require(hem || cuff || neckline,
                label(model) + " unintended open sewn edge at " +
                    std::to_string(a.x) + "," + std::to_string(a.y) + "," +
                    std::to_string(a.z) + " to " + std::to_string(b.x) + "," +
                    std::to_string(b.y) + "," + std::to_string(b.z));
      }
    }
    require(interior > 100 && boundary > 0 && !neck.empty(),
            "Real sewn manifold and anatomical neck are audited");
    for (auto id : lia_anatomy::attachment_boundary_indices) {
      const auto p = lia_anatomy::positions[id] * h;
      std::size_t found = SIZE_MAX;
      for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
        if (surface_at(mesh.vertices[i].uv) == int(CharacterSurface::Face) &&
            exact(mesh.vertices[i].position, p)) {
          found = i;
          break;
        }
      require(
          found != SIZE_MAX,
          "Every one of the 50 canonical skin attachment vertices survives");
      attachment.push_back(found);
    }
    require(attachment.size() == 50,
            "Complete Lia attachment boundary is sampled");
    // Recover the source/Skin continuation through topology, not creation
    // order. The source cut itself may now be visible continuous neck skin;
    // only the new lower termination is permitted to be an open concealed
    // boundary.
    const auto source_attachment = attachment;
    skin_top = attachment;
    attachment.clear();
    std::map<Key, std::size_t> skin_nodes;
    std::vector<std::size_t> skin_parent(mesh.vertices.size());
    for (std::size_t i = 0; i < skin_parent.size(); ++i)
      skin_parent[i] = i;
    auto skin_root = [&](std::size_t i) {
      while (skin_parent[i] != i) {
        skin_parent[i] = skin_parent[skin_parent[i]];
        i = skin_parent[i];
      }
      return i;
    };
    auto key = [&](std::size_t i) {
      const auto p = mesh.vertices[i].position;
      return Key{{p.x, p.y, p.z}};
    };
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
      if (surface_at(mesh.vertices[i].uv) == int(CharacterSurface::Skin)) {
        const auto first = skin_nodes.emplace(key(i), i).first->second;
        skin_parent[skin_root(i)] = skin_root(first);
      }
    for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
      const auto a = mesh.indices[t], b = mesh.indices[t + 1],
                 c = mesh.indices[t + 2];
      if (surface_at(mesh.vertices[a].uv) != int(CharacterSurface::Skin))
        continue;
      skin_parent[skin_root(b)] = skin_root(a);
      skin_parent[skin_root(c)] = skin_root(a);
    }
    std::size_t component = SIZE_MAX;
    for (auto source_vertex : source_attachment) {
      const auto found = skin_nodes.find(key(source_vertex));
      require(found != skin_nodes.end(),
              label(model) +
                  " source neck cut has an exact Skin continuation at vertex " +
                  std::to_string(source_vertex));
      if (component == SIZE_MAX)
        component = skin_root(found->second);
      require(skin_root(found->second) == component,
              "All50 source attachment vertices share one connected underlay");
      for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        if (surface_at(mesh.vertices[i].uv) != int(CharacterSurface::Skin) ||
            key(i) != key(source_vertex))
          continue;
        require(exact(model.influences[i], model.influences[source_vertex]),
                "Source/underlay seam preserves exact weights");
        require(length(mesh.vertices[i].normal -
                       mesh.vertices[source_vertex].normal) < 1.e-5f,
                "Source/underlay seam preserves smooth normals");
        copies.emplace_back(i, source_vertex);
      }
    }
    using SkinEdge = std::pair<Key, Key>;
    std::map<SkinEdge, Edge> continuation_edges, source_edges;
    for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
      const auto a = mesh.indices[t];
      const int material = surface_at(mesh.vertices[a].uv);
      const bool underlay =
          material == int(CharacterSurface::Skin) && skin_root(a) == component;
      if (!underlay && material != int(CharacterSurface::Face))
        continue;
      if (underlay)
        neck.push_back(t);
      auto &list = underlay ? continuation_edges : source_edges;
      for (int c = 0; c < 3; ++c) {
        auto x = key(mesh.indices[t + c]),
             y = key(mesh.indices[t + (c + 1) % 3]);
        const int sign = x < y ? 1 : -1;
        if (y < x)
          std::swap(x, y);
        auto &e = list[{x, y}];
        ++e.count;
        e.direction += sign;
      }
    }
    std::set<SkinEdge> top_edges;
    for (const auto &entry : source_edges)
      if (entry.second.count == 1) {
        const auto found = continuation_edges.find(entry.first);
        require(found != continuation_edges.end() && found->second.count == 1 &&
                    found->second.direction + entry.second.direction == 0,
                "Every exact canonical neck edge pairs with an opposite "
                "underlay edge");
        top_edges.insert(entry.first);
      }
    require(top_edges.size() == 50,
            "Complete50-edge original neck cut is physically sewn closed");
    std::map<Key, std::vector<Key>> lower_graph;
    for (const auto &entry : continuation_edges) {
      const auto &e = entry.second;
      require(e.count <= 2, "Neck continuation is manifold");
      if (e.count == 2)
        require(e.direction == 0,
                "Underlay interior edges are oppositely wound");
      else if (!top_edges.count(entry.first)) {
        lower_graph[entry.first.first].push_back(entry.first.second);
        lower_graph[entry.first.second].push_back(entry.first.first);
      }
    }
    require(lower_graph.size() == 50,
            "Only the50-vertex hidden lower termination remains open");
    std::set<Key> visited;
    std::vector<Key> pending{lower_graph.begin()->first};
    while (!pending.empty()) {
      const auto k = pending.back();
      pending.pop_back();
      if (!visited.insert(k).second)
        continue;
      const auto &adjacent = lower_graph.at(k);
      require(adjacent.size() == 2, "Lower skin termination is a simple cycle");
      pending.insert(pending.end(), adjacent.begin(), adjacent.end());
    }
    require(visited.size() == 50,
            "Skin continuation has one connected hidden lower loop");
    for (const auto &entry : lower_graph)
      attachment.push_back(skin_nodes.at(entry.first));
    std::cout << label(model)
              << ": 50 exact Face/Skin sewn edges and one50-vertex hidden "
                 "termination\n";

    std::cout << label(model) << ": sewn main garment " << coat.size()
              << " triangles, " << interior << " paired edges, " << boundary
              << " intentional opening edges, " << copies.size()
              << " exact seam copies\n";
  }
  bool crossing(const Mesh &mesh, std::size_t ta, std::size_t tb) const {
    Vec3 p[3], q[3];
    Bounds a, b;
    for (int c = 0; c < 3; ++c) {
      p[c] = mesh.vertices[mesh.indices[ta + c]].position;
      q[c] = mesh.vertices[mesh.indices[tb + c]].position;
      a.add(p[c]);
      b.add(q[c]);
    }
    if (a.high.x < b.low.x || b.high.x < a.low.x || a.high.y < b.low.y ||
        b.high.y < a.low.y || a.high.z < b.low.z || b.high.z < a.low.z)
      return false;
    // A shared geometric vertex/edge is a sewn join. Coplanar contact has a
    // zero determinant and is intentionally not a proper triangle crossing.
    for (auto x : p)
      for (auto y : q)
        if (length(x - y) < model.rig.height * 1e-7f)
          return false;
    for (int e = 0; e < 3; ++e)
      if (proper_segment_triangle(p[e], p[(e + 1) % 3], q[0], q[1], q[2],
                                  model.rig.height) ||
          proper_segment_triangle(q[e], q[(e + 1) % 3], p[0], p[1], p[2],
                                  model.rig.height))
        return true;
    return false;
  }
  enum class Part { All, Shoulders, Neck, Seams };
  void check(const Mesh &mesh, const std::string &context,
             Part part = Part::All) const {
    if (part == Part::All || part == Part::Seams)
      for (auto pair : copies)
        require_element(length(mesh.vertices[pair.first].position -
                               mesh.vertices[pair.second].position) <
                            model.rig.height * 1e-7f,
                        context, "actual animated sewn seam opens at vertex ",
                        pair.first);
    // Sweep all actual neck/shoulder triangles by X. The broad phase does not
    // exclude any armhole overlap; it merely removes disjoint AABBs.
    if (part != Part::Seams) {
      struct Item {
        std::size_t t;
        Bounds b;
        bool skin;
      };
      std::vector<Item> items;
      items.reserve(shoulder.size() + neck.size());
      for (int skin = 0; skin < 2; ++skin)
        for (auto t : skin ? neck : shoulder) {
          if (skin && part == Part::Shoulders)
            continue;
          Item item{t, {}, skin != 0};
          for (int c = 0; c < 3; ++c)
            item.b.add(mesh.vertices[mesh.indices[t + c]].position);
          items.push_back(item);
        }
      std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
        return a.b.low.x < b.b.low.x;
      });
      for (std::size_t a = 0; a < items.size(); ++a)
        for (std::size_t b = a + 1;
             b < items.size() && items[b].b.low.x <= items[a].b.high.x; ++b) {
          if (items[a].skin && items[b].skin)
            continue;
          if (part == Part::Neck && items[a].skin == items[b].skin)
            continue;
          const auto &x = items[a].b;
          const auto &y = items[b].b;
          if (x.high.y < y.low.y || y.high.y < x.low.y || x.high.z < y.low.z ||
              y.high.z < x.low.z)
            continue;
          if (crossing(mesh, items[a].t, items[b].t))
            throw std::runtime_error(context +
                                     (items[a].skin != items[b].skin
                                          ? "proper collar/neck crossing"
                                          : "proper sewn shoulder crossing") +
                                     " between triangles " +
                                     std::to_string(items[a].t / 3) + "/" +
                                     std::to_string(items[b].t / 3));
        }
    }
    if (part == Part::Shoulders || part == Part::Neck)
      return;
    // Use the posed neck direction. World-up rays through an intentionally
    // open collar give false exposure reports as the torso breathes or leans.
    Vec3 top_center{}, bottom_center{};
    for (auto id : skin_top)
      top_center += mesh.vertices[id].position;
    for (auto id : attachment)
      bottom_center += mesh.vertices[id].position;
    top_center = top_center * (1.f / skin_top.size());
    bottom_center = bottom_center * (1.f / attachment.size());
    const auto up = normalize(top_center - bottom_center);
    const auto right = normalize(Vec3{1, 0, 0} - up * up.x);
    const auto forward = normalize(cross(right, up));
    float opening_floor = std::numeric_limits<float>::infinity();
    for (auto id : collar_boundary)
      opening_floor =
          std::min(opening_floor, dot(mesh.vertices[id].position, up));
    struct RayTriangle {
      Vec3 a, e1, e2;
      float low, high;
    };
    std::vector<RayTriangle> enclosure;
    enclosure.reserve(shoulder.size());
    for (auto t : shoulder) {
      const auto a = mesh.vertices[mesh.indices[t]].position;
      const auto b = mesh.vertices[mesh.indices[t + 1]].position;
      const auto c = mesh.vertices[mesh.indices[t + 2]].position;
      const float x = dot(a, up), y = dot(b, up), z = dot(c, up);
      enclosure.push_back(
          {a, b - a, c - a, std::min({x, y, z}), std::max({x, y, z})});
    }
    for (auto id : attachment) {
      const auto p = mesh.vertices[id].position;
      const float level = dot(p, up);
      if (level > opening_floor - model.rig.height * .003f)
        throw std::runtime_error(context +
                                 "lower skin termination rises into actual "
                                 "collar opening at vertex " +
                                 std::to_string(id));
      // Four independent outward rays in the actual transverse neck plane
      // require the lower cut to lie inside the garment at a meaningful depth.
      for (auto direction : {right, -right, forward, -forward}) {
        float nearest = std::numeric_limits<float>::infinity();
        for (const auto &triangle : enclosure) {
          if (level < triangle.low || level > triangle.high)
            continue;
          const auto cross_direction = cross(direction, triangle.e2);
          const float determinant = dot(triangle.e1, cross_direction);
          if (std::fabs(determinant) <
              model.rig.height * model.rig.height * 1e-12f)
            continue;
          const auto offset = p - triangle.a,
                     cross_offset = cross(offset, triangle.e1);
          const float u = dot(offset, cross_direction) / determinant;
          const float v = dot(direction, cross_offset) / determinant;
          const float distance = dot(triangle.e2, cross_offset) / determinant;
          if (u >= -1e-5f && v >= -1e-5f && u + v <= 1.00001f &&
              distance > model.rig.height * 1e-6f)
            nearest = std::min(nearest, distance);
        }
        if (!std::isfinite(nearest))
          throw std::runtime_error(
              context +
              "lower skin termination is outside actual shirt enclosure at "
              "vertex " +
              std::to_string(id) + " at " +
              std::to_string(p.x / model.rig.height) + "," +
              std::to_string(p.y / model.rig.height) + "," +
              std::to_string(p.z / model.rig.height));
      }
    }
  }
};

CharacterAnimationSample sample_for(int mode, int frame, float h,
                                    std::uint32_t seed) {
  CharacterAnimationSample sample;
  sample.phase_cycles = static_cast<double>(frame) / 32.;
  sample.idle_time = static_cast<double>(frame) / 8.;
  sample.seed = seed;
  if (mode == 1 || mode == 2 || mode == 5) {
    sample.speed = (mode == 2 ? 2.8f : mode == 5 ? .55f : .8f) * h;
    sample.move_weight = 1;
  }
  if (mode == 3) {
    sample.talk_weight = sample.attention_weight = 1;
    sample.attention_yaw = .45f;
    sample.attention_pitch = -.10f;
  }
  if (mode == 4 || mode == 5)
    sample.crouch_weight = 1;
  return sample;
}
void check_animation(const CharacterModel &model) {
  const float h = model.rig.height;
  const auto bind = model.bind_mesh;
  const KneeCrossingCheck knees(model);
  const TrouserSeamCheck trousers(model);
  const TuckedShirtCheck tuck(model);
  const SewnShirtCheck shoulders(model);
  std::array<std::string, 5> crossing_failures;
  auto audit = [&](const Mesh &mesh, const std::string &context) {
    trousers.check(mesh, context);
    for (unsigned i = 0; i < crossing_failures.size(); ++i) {
      if (!crossing_failures[i].empty())
        continue;
      try {
        if (i == 0)
          knees.check(mesh, context);

        if (i == 2)
          shoulders.check(mesh, context, SewnShirtCheck::Part::Shoulders);
        if (i == 3)
          shoulders.check(mesh, context, SewnShirtCheck::Part::Neck);
        if (i == 4)
          shoulders.check(mesh, context, SewnShirtCheck::Part::Seams);
      } catch (const std::exception &error) {
        crossing_failures[i] = error.what();
      }
    }
  };
  audit(bind, label(model) + " bind pose: ");
  Mesh output = bind;
  output.gpu_vao = 101;
  output.gpu_vbo = 102;
  output.gpu_ibo = 103;
  output.gpu_uploaded = true;
  const auto *vertices = output.vertices.data();
  const auto *indices = output.indices.data();
  const auto vertex_capacity = output.vertices.capacity(),
             index_capacity = output.indices.capacity();
  const auto identity = output.geometry_identity;
  constexpr const char *modes[]{"idle",           "walk",   "run",
                                "talk/attention", "crouch", "crouch-walk"};
  std::size_t poses = 0;
  for (int mode = 0; mode < 6; ++mode) {
    std::vector<Vec3> previous(output.vertices.size());
    float largest_change = 0;
    for (int frame = 0; frame <= 32; ++frame) {
      const auto sample = sample_for(mode, frame, h, model.seed);
      count_allocations = true;
      allocations = 0;
      const auto pose = sample_character_animation(model.rig, sample);
      count_allocations = false;
      require(allocations == 0,
              "Actual-profile pose sampling allocates no per-frame storage");
      const auto context = label(model) + " " + modes[mode] + " frame " +
                           std::to_string(frame) + ": ";
      for (const auto &matrix : pose.skin_matrices)
        for (float value : matrix.m)
          require(std::isfinite(value), context + "finite skin matrix");
      count_allocations = true;
      allocations = 0;
      apply_character_pose(model, pose, output);
      count_allocations = false;
      require(allocations == 0,
              context +
                  "actual-profile skinning allocates no per-frame storage");
      require(
          output.vertices.data() == vertices &&
              output.indices.data() == indices &&
              output.vertices.capacity() == vertex_capacity &&
              output.indices.capacity() == index_capacity &&
              output.vertices.size() == bind.vertices.size() &&
              output.indices == bind.indices &&
              output.geometry_identity == identity && output.gpu_vao == 101 &&
              output.gpu_vbo == 102 && output.gpu_ibo == 103 &&
              output.gpu_uploaded,
          context +
              "skinning keeps topology, storage and renderer handles stable");
      const auto revision = output.geometry_revision;
      require(!apply_character_pose(model, pose, output) &&
                  output.geometry_revision == revision,
              context +
                  "identical pose does not rebuild or dirty geometry twice");
      for (std::size_t i = 0; i < output.vertices.size(); ++i) {
        const auto &v = output.vertices[i];
        const auto &source = bind.vertices[i];
        require_element(finite(v.position) && finite(v.normal) &&
                            std::fabs(length(v.normal) - 1) < .0002f,
                        context, "finite skinned vertex/unit normal ", i);
        require_element(length(v.position) < h * 1.1f, context,
                        "no exploding deformed vertex ", i);
        require_element(
            exact(v.color, source.color) && exact(v.uv.x, source.uv.x) &&
                exact(v.uv.y, source.uv.y) && exact(v.opacity, source.opacity),
            context, "skinning preserves exact color/UV/opacity at vertex ", i);
        const auto bone = model.influences[i].first;
        if (bone == bone_index(CharacterBone::LeftFoot) ||
            bone == bone_index(CharacterBone::RightFoot))
          require_element(v.position.y >= -.5001f * h, context,
                          "actual shoe geometry stays above ground at vertex ",
                          i);
        if (frame) {
          const float displacement = length(v.position - previous[i]);
          require_element(displacement < h * .14f, context,
                          "adjacent sampled frames do not snap at vertex ", i);
          largest_change = std::max(largest_change, displacement);
        }
        previous[i] = v.position;
      }
      for (std::size_t i = 0; i < output.indices.size(); i += 3) {
        const auto a = output.vertices[output.indices[i]].position;
        const auto b = output.vertices[output.indices[i + 1]].position;
        const auto c = output.vertices[output.indices[i + 2]].position;
        require_element(length(cross(b - a, c - a)) > h * h * 1.e-13f, context,
                        "skinning cannot collapse triangle ", i / 3);
      }
      tuck.check(output, pose, context);
      audit(output, context);
      ++poses;
    }
    require(largest_change > h * .000001f,
            std::string(modes[mode]) + " changes actual model vertices");
  }
  for (float yaw : {-.8f, 0.f, .8f})
    for (float pitch : {-.25f, .25f})
      for (int frame = 0; frame <= 32; frame += 8) {
        auto sample = sample_for(3, frame, h, model.seed);
        sample.attention_yaw = yaw;
        sample.attention_pitch = pitch;
        apply_character_pose(
            model, sample_character_animation(model.rig, sample), output);
        audit(output, label(model) + " head turn yaw " + std::to_string(yaw) +
                          " pitch " + std::to_string(pitch) + " frame " +
                          std::to_string(frame) + ": ");
      }
  require(exact(bind, model.bind_mesh),
          "Animation never mutates the immutable bind geometry");
  std::cout << label(model) << ": " << poses
            << " actual skinned idle/walk/run/talk/crouch/crouch-walk poses "
               "plus 30 head turns\n";
  std::string failure;
  for (const auto &error : crossing_failures)
    if (!error.empty())
      failure += error + "\n";
  require(failure.empty(), failure);
}

void check_distinct_identity(const CharacterProfile &lia) {
  const auto &m = lia.model;
  for (auto id : {"NpcCivA", "NpcCivB", "NpcCivC", "NpcBankCust"}) {
    const auto role = std::string_view(id) == "NpcCivB"
                          ? CharacterRole::Market
                          : CharacterRole::Commuter;
    const auto other = make_character_profile(m.rig.height, role, id, m.lod);
    require(
        other.individualized && other.model.seed != m.seed &&
            !exact(other.model.appearance, m.appearance) &&
            !exact(other.model.rig, m.rig),
        "Lia retains his own stable identity, appearance and anatomical rig");
    Bounds a, b;
    for (const auto &v : m.bind_mesh.vertices)
      if (surface_at(v.uv) == int(CharacterSurface::Face))
        a.add(v.position);
    for (const auto &v : other.model.bind_mesh.vertices)
      if (surface_at(v.uv) == int(CharacterSurface::Face))
        b.add(v.position);
    require(length((a.high - a.low) - (b.high - b.low)) > m.rig.height * .002f,
            "Lia has independently morphed anatomy, not merely a new hair or "
            "cloth tint");
  }
  const auto generic =
      make_character_model(m.rig.height, m.role, m.seed, m.lod);
  require(!exact(m.bind_mesh, generic.bind_mesh) && !exact(m.rig, generic.rig),
          "Registered Lia profile replaces the generic body and rig");
}

void generic_identity_fallback() {
  struct Identity {
    const char *id;
    CharacterRole role;
  };
  using R = CharacterRole;
  constexpr Identity roster[]{{"unregistered_hale_security", R::Security},
                              {"unregistered_desk_guard", R::Security},
                              {"unregistered_metro_security", R::Security},
                              {"unregistered_enforcer", R::Enforcer},
                              {"unregistered_commuter", R::Commuter},
                              {"unregistered_alley_security", R::Security},
                              {"unregistered_market", R::Market},
                              {"unregistered_dock", R::Dock},
                              {"unregistered_east_dock", R::Dock},
                              {"unregistered_fence", R::Fence},
                              {"unregistered_crew_tech", R::CrewTech},
                              {"unregistered_scout", R::CrewScout},
                              {"unregistered_player", R::Player},
                              {"unregistered_ghost", R::Ghost},
                              {"unknown_identity", R::Enforcer},
                              {"npcteller", R::Commuter},
                              {"NpcTeller_extra", R::Commuter},
                              {"", R::Commuter}};
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
    for (auto lod : {CharacterLod::Near, CharacterLod::Far}) {
      for (const auto &identity : roster)
        check(h, identity.role, identity.id, lod);
      for (unsigned r = 0; r < static_cast<unsigned>(R::Count); ++r)
        if (static_cast<R>(r) != R::BankStaff)
          check(h, static_cast<R>(r), "NpcTeller", lod);
      check(h, R::BankStaff, std::string_view("NpcTeller\0extra", 15), lod);
    }
  std::cout << cases
            << " bit-exact generic identity/role/height/LOD fallback cases\n";
}
// Requires #include "fury/material_sampling.hpp"; other includes are already
// present in character_lia_tests.cpp. Call once, outside the height/LOD loops.
void check_lia_face_charts() {
  // Keep these source/component bounds and atlas cells independent of the
  // production classifier. Lia's neck uses a subset of the canonical bounds.
  struct Chart {
    Vec2 low, high, origin;
    float scale;
    int x, y, width, height;
    unsigned triangles;
  };
  constexpr std::array<Chart, 5> charts{{
      {{.641249001f, .148696005f},
       {.993515015f, .817633986f},
       {8, 8},
       1.44f,
       0,
       0,
       576,
       1024,
       6232},
      {{.757633984f, .0118629998f},
       {.923740029f, .136439994f},
       {288, 8},
       1.f,
       576,
       0,
       448,
       256,
       796},
      {{.215131998f, .840623021f},
       {.545929015f, .973855972f},
       {288, 112},
       1.f,
       576,
       256,
       448,
       256,
       504},
      {{.0118089998f, .547390997f},
       {.0768219978f, .632857978f},
       {288, 240},
       1.f,
       576,
       512,
       448,
       256,
       586},
      {{.0861430019f, .547390997f},
       {.151155993f, .632857978f},
       {288, 368},
       1.f,
       576,
       768,
       448,
       256,
       586},
  }};
  auto check = [](bool condition, const char *message) {
    // Avoid allocating diagnostic strings for every successful sample.
    if (!condition)
      require(false, std::string("Lia face charts: ") + message);
  };
  auto close = [](float a, float b, float epsilon = 1.e-6f) {
    return std::abs(a - b) <= epsilon;
  };
  auto same_sample = [&](Vec4 a, Vec4 b) {
    return close(a.x, b.x, 3.e-6f) && close(a.y, b.y, 3.e-6f) &&
           close(a.z, b.z, 3.e-6f) && close(a.w, b.w, 3.e-6f);
  };
  const auto &uvs = lia_anatomy::uv_coordinates;
  const auto &corners = lia_anatomy::uv_indices;
  const auto &indices = lia_anatomy::indices;
  check(corners.size() == indices.size() && corners.size() % 3 == 0,
        "source position and per-corner UV streams agree");
  std::vector<std::size_t> parent(uvs.size());
  for (std::size_t i = 0; i < parent.size(); ++i)
    parent[i] = i;
  auto root = [&](std::size_t i) {
    while (parent[i] != i) {
      parent[i] = parent[parent[i]];
      i = parent[i];
    }
    return i;
  };
  for (std::size_t i = 0; i < corners.size(); ++i)
    check(corners[i] < uvs.size() && indices[i] < lia_anatomy::positions.size(),
          "all source corner/position indices are in range");
  for (std::size_t i = 0; i < corners.size(); i += 3) {
    parent[root(corners[i + 1])] = root(corners[i]);
    parent[root(corners[i + 2])] = root(corners[i]);
  }
  std::vector<Vec2> lows(uvs.size(), {2, 2}), highs(uvs.size(), {-1, -1});
  for (auto corner : corners) {
    const auto id = root(corner);
    const auto uv = uvs[corner];
    check(std::isfinite(uv.x) && std::isfinite(uv.y), "source UVs are finite");
    lows[id] = {std::min(lows[id].x, uv.x), std::min(lows[id].y, uv.y)};
    highs[id] = {std::max(highs[id].x, uv.x), std::max(highs[id].y, uv.y)};
  }
  std::vector<unsigned> component_chart(uvs.size(), unsigned(charts.size()));
  std::array<unsigned, 5> components{}, counts{};
  for (std::size_t id = 0; id < uvs.size(); ++id) {
    if (highs[id].x < 0)
      continue;
    unsigned matches = 0;
    for (unsigned c = 0; c < charts.size(); ++c) {
      const auto low = c == 2 ? Vec2{.248307005f, .868807971f} : charts[c].low;
      const auto high =
          c == 2 ? Vec2{.512753010f, .973855972f} : charts[c].high;
      if (close(lows[id].x, low.x, 1.e-8f) &&
          close(lows[id].y, low.y, 1.e-8f) &&
          close(highs[id].x, high.x, 1.e-8f) &&
          close(highs[id].y, high.y, 1.e-8f)) {
        component_chart[id] = c;
        ++components[c];
        ++matches;
      }
    }
    check(matches == 1,
          "each connected source component has its exact Lia bounds");
  }
  check(std::all_of(components.begin(), components.end(),
                    [](unsigned n) { return n == 1; }),
        "exactly one main, mouth, neck and each eye-socket component exist");
  auto area = [](Vec2 a, Vec2 b, Vec2 c) {
    return (double(b.x) - a.x) * (double(c.y) - a.y) -
           (double(b.y) - a.y) * (double(c.x) - a.x);
  };
  constexpr std::array<std::array<float, 3>, 7> weights{{
      {{1, 0, 0}},
      {{0, 1, 0}},
      {{0, 0, 1}},
      {{.5f, .5f, 0}},
      {{0, .5f, .5f}},
      {{.5f, 0, .5f}},
      {{1.f / 3, 1.f / 3, 1.f / 3}},
  }};
  std::array<std::vector<Vec2>, 5> probes;
  for (unsigned c = 0; c < charts.size(); ++c)
    probes[c].reserve(charts[c].triangles * weights.size());
  for (std::size_t i = 0; i < corners.size(); i += 3) {
    const auto component = root(corners[i]);
    const unsigned c = component_chart[component];
    check(c < charts.size(),
          "every source triangle belongs to a supported chart");
    const auto &chart = charts[c];
    ++counts[c];
    std::array<Vec2, 3> source{}, packed{};
    std::array<Vec3, 3> position{};
    for (unsigned j = 0; j < 3; ++j) {
      check(root(corners[i + j]) == component,
            "no triangle bridges source charts");
      source[j] = uvs[corners[i + j]];
      position[j] = lia_anatomy::positions[indices[i + j]];
      packed[j] = character_surface_uv(CharacterSurface::Face, source[j].x,
                                       source[j].y);
      const Vec2 pixel{packed[j].x * 1536, packed[j].y * 1536};
      const Vec2 expected{
          64.5f + 895.f * ((source[j].x - chart.low.x) * chart.scale +
                           chart.origin.x / 447.f),
          64.5f + 895.f * ((source[j].y - chart.low.y) * chart.scale +
                           chart.origin.y / 447.f)};
      check(close(pixel.x, expected.x, .0002f) &&
                close(pixel.y, expected.y, .0002f),
            "every original corner receives the expected positive uniform "
            "affine map");
      check(
          pixel.x >= chart.x + 32 && pixel.x <= chart.x + chart.width - 32 &&
              pixel.y >= chart.y + 32 && pixel.y <= chart.y + chart.height - 32,
          "all corners retain 32 source pixels of chart-local filter padding");
    }
    check(area(source[0], source[1], source[2]) *
                  area(packed[0], packed[1], packed[2]) >
              0,
          "the signed winding of every original per-corner UV triangle is "
          "preserved");
    auto frame = [&](const std::array<Vec2, 3> &uv) {
      const Vec3 e1 = position[1] - position[0], e2 = position[2] - position[0];
      const float inverse = 1.f / float(area(uv[0], uv[1], uv[2]));
      return MaterialTangentFrame{
          normalize((e1 * (uv[2].y - uv[0].y) - e2 * (uv[1].y - uv[0].y)) *
                    inverse),
          normalize((e2 * (uv[1].x - uv[0].x) - e1 * (uv[2].x - uv[0].x)) *
                    inverse)};
    };
    const auto before = frame(source), after = frame(packed);
    check(finite(after.tangent) && finite(after.bitangent) &&
              dot(before.tangent, after.tangent) > .999f &&
              dot(before.bitangent, after.bitangent) > .999f,
          "positive chart packing preserves tangent-space orientation");
    for (unsigned j = 0; j < 3; ++j) {
      const unsigned k = (j + 1) % 3;
      const double original = std::hypot(double(source[k].x) - source[j].x,
                                         double(source[k].y) - source[j].y);
      const double mapped =
          1536 * std::hypot(double(packed[k].x) - packed[j].x,
                            double(packed[k].y) - packed[j].y);
      check(std::abs(mapped - original * 895 * chart.scale) < .0003,
            "every source edge preserves isotropic chart texel density");
    }
    for (const auto &weight : weights) {
      Vec2 original{}, interpolated{};
      for (unsigned j = 0; j < 3; ++j) {
        original.x += source[j].x * weight[j];
        original.y += source[j].y * weight[j];
        interpolated.x += packed[j].x * weight[j];
        interpolated.y += packed[j].y * weight[j];
      }
      const auto mapped =
          character_surface_uv(CharacterSurface::Face, original.x, original.y);
      check(close(mapped.x, interpolated.x, 2.e-7f) &&
                close(mapped.y, interpolated.y, 2.e-7f),
            "corners, edge midpoints and centroids interpolate within one "
            "affine chart");
      probes[c].push_back(
          interpolated); // Interpolate vertices, as the renderer does.
    }
  }
  check(character_surface_atlas_size == 1536 && character_surface_max_mip == 6,
        "the retained atlas contract is 1536 through 24 pixels");
  for (unsigned c = 0; c < charts.size(); ++c) {
    const auto &chart = charts[c];
    check(counts[c] == chart.triangles,
          "each Lia chart retains its original triangle count");
    check(chart.x % 64 == 0 && chart.y % 64 == 0 && chart.width % 64 == 0 &&
              chart.height % 64 == 0,
          "chart boundaries align with the last retained mip's complete filter "
          "blocks");
    for (int level = 0; level <= 6; ++level) {
      const int dimension = 1536 >> level;
      for (const auto uv : probes[c]) {
        const int x = int(std::floor(uv.x * dimension - .5f));
        const int y = int(std::floor(uv.y * dimension - .5f));
        check(x >= (chart.x >> level) &&
                  x + 1 < ((chart.x + chart.width) >> level) &&
                  y >= (chart.y >> level) &&
                  y + 1 < ((chart.y + chart.height) >> level),
              "all four bilinear taps stay in the source chart at every "
              "retained level");
      }
    }
  }
  const auto textures = character_surface_textures();
  check(bool(textures), "shared character atlas exists");
  struct Map {
    const RgbaImage *base;
    const std::vector<RgbaImage> *mips;
    TextureEncoding encoding;
  };
  const std::array<Map, 3> maps{{
      {&textures->base_color, &textures->base_color_mips,
       TextureEncoding::SRGB},
      {&textures->normal, &textures->normal_mips, TextureEncoding::Normal},
      {&textures->metallic_roughness, &textures->metallic_roughness_mips,
       TextureEncoding::Linear},
  }};
  constexpr std::array<float, 14> lods{
      {0, .5f, 1, 1.5f, 2, 2.5f, 3, 3.5f, 4, 4.5f, 5, 5.5f, 6, 100}};
  for (const auto &map : maps) {
    check(map.base->valid() && map.base->width == 1536 &&
              map.base->height == 1536 && map.mips->size() == 6,
          "base, normal and ORM have precisely six retained mips");
    for (unsigned level = 1; level <= 6; ++level) {
      const auto &mip = (*map.mips)[level - 1];
      check(mip.valid() && mip.width == (1536 >> level) &&
                mip.height == (1536 >> level),
            "every retained PBR mip has valid half-size dimensions");
    }
    for (unsigned c = 0; c < charts.size(); ++c) {
      const auto &chart = charts[c];
      // Build a reference from ONLY this chart's base pixels. Neighboring
      // charts/materials literally cannot participate in this filter chain.
      // Together with the four-tap bounds above, equality proves isolation
      // for any values outside the chart, without fifteen full-atlas rebuilds.
      RgbaImage crop{chart.width, chart.height,
                     std::vector<std::uint8_t>(std::size_t(chart.width) *
                                               chart.height * 4)};
      for (int y = 0; y < chart.height; ++y)
        std::copy_n(map.base->pixels.data() +
                        (std::size_t(chart.y + y) * 1536 + chart.x) * 4,
                    std::size_t(chart.width) * 4,
                    crop.pixels.data() + std::size_t(y) * chart.width * 4);
      auto isolated = build_mip_chain(std::move(crop), map.encoding);
      check(isolated.size() > 6,
            "each isolated cell supports all retained mip levels");
      for (unsigned level = 1; level <= 6; ++level) {
        const auto &expected = isolated[level],
                   &actual = (*map.mips)[level - 1];
        for (int y = 0; y < expected.height; ++y) {
          const auto first =
              actual.pixels.begin() +
              (std::size_t((chart.y >> level) + y) * actual.width +
               (chart.x >> level)) *
                  4;
          check(std::equal(expected.pixels.begin() +
                               std::size_t(y) * expected.width * 4,
                           expected.pixels.begin() +
                               std::size_t(y + 1) * expected.width * 4,
                           first),
                "each chart's full mip rectangle equals independently isolated "
                "encoding-correct filtering");
        }
      }
      std::vector<RgbaImage> isolated_mips;
      isolated_mips.reserve(6);
      for (unsigned level = 1; level <= 6; ++level)
        isolated_mips.push_back(std::move(isolated[level]));
      const bool srgb = map.encoding == TextureEncoding::SRGB;
      for (const auto uv : probes[c]) {
        const Vec2 local{(uv.x * 1536 - chart.x) / chart.width,
                         (uv.y * 1536 - chart.y) / chart.height};
        for (float lod : lods) {
          const auto actual =
              sample_material_texture_lod(*map.base, *map.mips, uv, srgb, lod);
          const auto expected = sample_material_texture_lod(
              isolated[0], isolated_mips, local, srgb, lod);
          check(same_sample(actual, expected),
                "every actual corner/edge/interior sample is isolated through "
                "integer, fractional and clamped LODs");
          check(finite(Vec3{actual.x, actual.y, actual.z}) &&
                    close(actual.w, 1.f),
                "all face PBR samples remain finite and opaque at every "
                "retained mip");
          if (map.encoding == TextureEncoding::SRGB)
            check(actual.x > .80f && actual.y > .79f && actual.z > .80f &&
                      actual.x <= 1 && actual.y <= 1 && actual.z <= 1,
                  "sRGB base color decodes to bounded linear skin multipliers");
          else if (map.encoding == TextureEncoding::Normal) {
            const Vec3 normal{actual.x * 2 - 1, actual.y * 2 - 1,
                              actual.z * 2 - 1};
            check(std::abs(length(normal) - 1.f) < .008f && normal.z > .985f,
                  "all retained normal samples remain near-unit and outward");
          } else
            check(close(actual.x, 1.f) && actual.y >= .37f &&
                      actual.y <= .51f && close(actual.z, 0.f),
                  "every ORM sample retains unoccluded, physically bounded "
                  "dielectric skin");
        }
      }
    }
  }
}

void input_validation() {
  for (auto id : {"NpcTeller", "unknown_identity"})
    for (auto lod : {CharacterLod::Near, CharacterLod::Far})
      for (float height : {0.f, -1.f, std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::infinity(),
                           -std::numeric_limits<float>::infinity()}) {
        bool threw = false;
        try {
          make_character_profile(height, CharacterRole::BankStaff, id, lod);
        } catch (const std::invalid_argument &) {
          threw = true;
        }
        require(
            threw,
            "Invalid height is rejected for individual and generic identities");
      }
  for (auto id : {"NpcTeller", "unknown_identity"}) {
    bool bad_role = false, bad_lod = false;
    try {
      make_character_profile(1.68f, CharacterRole::Count, id,
                             CharacterLod::Near);
    } catch (const std::invalid_argument &) {
      bad_role = true;
    }
    try {
      make_character_profile(1.68f, CharacterRole::BankStaff, id,
                             static_cast<CharacterLod>(255));
    } catch (const std::invalid_argument &) {
      bad_lod = true;
    }
    require(bad_role && bad_lod, "Invalid role and LOD fail explicitly");
  }
}
} // namespace

int main(int argc, char **argv) {
  const bool quick = argc > 1 && std::string(argv[1]) == "--quick";
  const bool static_only =
      quick || (argc > 1 && std::string(argv[1]) == "--static");
  unsigned failures = 0;
  auto run = [&](const std::string &name, const auto &check) {
    try {
      check();
    } catch (const std::exception &error) {
      count_allocations = false;
      ++failures;
      std::cerr << name << ": " << error.what() << '\n';
    }
  };
  run("input validation", [] { input_validation(); });
  run("Lia face charts/mips", [] { check_lia_face_charts(); });
  run("generic fallback", [] { generic_identity_fallback(); });
  for (float height : heights) {
    if ((quick || (argc > 1 && std::string(argv[1]) == "--motion")) &&
        height != 1.68f)
      continue;
    const auto near = make_character_profile(height, CharacterRole::BankStaff,
                                             "NpcTeller", CharacterLod::Near);
    const auto far = make_character_profile(height, CharacterRole::BankStaff,
                                            "NpcTeller", CharacterLod::Far);
    run("LOD/material", [&] { check_lods_and_materials(near, far); });
    if (height == 1.68f)
      run("distinct identity", [&] { check_distinct_identity(near); });
    for (const auto *p : {&near, &far}) {
      const auto &m = p->model;
      const auto name = label(m);
      run(name + " geometry", [&] { check_geometry(*p); });
      run(name + " preserved anatomy", [&] { check_anatomical_head(m); });
      run(name + " actual socket fit", [&] { check_socket_fit(m); });
      run(name + " hair cards", [&] { check_hair_cards(m); });
      run(name + " opaque scalp coverage",
          [&] { check_opaque_scalp_coverage(m); });
      run(name + " periodic seams", [&] { check_periodic_seam_copies(m); });
      run(name + " rear collar seam support",
          [&] { check_rear_collar_seam_support(m); });
      for (auto surface : {CharacterSurface::Skin, CharacterSurface::Metal})
        run(name + " closed material " + std::to_string(int(surface)),
            [&] { check_closed_surface_seams(m, surface); });
      run(name + " repeat identity", [&] {
        const auto repeated = make_character_profile(
            height, CharacterRole::BankStaff, "NpcTeller", m.lod);
        require(exact(m, repeated.model) &&
                    p->material.textures == repeated.material.textures &&
                    exact(m.appearance,
                          character_appearance(character_seed("NpcTeller"))),
                "Lia generation preserves every deterministic "
                "geometry/rig/weight/palette bit");
      });
      run(name + " sewn shirt bind", [&] {
        SewnShirtCheck seams(m);
        seams.check(m.bind_mesh, name + " bind: ");
      });
      run(name + " tucked shirt", [&] {
        TuckedShirtCheck tuck(m);
        tuck.check(m.bind_mesh, CharacterPose{}, name);
      });
      run(name + " sewn trousers", [&] {
        TrouserSeamCheck trousers(m);
        trousers.check(m.bind_mesh, name);
      });
      run(name + " knee bind", [&] {
        KneeCrossingCheck knees(m);
        knees.check(m.bind_mesh, name + " bind: ");
      });
      if (!static_only)
        run(name + " animation", [&] { check_animation(m); });
    }
  }
  if (failures) {
    std::cerr << failures << " Lia character checks failed\n";
    return 1;
  }
  std::cout
      << (static_only
              ? "Lia static geometry checks passed; animation was not run\n"
              : "Lia character geometry and deformation checks passed\n");
  return 0;
}
