#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/ghost_anatomy.hpp"
#include "fury/generated/ghost_hands.hpp"
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
constexpr std::array<float, 3> heights{{1.35f, 1.80f, 2.10f}};
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
  require(profile.individualized && m.role == CharacterRole::Ghost &&
              m.seed == 0x46b6a22eu && m.seed == character_seed("GhostLoop"),
          context + "actual GhostLoop identity/role");
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
    if (s == static_cast<int>(CharacterSurface::HairShortStrands) || s == static_cast<int>(CharacterSurface::HairStrands))
      require(w.first == bone_index(CharacterBone::Head) &&
                  w.first_weight == 1 && w.third_weight == 0,
              where + " strand card remains rigidly attached to the skull");
    if (w.first == bone_index(CharacterBone::LeftFoot))
      soles[0] = std::min(soles[0], v.position.y);
    if (w.first == bone_index(CharacterBone::RightFoot))
      soles[1] = std::min(soles[1], v.position.y);
    bounds.add(v.position);
  }
  // GhostLoop's wardrobe and groom have an explicit material contract; adding an
  // unrelated atlas region must not require every actor to use that region.
  for (auto surface : {CharacterSurface::Face, CharacterSurface::Jacket,
                       CharacterSurface::Hair, CharacterSurface::HairCrop,
                       CharacterSurface::Skin, CharacterSurface::Shirt,
                       CharacterSurface::Pants, CharacterSurface::Leather,
                       CharacterSurface::Eyes, CharacterSurface::Rubber,
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

void check_lods_and_materials(const CharacterProfile &near,
                              const CharacterProfile &far) {
  const auto &a = near.model;
  const auto &b = far.model;
  const auto frozen = character_appearance(0x46b6a22eu);
  require(a.seed==0x46b6a22eu && exact(a.appearance,frozen) &&
          exact(a.appearance.shirt,{.48f,.23f,.16f}) &&
          exact(a.appearance.trousers,{.16f,.15f,.20f}) &&
          exact(a.appearance.hair,{.36f,.22f,.105f}),
          "GhostLoop preserves its frozen original rust/plum/chestnut palette and complete appearance");
  const auto original=make_character_model(a.rig.height,a.role,a.seed,a.lod);
  for(Vec3 color : {Vec3{.11f,.49f,.53f},Vec3{.57f,.79f,.75f}}) {
    auto has=[&](const CharacterModel& model){return std::any_of(model.bind_mesh.vertices.begin(),model.bind_mesh.vertices.end(),[&](const Vertex& v){return exact(v.color,color);});};
    require(has(a)&&has(original),"Actual original cyan vest and horizontal identification stripe colors retained");
  }
  require(exact(a.rig, b.rig) && exact(a.appearance, b.appearance) &&
              a.seed == b.seed && a.dual_quaternion_skinning &&
              b.dual_quaternion_skinning,
          "Near/far GhostLoop retains bit-identical rig, identity and opt-in "
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
        CharacterSurface::Jacket, CharacterSurface::Leather}) {
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
             ghost_anatomy::head_center * h) < h * 1.e-6f &&
          length(
              model.rig.joints[bone_index(CharacterBone::Neck)].bind_position -
              ghost_anatomy::neck_center * h) < h * 1.e-6f,
      context + "head and neck rig pivots use the morphed source landmarks");
  std::map<std::array<float, 3>, std::uint32_t> source_positions;
  for (std::uint32_t i = 0; i < ghost_anatomy::positions.size(); ++i) {
    const auto p = ghost_anatomy::positions[i] * h;
    source_positions.emplace(std::array<float, 3>{{p.x, p.y, p.z}}, i);
  }
  require(source_positions.size() == ghost_anatomy::positions.size(),
          context + "source geometry has unambiguous canonical positions");
  std::vector<std::uint32_t> canonical(model.bind_mesh.vertices.size(),
                                       UINT32_MAX);
  std::vector<std::uint32_t> first_copy(ghost_anatomy::positions.size(),
                                        UINT32_MAX);
  std::vector<bool> retained(ghost_anatomy::positions.size(), false);
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
    require(source_corner + 2 < ghost_anatomy::uv_indices.size(),
            context + "no added anatomy triangle");
    const std::uint32_t corners[]{a, b, c};
    for (std::size_t corner = 0; corner < 3; ++corner) {
      const auto uv =
          ghost_anatomy::uv_coordinates[ghost_anatomy::uv_indices[source_corner +
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
      actual_indices.size() == ghost_anatomy::indices.size() &&
          std::equal(actual_indices.begin(), actual_indices.end(),
                     ghost_anatomy::indices.begin()),
      context +
          "all source triangles survive intact and in their original winding");
  for (std::size_t side = 0; side < 2; ++side) {
    require(eye_vertices[side] > 0,
            context + "both source eye sockets contain actual eye geometry");
    const auto center = ghost_anatomy::eye_centers[side] * h;
    const auto helper = ghost_anatomy::eye_helper_half_extents[side] * h;
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

std::array<std::vector<Vec3>, 2> source_eye_openings() {
  std::map<std::pair<std::uint32_t, std::uint32_t>, unsigned> charts;
  for (std::size_t i = 0; i < ghost_anatomy::indices.size(); i += 3) {
    const auto uv = ghost_anatomy::uv_coordinates[ghost_anatomy::uv_indices[i]];
    const unsigned chart = uv.x < .08f ? 2 : uv.x < .16f ? 1 : 0;
    for (int c = 0; c < 3; ++c) {
      auto a = ghost_anatomy::indices[i + c],
           b = ghost_anatomy::indices[i + (c + 1) % 3];
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
      result[side].push_back(ghost_anatomy::positions[i]);
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

// Chart-based selection covers the complete cavity and entire exterior jaw,
// including positions below conventional horizontal neck blend thresholds.
void check_complete_head_motion(const CharacterModel& m) {
  std::array<bool,ghost_anatomy::positions.size()> head{},mouth{},outside{};
  for(std::size_t t=0;t<ghost_anatomy::indices.size();t+=3) {
    const auto uv=ghost_anatomy::uv_coordinates[ghost_anatomy::uv_indices[t]];
    const bool external=uv.x>=.641249f&&uv.y>=.148696f&&uv.y<=.8176341f;
    const bool cavity=uv.x>=.757633f&&uv.x<=.923741f&&uv.y<.136441f;
    const bool socket=uv.x<.16f&&uv.y>.54f&&uv.y<.64f;
    for(int k=0;k<3;++k) {
      const unsigned a=ghost_anatomy::indices[t+k];
      head[a]=head[a]||external||cavity||socket;
      mouth[a]=mouth[a]||cavity;outside[a]=outside[a]||external;
    }
  }
  using Key=std::array<float,3>;
  std::map<Key,unsigned> canonical;
  for(unsigned i=0;i<head.size();++i) {
    const auto p=ghost_anatomy::positions[i]*m.rig.height;
    canonical[{p.x,p.y,p.z}]=i;
  }
  std::vector<unsigned> actual;std::set<unsigned> retained,mouth_retained,outside_retained;
  for(unsigned i=0;i<m.bind_mesh.vertices.size();++i) {
    const auto& v=m.bind_mesh.vertices[i];
    if(surface_at(v.uv)!=int(CharacterSurface::Face))continue;
    const auto source=canonical.at({v.position.x,v.position.y,v.position.z});
    if(!head[source])continue;
    actual.push_back(i);retained.insert(source);
    if(mouth[source])mouth_retained.insert(source);
    if(outside[source])outside_retained.insert(source);
    const auto& w=m.influences[i];
    require(w.first==bone_index(CharacterBone::Head)&&w.first_weight==1.f&&w.third_weight==0.f,
      "Complete canonical exterior jaw, mouth and eye socket require rigid Head influence");
  }
  require(retained.size()==std::count(head.begin(),head.end(),true)&&retained.size()>4000,
    "Every source-chart head vertex is retained and tested without a Y threshold");
  require(mouth_retained.size()==std::count(mouth.begin(),mouth.end(),true)&&mouth_retained.size()>300,
    "The complete mouth cavity is included, not only its lip seam");
  float worst=0;unsigned poses=0;auto output=m.bind_mesh;
  for(float yaw:{-.9f,0.f,.9f})for(float pitch:{-.4f,.4f})for(float time:{0.f,.75f,1.5f,2.25f}) {
    CharacterAnimationSample sample;sample.seed=m.seed;sample.idle_time=time;
    sample.attention_weight=sample.talk_weight=1.f;sample.attention_yaw=yaw;sample.attention_pitch=pitch;
    const auto pose=sample_character_animation(m.rig,sample);apply_character_pose(m,pose,output);
    for(unsigned i:actual) {
      const auto expected=transform_point(pose.skin_matrices[bone_index(CharacterBone::Head)],m.bind_mesh.vertices[i].position);
      const float error=length(output.vertices[i].position-expected)/m.rig.height;
      worst=std::max(worst,error);require(error<2e-6f,"Complete source jaw/mouth follows actual Head matrix under combined attention");
    }
    ++poses;
  }
  std::cout<<label(m)<<": complete head chart motion "<<retained.size()<<" canonical positions, "
    <<mouth_retained.size()<<" complete-mouth, "<<outside_retained.size()<<" exterior head/jaw, "<<poses<<" combined poses, max error/h="<<worst<<'\n';
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
  for (std::size_t t = 0; t < ghost_anatomy::indices.size(); t += 3) {
    const auto uv = ghost_anatomy::uv_coordinates[ghost_anatomy::uv_indices[t]];
    if (uv.x > .6f && uv.y > .14f)
      skull.push_back(t);
  }
  std::size_t count = 0;
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
    const auto &a = mesh.vertices[mesh.indices[t]],
               &b = mesh.vertices[mesh.indices[t + 1]],
               &c = mesh.vertices[mesh.indices[t + 2]];
    if (surface_at(a.uv) != int(CharacterSurface::HairShortStrands) && surface_at(a.uv) != int(CharacterSurface::HairStrands))
      continue;
    const auto center =
        (a.position + b.position + c.position) * (1.f / (3 * h));
    float best = std::numeric_limits<float>::infinity();
    Vec3 nearest{}, normal{};
    std::size_t source = 0;
    for (auto offset : skull) {
      const auto p = ghost_anatomy::positions[ghost_anatomy::indices[offset]];
      const auto q = ghost_anatomy::positions[ghost_anatomy::indices[offset + 1]];
      const auto r = ghost_anatomy::positions[ghost_anatomy::indices[offset + 2]];
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

// Opaque coverage is a triangle property: cap vertices outside the skull do not
// keep coarse chords outside it. Inspect actual emitted Face/opaque-groom triangles
// and the cap's actual open root, without repeating the authored crop formula.
void check_crop_coverage(const CharacterModel &model) {
  const auto &mesh = model.bind_mesh;
  const float h = model.rig.height;
  struct Triangle {
    std::array<Vec3, 3> p;
    Bounds bounds;
    std::size_t offset;
  };
  std::vector<Triangle> face, cap;
  // Identify the actual physical opaque cap, independently of triangle order.
  // Coverage, winding and sample thresholds apply to the complete component.
  std::vector<unsigned> parents(mesh.vertices.size());
  for(unsigned i=0;i<parents.size();++i)parents[i]=i;
  const auto physical_root=[&](unsigned i){while(parents[i]!=i){parents[i]=parents[parents[i]];i=parents[i];}return i;};
  const auto join=[&](unsigned a,unsigned b){parents[physical_root(b)]=physical_root(a);};
  std::map<std::array<long long,3>,unsigned> physical;
  for(unsigned i=0;i<mesh.vertices.size();++i)if(surface_at(mesh.vertices[i].uv)==int(CharacterSurface::HairCrop)) {
    const auto p=mesh.vertices[i].position*(1/h);const std::array<long long,3> key{{std::llround(p.x*1e7),std::llround(p.y*1e7),std::llround(p.z*1e7)}};
    join(i,physical.emplace(key,i).first->second);
  }
  for(unsigned t=0;t<mesh.indices.size();t+=3)if(surface_at(mesh.vertices[mesh.indices[t]].uv)==int(CharacterSurface::HairCrop)){join(mesh.indices[t],mesh.indices[t+1]);join(mesh.indices[t],mesh.indices[t+2]);}
  std::map<unsigned,unsigned> counts;
  for(unsigned t=0;t<mesh.indices.size();t+=3)if(surface_at(mesh.vertices[mesh.indices[t]].uv)==int(CharacterSurface::HairCrop))++counts[physical_root(mesh.indices[t])];
  require(!counts.empty(),"Actual opaque swept groom exists");
  const auto cap_physical_root=std::max_element(counts.begin(),counts.end(),[](const auto&a,const auto&b){return a.second<b.second;})->first;
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
    const bool is_cap=surface==int(CharacterSurface::HairCrop)&&physical_root(mesh.indices[t])==cap_physical_root;
    if (surface != int(CharacterSurface::Face) && !is_cap)continue;
    Triangle triangle{{}, {}, t / 3};
    for (unsigned i = 0; i < 3; ++i) {
      const auto vertex = mesh.indices[t + i];
      triangle.p[i] = mesh.vertices[vertex].position * (1.f / h);
      triangle.bounds.add(triangle.p[i]);
      if (is_cap) {
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
  require(!cap.empty() && face.size() == ghost_anatomy::indices.size() / 3,
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
  require(root.size() >= 24 && root_low > .399f,
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

// The two rolled leather sheets are open, nonperiodic indexed patches. Closed
// bag/shoe/strap tubes have coincident periodic corners and are excluded by
// topology, not hard-coded creation order or tessellation.

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

void check_distinct_identity(const CharacterProfile &ghost) {
  const auto &m = ghost.model;
  for (auto id : {"NpcCivA", "NpcCivB", "NpcCivC"}) {
    const auto role = std::string_view(id) == "NpcCivB"
                          ? CharacterRole::Market
                          : CharacterRole::Commuter;
    const auto other = make_character_profile(m.rig.height, role, id, m.lod);
    require(other.individualized && other.model.seed != m.seed &&
                !exact(other.model.appearance, m.appearance) &&
                !exact(other.model.rig, m.rig),
            "GhostLoop retains an individual stable identity, appearance and "
            "anatomical rig");
    Bounds a, b;
    for (const auto &v : m.bind_mesh.vertices)
      if (surface_at(v.uv) == int(CharacterSurface::Face))
        a.add(v.position);
    for (const auto &v : other.model.bind_mesh.vertices)
      if (surface_at(v.uv) == int(CharacterSurface::Face))
        b.add(v.position);
    std::map<std::pair<float, float>, Vec3> corresponding;
    for (const auto &v : other.model.bind_mesh.vertices)
      if (surface_at(v.uv) == int(CharacterSurface::Face))
        corresponding[{v.uv.x, v.uv.y}] = v.position;
    double sum = 0;
    unsigned matches = 0;
    for (const auto &v : m.bind_mesh.vertices)
      if (surface_at(v.uv) == int(CharacterSurface::Face)) {
        auto found = corresponding.find({v.uv.x, v.uv.y});
        if (found == corresponding.end())
          continue;
        auto delta = v.position - found->second;
        sum += dot(delta, delta);
        ++matches;
      }
    double rms = std::sqrt(sum / std::max(1u, matches)) / m.rig.height;
    std::cout << "GhostLoop versus " << id
              << " source-UV-correspondent anatomical RMS/h=" << rms << " over "
              << matches << " corners\n";
    require(matches > 4000 && rms > .0005,
            "Independent face differs across corresponding source skin, not "
            "only total bounding dimensions");
  }
  const auto generic =
      make_character_model(m.rig.height, m.role, m.seed, m.lod);
  require(!exact(m.bind_mesh, generic.bind_mesh) && !exact(m.rig, generic.rig),
          "Registered GhostLoop profile replaces the generic body and rig");
}

void generic_identity_fallback() {
  struct Identity {
    const char *id;
    CharacterRole role;
  };
  using R = CharacterRole;
  // Unknown identities exercise every role without making an assertion that
  // another named roster actor must remain generic in future individual work.
  constexpr Identity roster[]{{"unregistered_commuter", R::Commuter},
                              {"unregistered_market", R::Market},
                              {"unregistered_dock", R::Ghost},
                              {"unregistered_security", R::Security},
                              {"unregistered_fence", R::Fence},
                              {"unregistered_scout", R::CrewScout},
                              {"unregistered_tech", R::CrewTech},
                              {"unregistered_enforcer", R::Enforcer},
                              {"unregistered_player", R::Player},
                              {"unregistered_ghost", R::Ghost},
                              {"unregistered_staff", R::BankStaff},
                              {"ghostloop", R::Ghost},
                              {"GhostLoop_extra", R::Ghost},
                              {"", R::Ghost}};
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
        if (static_cast<R>(r) != R::Ghost)
          check(h, static_cast<R>(r), "GhostLoop", lod);
      check(h, R::Ghost, std::string_view("GhostLoop\0extra", 13), lod);
    }
  std::cout << cases
            << " bit-exact generic identity/role/height/LOD fallback cases\n";
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
  auto join = [&](unsigned a, unsigned b) { parent[root(b)] = root(a); };
  using Key = std::array<long long, 3>;
  std::map<Key, unsigned> unique;
  for (unsigned i = 0; i < m.vertices.size(); ++i)
    if (surface_at(m.vertices[i].uv) == int(surface)) {
      auto p = m.vertices[i].position * (1 / h);
      Key k{{std::llround(p.x * 1e7), std::llround(p.y * 1e7),
             std::llround(p.z * 1e7)}};
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
        c.copies.push_back({i, canonical[i]});
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

struct GarmentAudit {
  const CharacterModel &m;
  Component shirt, vest, pants;
  std::vector<std::pair<unsigned, unsigned>> sewn;
  explicit GarmentAudit(const CharacterModel &model) : m(model) {
    const auto shirts = components(m, CharacterSurface::Shirt),
               vests = components(m, CharacterSurface::Jacket);
    require(!shirts.empty() && !vests.empty(),
            "Both actual layered garments exist");
    shirt = shirts[0];
    vest = vests[0];
    const auto trouser_parts = components(m, CharacterSurface::Pants);
    require(!trouser_parts.empty(), "Actual trousers exist");
    pants = trouser_parts[0];
    std::map<unsigned, std::vector<unsigned>> pants_graph;
    for (auto e : pants.boundary) {
      pants_graph[e.first].push_back(e.second);
      pants_graph[e.second].push_back(e.first);
    }
    std::set<unsigned> pending;
    for (auto e : pants_graph) {
      require(e.second.size() == 2, "Trouser boundaries are unbranched");
      pending.insert(e.first);
    }
    int pants_loops = 0;
    while (!pending.empty()) {
      ++pants_loops;
      std::vector<unsigned> stack{*pending.begin()};
      pending.erase(stack[0]);
      while (!stack.empty()) {
        auto i = stack.back();
        stack.pop_back();
        for (auto j : pants_graph[i])
          if (pending.erase(j))
            stack.push_back(j);
      }
    }
    require(pants_loops == 3 && pants.euler == -1,
            "Connected trousers retain only waist and two ankle boundaries");
    require(vest.boundary.empty(),
            "Vest outer, lining and every cut edge form a closed shell");
    require(vest.volume > m.rig.height * m.rig.height * m.rig.height * 1e-5,
            "Actual vest has positive signed cloth thickness");
    std::map<unsigned, std::vector<unsigned>> edge_graph;
    for (auto e : shirt.boundary) {
      edge_graph[e.first].push_back(e.second);
      edge_graph[e.second].push_back(e.first);
    }
    for (auto e : edge_graph)
      require(e.second.size() == 2,
              "Shirt attachment boundaries form unbranched cycles");
    std::set<unsigned> left;
    for (auto e : edge_graph)
      left.insert(e.first);
    int loops = 0;
    std::vector<Bounds> rings;
    while (!left.empty()) {
      ++loops;
      Bounds ring;
      std::vector<unsigned> stack{*left.begin()};
      left.erase(stack[0]);
      while (!stack.empty()) {
        unsigned i = stack.back();
        stack.pop_back();
        ring.add(m.bind_mesh.vertices[i].position * (1 / m.rig.height));
        for (auto j : edge_graph[i])
          if (left.erase(j))
            stack.push_back(j);
      }
      rings.push_back(ring);
    }
    require(loops == 4, "Sewn shirt has only neck, hem and two open cuffs, "
                        "with no armhole cracks");
    bool hem = false, neck = false, left_cuff = false, right_cuff = false;
    for (auto ring : rings) {
      hem |= ring.high.y < .09f;
      neck |= ring.low.y > .32f;
      left_cuff |= ring.high.x < -.1f;
      right_cuff |= ring.low.x > .1f;
    }
    require(hem && neck && left_cuff && right_cuff,
            "Four boundaries occupy real neck/hem/cuff locations");
    for (const auto *garment : {&shirt, &vest, &pants})
      for (auto e : garment->copies) {
        require(exact(m.influences[e.first], m.influences[e.second]),
                "Sewn physical copies keep identical bone weights");
        sewn.push_back(e);
      }
    require(sewn.size() > 50,
            "Many actual garment UV/edge seams are exercised");
    std::cout << label(m)
              << ": sewn shirt four boundaries, trousers three boundaries; "
                 "closed lined vest Euler="
              << vest.euler << ", thickness volume/h^3="
              << vest.volume / std::pow(m.rig.height, 3) << '\n';
  }
  void check_seams(const Mesh &pose) const {
    for (auto e : sewn)
      require(length(pose.vertices[e.first].position -
                     pose.vertices[e.second].position) < m.rig.height * 2e-7f,
              "A real pose separates a sewn physical boundary");
  }
};

void check_hands(const CharacterModel &m) {
  const float h = m.rig.height;
  std::array<unsigned, 2> retained{};
  for (float side : {-1.f, 1.f}) {
    const auto bone = bone_index(side > 0 ? CharacterBone::RightHand
                                          : CharacterBone::LeftHand);
    std::map<std::array<float, 3>, unsigned> source;
    const float a = side * (51.f * 3.14159265358979323846f / 180.f),
                cs = std::cos(a), sn = std::sin(a);
    for (unsigned i = 0; i < ghost_hands::positions.size(); ++i) {
      Vec3 p = ghost_hands::positions[i];
      p.x *= side;
      p = {p.x * cs + p.z * sn, p.y, -p.x * sn + p.z * cs};
      p += Vec3{side * .180f, -.039f, .008f};
      p = p * h;
      source[{{p.x, p.y, p.z}}] = i;
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
        auto found = source.find({{p.x, p.y, p.z}});
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
                              ? ghost_hands::near_indices.size() / 3
                              : ghost_hands::far_indices.size() / 3;
    require(
        triangles == expected,
        "Every expected connected hand triangle survives rigid fit; actual=" +
            std::to_string(triangles) +
            " expected=" + std::to_string(expected));
    for (auto i : ghost_hands::wrist_boundary_indices)
      require(used.count(i), "Complete anatomical wrist retained in each LOD");
    retained[side > 0 ? 1 : 0] = used.size();
  }
  require(retained[0] == retained[1] && retained[0] > 200,
          "Both source-correspondent hands have matching topology");
}

void check_animation(const CharacterModel &m, bool quick) {
  const auto bind = m.bind_mesh;
  const float h = m.rig.height;
  GarmentAudit garments(m);
  Mesh out = bind;
  const auto *vp = out.vertices.data();
  const auto *ip = out.indices.data();
  const auto identity = out.geometry_identity;
  out.gpu_vao = 101;
  out.gpu_vbo = 102;
  out.gpu_ibo = 103;
  out.gpu_uploaded = true;
  unsigned count = 0;
  for (int mode = 0; mode < 6; ++mode)
    for (int frame = 0; frame <= 32; frame += quick ? 8 : 1) {
      const auto s = sample_for(mode, frame, h, m.seed);
      const auto pose = sample_character_animation(m.rig, s);
      allocations = 0;
      count_allocations = true;
      apply_character_pose(m, pose, out);
      count_allocations = false;
      require(allocations == 0,
              "Production skinning does not allocate per frame");
      require(out.vertices.data() == vp && out.indices.data() == ip &&
                  out.geometry_identity == identity &&
                  out.indices == bind.indices && out.gpu_vao == 101 &&
                  out.gpu_vbo == 102 && out.gpu_ibo == 103 && out.gpu_uploaded,
              "Pose preserves exact mesh topology/storage/renderer ownership");
      auto rev = out.geometry_revision;
      require(!apply_character_pose(m, pose, out) &&
                  rev == out.geometry_revision,
              "Identical repeated pose does not dirty geometry");
      for (unsigned i = 0; i < out.vertices.size(); ++i) {
        const auto &v = out.vertices[i];
        const auto &w = m.influences[i];
        require_element(finite(v.position) && finite(v.normal) &&
                            std::fabs(length(v.normal) - 1) < .0002f,
                        label(m), " finite posed vertex/unit normal ", i);
        require_element(exact(v.color, bind.vertices[i].color) &&
                            exact(v.uv.x, bind.vertices[i].uv.x) &&
                            exact(v.uv.y, bind.vertices[i].uv.y),
                        label(m), " exact pose UV/color ", i);
        require_element(length(v.position) < h * 1.1f, label(m),
                        " non-exploding pose ", i);
        if (w.first == bone_index(CharacterBone::LeftFoot) ||
            w.first == bone_index(CharacterBone::RightFoot))
          require_element(v.position.y >= -.5001f * h, label(m),
                          " shoe ground clearance ", i);
      }
      for (std::size_t i = 0; i < out.indices.size(); i += 3) {
        auto a = out.vertices[out.indices[i]].position,
             b = out.vertices[out.indices[i + 1]].position,
             c = out.vertices[out.indices[i + 2]].position;
        require_element(length(cross(b - a, c - a)) > h * h * 1e-13f, label(m),
                        " noncollapsed actual posed triangle ", i / 3);
      }
      garments.check_seams(out);
      ++count;
    }
  for (float yaw : {-.9f, 0.f, .9f})
    for (float pitch : {-.4f, .4f})
      for (int frame : {0, 8, 16, 24, 32}) {
        auto s = sample_for(3, frame, h, m.seed);
        s.attention_yaw = yaw;
        s.attention_pitch = pitch;
        apply_character_pose(m, sample_character_animation(m.rig, s), out);
        garments.check_seams(out);
        ++count;
      }
  require(exact(m.bind_mesh, bind), "Pose leaves immutable source unchanged");
  std::cout << label(m) << ": " << count
            << " actual idle/walk/run/talk/crouch/crouch-walk and attention "
               "poses passed\n";
}

void check_tape_coverage(const CharacterModel &m) {
  const auto &mesh = m.bind_mesh;
  const float h = m.rig.height;
  unsigned samples = 0;
  std::vector<unsigned> tape, cloth;
  for (unsigned t = 0; t < mesh.indices.size(); t += 3) {
    const auto &v = mesh.vertices[mesh.indices[t]];
    if (surface_at(v.uv) == int(CharacterSurface::Jacket))
      cloth.push_back(t);
    else if (surface_at(v.uv) == int(CharacterSurface::Shirt) &&
             v.color.x > .56f && v.color.y > .78f && v.color.z > .74f)
      tape.push_back(t);
  }
  require(tape.size() > 30 && cloth.size() > 100, "Real tapes and vest exist: tape="+std::to_string(tape.size())+" cloth="+std::to_string(cloth.size()));
  for (int facing : {-1, 1})
    for (int side : {-1, 1})
      for (int ix = -1; ix <= 1; ++ix)
        for (int iy = 0; iy < 48; ++iy) {
          Vec3 origin{(side * (.015f + .080f * iy / 47)) * h,
                      (.178f + ix * .003f) * h,
                      facing * .25f * h},
              ray{0, 0, float(-facing)};
          auto distance = [&](const std::vector<unsigned> &group) {
            float nearest = 100;
            for (auto t : group) {
              Vec3 a = mesh.vertices[mesh.indices[t]].position,
                   e = mesh.vertices[mesh.indices[t + 1]].position - a,
                   f = mesh.vertices[mesh.indices[t + 2]].position - a,
                   q = cross(ray, f), off = origin - a, r = cross(off, e);
              float det = dot(e, q);
              if (std::fabs(det) < 1e-12f)
                continue;
              float u = dot(off, q) / det, v = dot(ray, r) / det,
                    d = dot(f, r) / det;
              if (u >= -1e-6f && v >= -1e-6f && u + v <= 1.000001f && d > 0)
                nearest = std::min(nearest, d);
            }
            return nearest;
          };
          float garment = distance(cloth), trim = distance(tape),
                gap = (garment - trim) / h;
          require(garment < .24f * h && trim < .24f * h && gap > .00035f &&
                      gap < .0015f,
                  "Actual tape remains outside every sampled vest face");
          ++samples;
        }
  std::cout << label(m) << ": " << samples << " actual tape/vest rays pass\n";
}

void check_seat_coverage(const CharacterModel &m) {
  const auto &mesh = m.bind_mesh;
  const float h = m.rig.height;
  unsigned count = 0;
  // Independent front/back rays sample the physical central seat below the
  // waist. Coarse angular layouts can miss the quarter-ring center seam.
  for (int facing : {-1, 1})
    for (int ix = -4; ix <= 4; ++ix)
      for (int iy = 0; iy <= 6; ++iy) {
        Vec3 origin{ix * .002f * h, (.027f + .004f * iy) * h,
                    facing * .20f * h},
            direction{0, 0, float(-facing)};
        float distance = 100;
        for (unsigned t = 0; t < mesh.indices.size(); t += 3) {
          const auto &a = mesh.vertices[mesh.indices[t]];
          if (surface_at(a.uv) != int(CharacterSurface::Pants))
            continue;
          Vec3 e = mesh.vertices[mesh.indices[t + 1]].position - a.position,
               f = mesh.vertices[mesh.indices[t + 2]].position - a.position,
               q = cross(direction, f), offset = origin - a.position;
          float determinant = dot(e, q);
          if (std::fabs(determinant) < 1e-12f)
            continue;
          Vec3 r = cross(offset, e);
          float u = dot(offset, q) / determinant,
                v = dot(direction, r) / determinant,
                d = dot(f, r) / determinant;
          if (u >= -1e-6f && v >= -1e-6f && u + v <= 1.000001f && d > 0)
            distance = std::min(distance, d);
        }
        require(distance < .18f * h, "Actual front/back central seat coverage");
        ++count;
      }
  std::cout << label(m) << ": " << count << " actual seat rays pass\n";
}

void controls(const CharacterModel &m) {
  auto expect_reject = [](auto action, const char *why) {
    bool rejected = false;
    try {
      action();
    } catch (const std::exception &) {
      rejected = true;
    }
    require(rejected, why);
  };
  auto bad = m;
  for (auto &v : bad.bind_mesh.vertices)
    if (surface_at(v.uv) == int(CharacterSurface::Shirt) && v.color.x > .56f &&
        v.color.y > .78f && v.color.z > .74f)
      v.position.z -= std::copysign(.003f * m.rig.height, v.position.z);
  expect_reject([&] { check_tape_coverage(bad); },
                "Actual buried tape must fail cloth coverage");
  bad = m;
  // Remove an actual central-seat neighborhood, then require rays to expose
  // the opening. This rejects a geometric hole without constructor metadata.
  std::vector<std::uint32_t> intact;
  for (unsigned t = 0; t < bad.bind_mesh.indices.size(); t += 3) {
    Vec3 center{};
    for (int k = 0; k < 3; ++k)
      center += bad.bind_mesh.vertices[bad.bind_mesh.indices[t + k]].position *
                (1.f / (3.f * m.rig.height));
    if (surface_at(bad.bind_mesh.vertices[bad.bind_mesh.indices[t]].uv) ==
            int(CharacterSurface::Pants) &&
        std::fabs(center.x) < .025f && center.y > .012f && center.z < 0)
      continue;
    for (int k = 0; k < 3; ++k)
      intact.push_back(bad.bind_mesh.indices[t + k]);
  }
  bad.bind_mesh.indices = std::move(intact);
  expect_reject([&] { check_seat_coverage(bad); },
                "Actual missing seat neighborhood must fail coverage");
  bad = m;
  std::size_t head = 0;
  while (surface_at(bad.bind_mesh.vertices[head].uv) !=
         int(CharacterSurface::Face))
    ++head;
  bad.bind_mesh.vertices[head].position.x += m.rig.height * .0001f;
  expect_reject([&] { check_anatomical_head(bad); },
                "Source-position perturbation control must fail");
  bad = m;
  bad.bind_mesh.vertices[head].uv.x += .0001f;
  expect_reject([&] { check_anatomical_head(bad); },
                "Source-UV perturbation control must fail");
  for (bool cavity : {true,false}) {
    bad=m;unsigned selected=UINT32_MAX;
    for(std::size_t t=0;t<ghost_anatomy::indices.size()&&selected==UINT32_MAX;t+=3) {
      const auto uv=ghost_anatomy::uv_coordinates[ghost_anatomy::uv_indices[t]];
      const bool matches=cavity?(uv.x>.75f&&uv.x<.93f&&uv.y<.13645f):(uv.x>.64f&&uv.y>.1486f&&uv.y<.81764f);
      if(!matches)continue;
      for(int k=0;k<3&&selected==UINT32_MAX;++k) {
        const auto p=ghost_anatomy::positions[ghost_anatomy::indices[t+k]]*m.rig.height;
        for(unsigned j=0;j<bad.bind_mesh.vertices.size();++j)if(surface_at(bad.bind_mesh.vertices[j].uv)==int(CharacterSurface::Face)&&exact(bad.bind_mesh.vertices[j].position,p)) {selected=j;break;}
      }
    }
    require(selected!=UINT32_MAX,"Real source chart exists for rig negative control");
    bad.influences[selected]={std::uint8_t(CharacterBone::Neck),std::uint8_t(CharacterBone::Neck),1.f};
    expect_reject([&]{check_complete_head_motion(bad);},"Actual mouth/exterior source rig corruption must fail");
  }
  bad = m;
  const auto vest = components(m, CharacterSurface::Jacket)[0];
  const auto t = vest.triangles[0];
  std::swap(bad.bind_mesh.indices[t + 1], bad.bind_mesh.indices[t + 2]);
  expect_reject([&] { GarmentAudit q(bad); },
                "One reversed shell face must fail manifold winding");
  bad = m;
  bad.bind_mesh.indices.erase(bad.bind_mesh.indices.begin() + t,
                              bad.bind_mesh.indices.begin() + t + 3);
  expect_reject(
      [&] { GarmentAudit q(bad); },
      "A real missing shell triangle must fail watertight boundary proof");
  bad = m;
  const auto shirts = components(m, CharacterSurface::Shirt);
  auto seam = shirts[0].copies.front();
  bad.influences[seam.first].first_weight = .3f;
  expect_reject([&] { GarmentAudit q(bad); },
                "Mismatched physical seam weights must fail");
  Vec3 a{0, 0, 0}, b{1, 0, 0}, c{0, 1, 0};
  require(proper_segment_triangle({.2f, .2f, -1}, {.2f, .2f, 1}, a, b, c, 1) &&
              !proper_segment_triangle({1, 1, -1}, {1, 1, 1}, a, b, c, 1),
          "Intersection detector controls distinguish true interior crossing "
          "from miss");
  std::cout << "Actual source, UV, winding, boundary and seam negative "
               "controls passed\n";
}
} // namespace

int main(int argc, char **argv) {
  bool quick = argc > 1 && std::string(argv[1]) == "--quick";
  int failures = 0;
  auto run = [&](std::string name, auto action) {
    try {
      action();
    } catch (const std::exception &e) {
      ++failures;
      std::cerr << name << ": " << e.what() << '\n';
    }
  };
  run("fallback", [] { generic_identity_fallback(); });
  for (float h : heights) {
    if (quick && h != 1.80f)
      continue;
    auto near = make_character_profile(h, CharacterRole::Ghost, "GhostLoop",
                                       CharacterLod::Near),
         far = make_character_profile(h, CharacterRole::Ghost, "GhostLoop",
                                      CharacterLod::Far);
    run("LODs", [&] { check_lods_and_materials(near, far); });
    if (h == 1.80f)
      run("distinct", [&] { check_distinct_identity(near); });
    for (const auto *p : {&near, &far}) {
      const auto &m = p->model;
      auto l = label(m);
      run(l + " geometry", [&] { check_geometry(*p); });
      run(l + " source", [&] { check_anatomical_head(m); });
      run(l + " complete head motion", [&] { check_complete_head_motion(m); });
      run(l + " sockets", [&] { check_socket_fit(m); });
      run(l + " hands", [&] { check_hands(m); });
      run(l + " seat", [&] { check_seat_coverage(m); });
      run(l + " tapes", [&] { check_tape_coverage(m); });
      run(l + " scalp", [&] { check_crop_coverage(m); });
      run(l + " cards", [&] { check_hair_cards(m); });
      run(l + " repeat", [&] {
        auto again =
            make_character_profile(h, CharacterRole::Ghost, "GhostLoop", m.lod);
        require(exact(m, again.model), "Deterministic repeated construction");
      });
      run(l + " animation", [&] { check_animation(m, quick); });
      if (h == 1.80f)
        run(l + " controls", [&] { controls(m); });
    }
  }
  if (failures)
    std::cerr << failures << " GhostLoop test groups failed\n";
  return failures ? 1 : 0;
}
