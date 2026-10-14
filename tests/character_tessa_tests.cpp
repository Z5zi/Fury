#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/tessa_anatomy.hpp"

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
constexpr std::array<float, 3> heights{{1.35f, 1.65f, 2.10f}};
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
  require(profile.individualized && m.role == CharacterRole::Commuter &&
              m.seed == 0xe78507f3u && m.seed == character_seed("NpcCivC"),
          context + "actual Tessa identity/role");
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
    if (s == static_cast<int>(CharacterSurface::HairStrands))
      require(w.first == bone_index(CharacterBone::Head) &&
                  w.first_weight == 1 && w.third_weight == 0,
              where + " strand card remains rigidly attached to the skull");
    if (w.first == bone_index(CharacterBone::LeftFoot))
      soles[0] = std::min(soles[0], v.position.y);
    if (w.first == bone_index(CharacterBone::RightFoot))
      soles[1] = std::min(soles[1], v.position.y);
    bounds.add(v.position);
  }
  // Tessa's wardrobe and groom have an explicit material contract; adding an
  // unrelated atlas region must not require every actor to use that region.
  for (auto surface :
       {CharacterSurface::Face, CharacterSurface::Jacket,
        CharacterSurface::Hair, CharacterSurface::HairCrop,
        CharacterSurface::Skin, CharacterSurface::Shirt,
        CharacterSurface::Pants, CharacterSurface::Leather,
        CharacterSurface::Eyes, CharacterSurface::Rubber,
        CharacterSurface::Metal, CharacterSurface::HairStrands}) {
    const auto i = static_cast<std::size_t>(surface);
    require(surfaces[i] > 0,
            context + "modeled atlas surface " + std::to_string(i));
  }
  const auto card_vertices =
      surfaces[static_cast<std::size_t>(CharacterSurface::HairStrands)];
  require(card_vertices > 0,
          context + "both LODs retain real strand cutout geometry");
  for (bool used : used_bones)
    require(used, context + "every rig joint deforms actual vertices");
  const Vec3 extent = (bounds.high - bounds.low) * (1.f / h);
  require(extent.x > .28f && extent.x < .50f && extent.z > .12f &&
              extent.z < .25f,
          context + "sensible nonzero body, fingers, satchel and shoe extents");
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
            where + " winding agrees with surface normals");
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
  require(exact(a.rig, b.rig) && exact(a.appearance, b.appearance) &&
              a.seed == b.seed && a.dual_quaternion_skinning &&
              b.dual_quaternion_skinning,
          "Near/far Tessa retains bit-identical rig, identity and opt-in "
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
  for (auto surface : {CharacterSurface::Hair, CharacterSurface::HairCrop,
                       CharacterSurface::HairStrands, CharacterSurface::Skin,
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
             tessa_anatomy::head_center * h) < h * 1.e-6f &&
          length(
              model.rig.joints[bone_index(CharacterBone::Neck)].bind_position -
              tessa_anatomy::neck_center * h) < h * 1.e-6f,
      context + "head and neck rig pivots use the morphed source landmarks");
  std::map<std::array<float, 3>, std::uint32_t> source_positions;
  for (std::uint32_t i = 0; i < tessa_anatomy::positions.size(); ++i) {
    const auto p = tessa_anatomy::positions[i] * h;
    source_positions.emplace(std::array<float, 3>{{p.x, p.y, p.z}}, i);
  }
  require(source_positions.size() == tessa_anatomy::positions.size(),
          context + "source geometry has unambiguous canonical positions");
  std::vector<std::uint32_t> canonical(model.bind_mesh.vertices.size(),
                                       UINT32_MAX);
  std::vector<std::uint32_t> first_copy(tessa_anatomy::positions.size(),
                                        UINT32_MAX);
  std::vector<bool> retained(tessa_anatomy::positions.size(), false);
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
    require(source_corner + 2 < tessa_anatomy::uv_indices.size(),
            context + "no added anatomy triangle");
    const std::uint32_t corners[]{a, b, c};
    for (std::size_t corner = 0; corner < 3; ++corner) {
      const auto uv = tessa_anatomy::uv_coordinates
          [tessa_anatomy::uv_indices[source_corner + corner]];
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
      actual_indices.size() == tessa_anatomy::indices.size() &&
          std::equal(actual_indices.begin(), actual_indices.end(),
                     tessa_anatomy::indices.begin()),
      context +
          "all source triangles survive intact and in their original winding");
  for (std::size_t side = 0; side < 2; ++side) {
    require(eye_vertices[side] > 0,
            context + "both source eye sockets contain actual eye geometry");
    const auto center = tessa_anatomy::eye_centers[side] * h;
    const auto helper = tessa_anatomy::eye_helper_half_extents[side] * h;
    const auto &bounds = eye_bounds[side];
    const auto middle = (bounds.low + bounds.high) * .5f;
    require(
        std::fabs(middle.x - center.x) < h * .0005f &&
            std::fabs(middle.y - center.y) < h * .0015f,
        context +
            "eyeballs are centered at the independent morphed eye landmarks");
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

// Trousers are one authored garment. Three independently capped pelvis/leg
// volumes can each pass closed-surface checks while overlapping at the hips.
// Exact coordinate welding also catches tiny cap gaps that rounded seam checks
// deliberately tolerate for older curved surfaces.
void check_coherent_trouser_shell(const CharacterModel &model) {
  const auto &mesh = model.bind_mesh;
  using Key = std::array<float, 3>;
  std::map<Key, std::uint32_t> welded;
  std::vector<std::uint32_t> canonical(mesh.vertices.size(), UINT32_MAX);
  std::vector<CharacterInfluence> influences;
  for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
    const auto &v = mesh.vertices[i];
    if (surface_at(v.uv) != static_cast<int>(CharacterSurface::Pants))
      continue;
    const Key key{{v.position.x, v.position.y, v.position.z}};
    const auto index = static_cast<std::uint32_t>(welded.size());
    const auto found = welded.emplace(key, index);
    canonical[i] = found.first->second;
    if (found.second)
      influences.push_back(model.influences[i]);
    else
      require(exact(influences[canonical[i]], model.influences[i]),
              label(model) + " coincident trouser copies have identical influences");
  }
  std::vector<std::uint32_t> parent(welded.size());
  for (std::uint32_t i = 0; i < parent.size(); ++i)
    parent[i] = i;
  auto root = [&](std::uint32_t x) {
    while (parent[x] != x) {
      parent[x] = parent[parent[x]];
      x = parent[x];
    }
    return x;
  };
  struct Edge { unsigned count{}; int orientation{}; };
  std::map<std::pair<std::uint32_t, std::uint32_t>, Edge> edges;
  std::size_t triangles = 0;
  for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
    if (canonical[mesh.indices[i]] == UINT32_MAX)
      continue;
    ++triangles;
    for (int corner = 0; corner < 3; ++corner) {
      auto a = canonical[mesh.indices[i + corner]];
      auto b = canonical[mesh.indices[i + (corner + 1) % 3]];
      require(a != UINT32_MAX && b != UINT32_MAX && a != b,
              label(model) + " trouser faces have distinct geometric corners");
      parent[root(a)] = root(b);
      const int orientation = a < b ? 1 : -1;
      if (b < a) std::swap(a, b);
      auto &edge = edges[{a, b}];
      ++edge.count; edge.orientation += orientation;
    }
  }
  require(!edges.empty(), "Coherent trousers have real indexed faces");
  for (const auto &edge : edges)
    require(edge.second.count == 2 && edge.second.orientation == 0,
            label(model) + " trousers close exactly with opposite edge winding");
  std::set<std::uint32_t> components;
  for (std::uint32_t i = 0; i < parent.size(); ++i)
    components.insert(root(i));
  require(components.size() == 1,
          label(model) + " trousers form one connected shell, not capped overlaps");
  require(welded.size() + triangles == edges.size() + 2,
          label(model) + " closed waist/two-leg shell has genus zero");
}

// Physical periodic seams preserve tint and deformation continuity even when
// the atlas has a discontinuous U coordinate. Opaque hair sectors additionally
// require smooth normals. Closed cloth caps can deliberately carry hard
// normals.
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
        s == int(CharacterSurface::HairStrands))
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
  for (std::size_t i = 0; i < tessa_anatomy::indices.size(); i += 3) {
    const auto uv = tessa_anatomy::uv_coordinates[tessa_anatomy::uv_indices[i]];
    const unsigned chart = uv.x < .08f ? 2 : uv.x < .16f ? 1 : 0;
    for (int c = 0; c < 3; ++c) {
      auto a = tessa_anatomy::indices[i + c],
           b = tessa_anatomy::indices[i + (c + 1) % 3];
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
      result[side].push_back(tessa_anatomy::positions[i]);
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
          require(delta > -.003f && delta < .002f,
                  label(model) + "tessellated eye lies within 0.3% height "
                                 "behind / 0.2% ahead of actual socket rim");
        }
      }
  }
  std::cout << label(model) << ": actual socket-rim eye depth/h " << nearest_gap
            << " .. " << largest_protrusion << '\n';
}

void check_hair_cards(const CharacterModel &model) {
  const auto &mesh = model.bind_mesh;
  const Vec3 origin = tessa_anatomy::head_center * model.rig.height;
  std::size_t triangles = 0;
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
    const auto &a = mesh.vertices[mesh.indices[t]];
    if (surface_at(a.uv) != int(CharacterSurface::HairStrands))
      continue;
    const auto &b = mesh.vertices[mesh.indices[t + 1]];
    const auto &c = mesh.vertices[mesh.indices[t + 2]];
    const Vec3 outward =
        (a.position + b.position + c.position) * (1.f / 3.f) - origin;
    require_element(dot(cross(b.position - a.position, c.position - a.position),
                        outward) > 0,
                    label(model),
                    " hair card faces away from the actual skull at triangle ",
                    t / 3);
    require_element(dot(a.normal + b.normal + c.normal, outward) > 0,
                    label(model),
                    " hair card shading normals face away from the actual "
                    "skull at triangle ",
                    t / 3);
    ++triangles;
  }
  require(triangles > 0,
          "Cutout material is attached to actual outward-facing hair geometry");
}

// The two rolled leather sheets are open, nonperiodic indexed patches. Closed
// bag/shoe/strap tubes have coincident periodic corners and are excluded by
// topology, not hard-coded creation order or tessellation.
void check_satchel_flap(const CharacterModel &model) {
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
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
    const auto a = mesh.indices[t], b = mesh.indices[t + 1],
               c = mesh.indices[t + 2];
    if (surface_at(mesh.vertices[a].uv) != int(CharacterSurface::Leather))
      continue;
    parent[root(b)] = root(a);
    parent[root(c)] = root(a);
  }
  struct Patch {
    std::vector<std::size_t> vertices, triangles;
    Bounds bounds;
    Vec2 low{2, 2}, high{-1, -1};
    bool repeated{false};
    std::set<std::array<float, 3>> positions;
  };
  std::map<std::size_t, Patch> patches;
  for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
    const auto &v = mesh.vertices[i];
    if (surface_at(v.uv) != int(CharacterSurface::Leather))
      continue;
    auto &p = patches[root(i)];
    p.vertices.push_back(i);
    p.bounds.add(v.position);
    p.low.x = std::min(p.low.x, v.uv.x);
    p.low.y = std::min(p.low.y, v.uv.y);
    p.high.x = std::max(p.high.x, v.uv.x);
    p.high.y = std::max(p.high.y, v.uv.y);
    p.repeated |= !p.positions
                       .insert(std::array<float, 3>{
                           {v.position.x, v.position.y, v.position.z}})
                       .second;
  }
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3)
    if (surface_at(mesh.vertices[mesh.indices[t]].uv) ==
        int(CharacterSurface::Leather))
      patches[root(mesh.indices[t])].triangles.push_back(t);
  const auto lo = character_surface_uv(CharacterSurface::Leather, 0, 0),
             hi = character_surface_uv(CharacterSurface::Leather, 1, 1);
  std::vector<const Patch *> sheets;
  for (const auto &entry : patches) {
    const auto &p = entry.second;
    const auto extent = (p.bounds.high - p.bounds.low) * (1.f / h);
    if (!p.repeated && p.triangles.size() > 20 &&
        std::max(extent.x, extent.z) > .045f && extent.y > .025f &&
        std::min(extent.x, extent.z) > .025f &&
        p.high.x - p.low.x > (hi.x - lo.x) * .9f &&
        p.high.y - p.low.y > (hi.y - lo.y) * .9f)
      sheets.push_back(&p);
  }
  require(
      sheets.size() == 2,
      label(model) +
          " exactly two real open rolled-leather flap sheets are recovered");
  auto brightness = [&](const Patch *p) {
    const auto c = mesh.vertices[p->vertices.front()].color;
    return c.x + c.y + c.z;
  };
  if (brightness(sheets[0]) < brightness(sheets[1]))
    std::swap(sheets[0], sheets[1]);
  const auto &outer = *sheets[0], &inner = *sheets[1];
  require(outer.vertices.size() == inner.vertices.size() &&
              outer.triangles.size() == inner.triangles.size(),
          "Flap layers have corresponding complete sample grids");
  std::map<std::size_t, std::size_t> mates;
  std::set<std::size_t> used;
  const auto uniform_influence = model.influences[outer.vertices.front()];
  for (const auto *p : sheets)
    for (auto i : p->vertices) {
      const auto &w = model.influences[i];
      require(exact(w, uniform_influence) && model.dual_quaternion_skinning,
              "Both entire flap sheets share one uniform rigid DQ transform, "
              "preserving "
              "measured separation through every runtime pose");
    }
  float shading_separation = 1.f;
  for (auto a : outer.vertices) {
    const auto &v = mesh.vertices[a];
    std::size_t match = SIZE_MAX;
    for (auto b : inner.vertices) {
      const auto uv = mesh.vertices[b].uv;
      if (std::fabs(v.uv.x + uv.x - lo.x - hi.x) < 2e-7f &&
          std::fabs(v.uv.y - uv.y) < 2e-7f) {
        require(match == SIZE_MAX,
                "Unique mirrored-U correspondence between actual flap sheets");
        match = b;
      }
    }
    require(match != SIZE_MAX && used.insert(match).second,
            "Every outer flap corner has one real underside corner");
    mates[a] = match;
    const auto delta = v.position - mesh.vertices[match].position;
    require(std::fabs(length(delta) / h - .0011f) < 1e-6f,
            "Actual flap offset/h " + std::to_string(length(delta) / h) +
                " at outer vertex " + std::to_string(a) + " and underside " +
                std::to_string(match) +
                " must be 0.0011h including the rolled top");
    shading_separation =
        std::min({shading_separation, dot(v.normal, delta) / h,
                  -dot(mesh.vertices[match].normal, delta) / h});
  }
  Vec3 historical_z{};
  for (auto t : outer.triangles) {
    const auto &a = mesh.vertices[mesh.indices[t]],
               &b = mesh.vertices[mesh.indices[t + 1]],
               &c = mesh.vertices[mesh.indices[t + 2]];
    if ((a.uv.y + b.uv.y + c.uv.y) / 3 > hi.y - (hi.y - lo.y) * .12f)
      historical_z +=
          normalize(cross(b.position - a.position, c.position - a.position));
  }
  historical_z = normalize(historical_z);
  require(length(historical_z) > .99f,
          "Straight lower flap recovers the historical local-Z offset axis "
          "after any authored bag rotation");
  float minimum = 1.f, legacy = 1.f, legacy_absolute = 1.f;
  for (auto t : outer.triangles) {
    const auto a = mesh.indices[t], b = mesh.indices[t + 1],
               c = mesh.indices[t + 2];
    const auto normal =
        normalize(cross(mesh.vertices[b].position - mesh.vertices[a].position,
                        mesh.vertices[c].position - mesh.vertices[a].position));
    for (auto id : {a, b, c}) {
      const auto delta =
          mesh.vertices[id].position - mesh.vertices[mates.at(id)].position;
      minimum = std::min(minimum, dot(normal, delta) / h);
    }
    // Negative control recreates the former bag-local-Z-only underside offset.
    // Its projection must fail the same physical-thickness requirement near
    // the almost-horizontal top; coplanarity is not accepted as harmless here.
    const float old_gap = dot(normal, historical_z * .0011f);
    legacy = std::min(legacy, old_gap);
    legacy_absolute = std::min(legacy_absolute, std::fabs(old_gap));
  }
  std::cout << label(model) << ": measured flap normal gap/h " << minimum
            << ", shading gap/h " << shading_separation
            << ", oldZ-only signed/absolute control " << legacy << "/"
            << legacy_absolute << '\n';
  require(
      shading_separation > 0.f,
      "Flap shading normals face opposite sides of the real separated layers");
  require(minimum > .0001f, label(model) +
                                " flap outer winding and every corner retain "
                                "positive normal thickness; minimum/h " +
                                std::to_string(minimum));
  if (model.lod == CharacterLod::Near)
    require(legacy_absolute < .0001f,
            "Old local-Z underside fails the same thickness gate at actual "
            "rolled-top triangles in the near mesh");
  require(legacy < .0001f, "The old Z-only underside negative control must "
                           "fail the real normal-thickness gate");
  for (auto a : outer.triangles)
    for (auto b : inner.triangles) {
      Vec3 p[3], q[3];
      Bounds x, y;
      for (int c = 0; c < 3; ++c) {
        p[c] = mesh.vertices[mesh.indices[a + c]].position;
        q[c] = mesh.vertices[mesh.indices[b + c]].position;
        x.add(p[c]);
        y.add(q[c]);
      }
      if (x.high.x < y.low.x || y.high.x < x.low.x || x.high.y < y.low.y ||
          y.high.y < x.low.y || x.high.z < y.low.z || y.high.z < x.low.z)
        continue;
      for (int e = 0; e < 3; ++e)
        require(!proper_segment_triangle(p[e], p[(e + 1) % 3], q[0], q[1], q[2],
                                         h) &&
                    !proper_segment_triangle(q[e], q[(e + 1) % 3], p[0], p[1],
                                             p[2], h),
                label(model) + " the real rolled outer/underside flap "
                               "triangles do not intersect");
    }
}

struct WardrobeCrossings {
  struct Triangle {
    std::size_t offset;
    int region;
    float bind_high_y;
  };
  const CharacterModel &model;
  std::vector<Triangle> triangles;
  explicit WardrobeCrossings(const CharacterModel &source) : model(source) {
    const auto &m = source.bind_mesh;
    // Recover separately authored wardrobe components without relying on their
    // generation order, resolution, triangle numbers or current bag position.
    std::vector<std::size_t> parent(m.vertices.size());
    for (std::size_t i = 0; i < parent.size(); ++i)
      parent[i] = i;
    auto root = [&](std::size_t i) {
      while (i != parent[i]) {
        parent[i] = parent[parent[i]];
        i = parent[i];
      }
      return i;
    };
    auto join = [&](std::size_t a, std::size_t b) {
      parent[root(b)] = root(a);
    };
    std::map<std::pair<int, std::array<float, 3>>, std::size_t> welded;
    for (std::size_t i = 0; i < m.vertices.size(); ++i) {
      const auto &v = m.vertices[i];
      const auto added = welded.emplace(
          std::make_pair(
              surface_at(v.uv),
              std::array<float, 3>{{v.position.x, v.position.y, v.position.z}}),
          i);
      if (!added.second)
        join(i, added.first->second);
    }
    for (std::size_t i = 0; i < m.indices.size(); i += 3) {
      join(m.indices[i], m.indices[i + 1]);
      join(m.indices[i], m.indices[i + 2]);
    }
    std::vector<bool> torso_component(m.vertices.size(), true);
    std::vector<float> component_top(m.vertices.size(), -1.f);
    for (std::size_t i = 0; i < m.vertices.size(); ++i) {
      const auto &w = source.influences[i];
      const auto r = root(i);
      torso_component[r] =
          torso_component[r] &&
          (w.first_weight == 0 ||
           w.first <= bone_index(CharacterBone::Chest)) &&
          (1 - w.first_weight - w.third_weight == 0 ||
           w.second <= bone_index(CharacterBone::Chest)) &&
          (w.third_weight == 0 || w.third <= bone_index(CharacterBone::Chest));
      component_top[r] = std::max(component_top[r],
                                  m.vertices[i].position.y / source.rig.height);
    }
    auto arm = [](std::uint8_t bone) {
      return bone >= bone_index(CharacterBone::LeftUpperArm) &&
             bone <= bone_index(CharacterBone::RightHand);
    };
    for (std::size_t i = 0; i < m.indices.size(); i += 3) {
      const auto a = m.indices[i], b = m.indices[i + 1], c = m.indices[i + 2];
      const auto center = (m.vertices[a].position + m.vertices[b].position +
                           m.vertices[c].position) *
                          (1.f / (3 * source.rig.height));
      const auto s = surface_at(m.vertices[a].uv);
      int region = -1;
      if (s == int(CharacterSurface::Jacket) && center.y < .38f &&
          center.y > -.06f) {
        bool is_arm = false;
        for (auto id : {a, b, c}) {
          const auto &w = source.influences[id];
          is_arm |=
              (arm(w.first) && w.first_weight > .01f) ||
              (arm(w.second) && 1 - w.first_weight - w.third_weight > .01f) ||
              (arm(w.third) && w.third_weight > .01f);
        }
        region = is_arm ? (center.x < 0 ? 1 : 2) : 0;
      } else if (s == int(CharacterSurface::Skin) && center.y < .03f) {
        region = center.x < 0 ? 1 : 2;
      } else if (s == int(CharacterSurface::Leather) ||
                 s == int(CharacterSurface::Metal)) {
        if (torso_component[root(a)] && component_top[root(a)] < .28f)
          region = 3; // Bag, flap and hardware.
        else if (s == int(CharacterSurface::Leather) && center.y > -.15f)
          region = 6; // Strap.
      } else if (s == int(CharacterSurface::Pants) && center.y < 0 &&
                 center.y > -.40f) {
        bool leg = false;
        for (auto id : {a, b, c}) {
          const auto &w = source.influences[id];
          leg |= (w.first >= bone_index(CharacterBone::LeftThigh) &&
                  w.first_weight > .01f) ||
                 (w.second >= bone_index(CharacterBone::LeftThigh) &&
                  1 - w.first_weight - w.third_weight > .01f);
        }
        if (leg)
          region = center.x < 0 ? 4 : 5;
      }
      // Sewn upper sleeves are real garment faces; no armhole-height waiver.
      const float high_y =
          std::max({m.vertices[a].position.y, m.vertices[b].position.y,
                    m.vertices[c].position.y}) /
          source.rig.height;
      if (region >= 0)
        triangles.push_back({i, region, high_y});
    }
    std::array<std::size_t, 7> groups{};
    for (auto t : triangles)
      ++groups[t.region];
    for (auto n : groups)
      require(n > 0, "Wardrobe audit includes coat, both arms/hands, satchel, "
                     "both thighs and strap");
  }
  void check(const Mesh &mesh, const std::string &context) const {
    struct Item {
      std::size_t triangle;
      Bounds bounds;
    };
    std::vector<Item> sweep;
    sweep.reserve(triangles.size());
    for (std::size_t i = 0; i < triangles.size(); ++i) {
      Item item{i, {}};
      for (int c = 0; c < 3; ++c)
        item.bounds.add(
            mesh.vertices[mesh.indices[triangles[i].offset + c]].position);
      sweep.push_back(item);
    }
    std::sort(sweep.begin(), sweep.end(), [](const Item &a, const Item &b) {
      return a.bounds.low.x < b.bounds.low.x;
    });
    for (std::size_t a = 0; a < sweep.size(); ++a) {
      const auto &first = sweep[a];
      const auto ta = triangles[first.triangle];
      for (std::size_t b = a + 1;
           b < sweep.size() && sweep[b].bounds.low.x <= first.bounds.high.x;
           ++b) {
        const auto &second = sweep[b];
        const auto tb = triangles[second.triangle];
        const bool thigh = ta.region == 4 || ta.region == 5 || tb.region == 4 ||
                           tb.region == 5;
        if (thigh && ta.region != 3 && tb.region != 3 && ta.region != 6 &&
            tb.region != 6)
          continue;
        // Retain the preexisting strap/bag exclusion in this regression only.
        // The independent severity audit records changed adjacent-band contact;
        // this exclusion does not establish exact or harmless bag contact.
        // Strap/coat intersections, including the refitted shoulder, are tested.
        if ((ta.region == 6 && tb.region == 3) ||
            (tb.region == 6 && ta.region == 3))
          continue;
        if (ta.region == tb.region ||
            first.bounds.high.y < second.bounds.low.y ||
            second.bounds.high.y < first.bounds.low.y ||
            first.bounds.high.z < second.bounds.low.z ||
            second.bounds.high.z < first.bounds.low.z)
          continue;
        Vec3 p[3], q[3];
        for (int i = 0; i < 3; ++i) {
          p[i] = mesh.vertices[mesh.indices[ta.offset + i]].position;
          q[i] = mesh.vertices[mesh.indices[tb.offset + i]].position;
        }
        bool adjacent = false;
        for (auto x : p)
          for (auto y : q)
            adjacent |= length(x - y) < model.rig.height * 1e-6f;
        if (adjacent)
          continue;
        for (int e = 0; e < 3; ++e)
          if (proper_segment_triangle(p[e], p[(e + 1) % 3], q[0], q[1], q[2],
                                      model.rig.height) ||
              proper_segment_triangle(q[e], q[(e + 1) % 3], p[0], p[1], p[2],
                                      model.rig.height)) {
            const auto pc =
                (p[0] + p[1] + p[2]) * (1.f / (3 * model.rig.height));
            const auto qc =
                (q[0] + q[1] + q[2]) * (1.f / (3 * model.rig.height));
            throw std::runtime_error(
                context + "wardrobe crossing regions " +
                std::to_string(ta.region) + "/" + std::to_string(tb.region) +
                " triangles " + std::to_string(ta.offset / 3) + "/" +
                std::to_string(tb.offset / 3) + " at " + std::to_string(pc.x) +
                "," + std::to_string(pc.y) + "," + std::to_string(pc.z) +
                " and " + std::to_string(qc.x) + "," + std::to_string(qc.y) +
                "," + std::to_string(qc.z));
          }
      }
    }
  }
};

// Fit the actual lower anatomical head to the actual coat, including coarse
// triangles whose centers lie below the shoulder. A fit at procedural sample
// points alone can miss skin between those points.
struct ShoulderCheck {
  const CharacterModel &model;
  std::vector<std::size_t> skin, coat, shell;
  std::vector<std::pair<std::size_t, std::size_t>> seam_copies;
  explicit ShoulderCheck(const CharacterModel &source) : model(source) {
    const auto &mesh = model.bind_mesh;
    const float h = model.rig.height;
    // Membership is independent of skinning weights and centroid cutoffs.
    // Keep all 8,912 anatomy faces and every Jacket face. Per-pose bounds only
    // accelerate triangle tests; they cannot exclude a deforming neck/arm face.
    for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
      const int surface = surface_at(mesh.vertices[mesh.indices[t]].uv);
      if (surface == int(CharacterSurface::Face)) skin.push_back(t);
      if (surface == int(CharacterSurface::Jacket)) coat.push_back(t);
    }
    require(skin.size() == tessa_anatomy::indices.size() / 3 &&
                skin.size() == 8912 && !coat.empty(),
            "Complete unchanged anatomy and all Jacket faces are included");

    std::array<bool, 2> covered_sides{};
    for (const auto &vertex : mesh.vertices) {
      const auto p = vertex.position;
      if (surface_at(vertex.uv) != int(CharacterSurface::Face) ||
          p.y / h > .331f || std::fabs(p.x) / h < .055f)
        continue;
      float top = -std::numeric_limits<float>::infinity();
      for (auto t : coat) {
        const auto a = mesh.vertices[mesh.indices[t]].position;
        const auto b = mesh.vertices[mesh.indices[t + 1]].position;
        const auto c = mesh.vertices[mesh.indices[t + 2]].position;
        const float d = (b.x - a.x) * (c.z - a.z) - (b.z - a.z) * (c.x - a.x);
        if (std::fabs(d) < h * h * 1e-12f)
          continue;
        const float u =
            ((p.x - a.x) * (c.z - a.z) - (p.z - a.z) * (c.x - a.x)) / d;
        const float v =
            ((b.x - a.x) * (p.z - a.z) - (b.z - a.z) * (p.x - a.x)) / d;
        if (u >= -1e-5f && v >= -1e-5f && u + v <= 1.00001f)
          top = std::max(top, a.y + u * (b.y - a.y) + v * (c.y - a.y));
      }
      require(std::isfinite(top) && top >= p.y - h * 1e-5f,
              label(model) +
                  " actual coat covers the anatomical shoulder flange at " +
                  std::to_string(p.x / h) + "," + std::to_string(p.y / h) +
                  "," + std::to_string(p.z / h));
      covered_sides[p.x > 0 ? 1 : 0] = true;
    }
    require(covered_sides[0] && covered_sides[1],
            "Both shoulder flange boundaries have coverage probes");

    // Recover the sewn core by exact physical welding and its full extents,
    // rather than requiring two independently authored torso/yoke patches.
    // The neckline, torso, and both sleeves must be one oriented manifold.
    // The ornamental hem band intentionally shares positions with the hem.
    // Recover structural indexed pieces before physical welding, so a touching
    // trim is not mistaken for an extra sheet of the core. These geometric
    // roles also accept a truly index-shared single core; no body/yoke split is
    // required. Collision membership above still includes every Jacket face.
    std::vector<std::uint32_t> indexed_parent(mesh.vertices.size());
    for (std::uint32_t i = 0; i < indexed_parent.size(); ++i) indexed_parent[i] = i;
    auto indexed_root = [&](std::uint32_t i) {
      while (i != indexed_parent[i]) {
        indexed_parent[i] = indexed_parent[indexed_parent[i]];
        i = indexed_parent[i];
      }
      return i;
    };
    for (auto t : coat) {
      const auto a = mesh.indices[t];
      indexed_parent[indexed_root(mesh.indices[t + 1])] = indexed_root(a);
      indexed_parent[indexed_root(mesh.indices[t + 2])] = indexed_root(a);
    }
    std::map<std::uint32_t, Bounds> indexed_bounds;
    for (auto t : coat)
      for (int c = 0; c < 3; ++c) {
        const auto i = mesh.indices[t + c];
        indexed_bounds[indexed_root(i)].add(mesh.vertices[i].position * (1.f / h));
      }
    std::set<std::uint32_t> structural_pieces;
    for (const auto &entry : indexed_bounds) {
      const auto &b = entry.second;
      const auto span = b.high - b.low;
      const bool torso_or_sleeve = span.y > .20f && b.high.y > .29f &&
          (span.x > .17f || b.low.x > .07f || b.high.x < -.07f);
      const bool shoulder = b.low.y > .29f && b.high.y > .34f && span.x > .15f;
      const bool neckline = b.low.y > .33f && b.high.y > .345f &&
          b.low.x < 0 && b.high.x > 0 &&
          b.low.z < tessa_anatomy::neck_center.z && b.high.z > tessa_anatomy::neck_center.z;
      const bool preserved_sleeve_cap =
          std::fabs(b.low.y + .042f) < 1e-6f &&
          std::fabs(b.high.y + .042f) < 1e-6f &&
          (b.low.x > .07f || b.high.x < -.07f);
      if (torso_or_sleeve || shoulder || neckline || preserved_sleeve_cap)
        structural_pieces.insert(entry.first);
    }
    std::vector<std::size_t> structural_faces;
    for (auto t : coat)
      if (structural_pieces.count(indexed_root(mesh.indices[t])))
        structural_faces.push_back(t);
    using Key = std::array<float, 3>;
    std::map<Key, std::uint32_t> welded;
    std::vector<std::uint32_t> canonical(mesh.vertices.size(), UINT32_MAX);
    std::vector<std::vector<std::size_t>> copies;
    std::vector<bool> used(mesh.vertices.size());
    for (auto t : structural_faces)
      for (int c = 0; c < 3; ++c) used[mesh.indices[t + c]] = true;
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
      if (!used[i]) continue;
      const auto q = mesh.vertices[i].position;
      const auto inserted = welded.emplace(Key{{q.x, q.y, q.z}}, welded.size());
      canonical[i] = inserted.first->second;
      if (inserted.second) copies.emplace_back();
      copies[canonical[i]].push_back(i);
    }
    std::vector<std::uint32_t> parent(welded.size());
    for (std::uint32_t i = 0; i < parent.size(); ++i) parent[i] = i;
    auto root = [&](std::uint32_t i) {
      while (i != parent[i]) { parent[i] = parent[parent[i]]; i = parent[i]; }
      return i;
    };
    for (auto t : structural_faces) {
      const auto a = canonical[mesh.indices[t]];
      parent[root(canonical[mesh.indices[t + 1]])] = root(a);
      parent[root(canonical[mesh.indices[t + 2]])] = root(a);
    }
    std::map<std::uint32_t, Bounds> components;
    for (std::uint32_t i = 0; i < copies.size(); ++i)
      components[root(i)].add(mesh.vertices[copies[i].front()].position);
    std::uint32_t core = UINT32_MAX;
    for (const auto &entry : components) {
      const auto &bounds = entry.second;
      if (bounds.low.x < -.13f * h && bounds.high.x > .13f * h &&
          bounds.low.y < -.035f * h && bounds.high.y > .34f * h) {
        require(core == UINT32_MAX, "Unique complete sewn coat core");
        core = entry.first;
      }
    }
    require(core != UINT32_MAX,
            "Neckline, torso and both full sleeves form one sewn component");
    struct Edge { unsigned count{}; int orientation{}; };
    std::map<std::pair<std::uint32_t, std::uint32_t>, Edge> edges;
    std::set<std::uint32_t> core_vertices;
    for (auto t : structural_faces) {
      if (root(canonical[mesh.indices[t]]) != core) continue;
      shell.push_back(t);
      for (int c = 0; c < 3; ++c) {
        auto a = canonical[mesh.indices[t + c]],
             b = canonical[mesh.indices[t + (c + 1) % 3]];
        require(a != b, "Sewn coat keeps distinct physical face corners");
        core_vertices.insert(a);
        const int orientation = a < b ? 1 : -1;
        if (b < a) std::swap(a, b);
        auto &edge = edges[{a, b}];
        ++edge.count; edge.orientation += orientation;
      }
    }
    for (auto id : core_vertices) {
      const auto &group = copies[id];
      const auto a = group.front();
      for (std::size_t j = 1; j < group.size(); ++j) {
        const auto b = group[j];
        require(exact(model.influences[a], model.influences[b]),
                "Every sewn core seam copy has identical influences");
        if (mesh.vertices[a].position.y > .15f * h)
          require(length(mesh.vertices[a].normal - mesh.vertices[b].normal) < 1e-5f,
                  label(model) + " upper sewn core seam normals differ at vertices " +
                      std::to_string(a) + "/" + std::to_string(b) +
                      " by " + std::to_string(length(mesh.vertices[a].normal -
                                                    mesh.vertices[b].normal)) +
                      " at normalized " + std::to_string(mesh.vertices[a].position.x/h) +
                      "," + std::to_string(mesh.vertices[a].position.y/h) +
                      "," + std::to_string(mesh.vertices[a].position.z/h));
        seam_copies.emplace_back(a, b);
      }
    }
    std::map<std::uint32_t, std::vector<std::uint32_t>> boundary;
    for (const auto &entry : edges) {
      const auto &edge = entry.second;
      require((edge.count == 2 && edge.orientation == 0) || edge.count == 1,
              "Sewn core has opposite two-face interior edges, no caps at armholes");
      if (edge.count == 1) {
        boundary[entry.first.first].push_back(entry.first.second);
        boundary[entry.first.second].push_back(entry.first.first);
      }
    }
    for (const auto &entry : boundary)
      require(entry.second.size() == 2,
              "Neckline and hem are closed unbranched boundary loops");
    std::set<std::uint32_t> visited;
    unsigned loops = 0, neckline = 0, hem = 0;
    for (const auto &entry : boundary) {
      if (visited.count(entry.first)) continue;
      ++loops;
      Bounds bounds;
      std::vector<std::uint32_t> pending{entry.first};
      std::size_t count = 0;
      while (!pending.empty()) {
        const auto id = pending.back(); pending.pop_back();
        if (!visited.insert(id).second) continue;
        ++count;
        bounds.add(mesh.vertices[copies[id].front()].position);
        for (auto neighbor : boundary.at(id)) pending.push_back(neighbor);
      }
      require(count > 10, "Each true coat opening is a sampled loop");
      if (bounds.low.y > .34f * h && bounds.high.y < .38f * h &&
          bounds.low.x < 0 && bounds.high.x > 0 &&
          bounds.low.z < tessa_anatomy::neck_center.z * h &&
          bounds.high.z > tessa_anatomy::neck_center.z * h)
        ++neckline;
      if (std::fabs(bounds.low.y / h - .010f) < 1e-6f &&
          std::fabs(bounds.high.y / h - .010f) < 1e-6f)
        ++hem;
    }
    require(loops == 2 && neckline == 1 && hem == 1,
            "Only the neckline and preserved hem stay open; both armholes are sewn");
    require(core_vertices.size() + shell.size() == edges.size(),
            "One genus-zero coat core with two intentional openings");
    std::cout << label(model) << ": sewn core " << shell.size()
              << " faces, two boundary loops, " << seam_copies.size()
              << " exact seam-copy pairs; full skin " << skin.size()
              << " faces / all Jacket " << coat.size() << " faces\n";
  }
  void check(const Mesh &mesh, const std::string &context) const {
    for (const auto &pair : seam_copies)
      require(length(mesh.vertices[pair.first].position -
                     mesh.vertices[pair.second].position) < model.rig.height * 1e-7f,
              context + "sewn neckline/armhole seam remains closed after skinning");

    struct Triangle {
      std::size_t offset;
      std::array<Vec3, 3> p;
      Bounds bounds;
    };
    auto gather = [&](const std::vector<std::size_t> &offsets) {
      std::vector<Triangle> result;
      result.reserve(offsets.size());
      for (auto offset : offsets) {
        Triangle triangle{offset, {}, {}};
        for (int c = 0; c < 3; ++c) {
          triangle.p[c] = mesh.vertices[mesh.indices[offset + c]].position;
          triangle.bounds.add(triangle.p[c]);
        }
        result.push_back(triangle);
      }
      return result;
    };
    const auto a = gather(skin);
    auto b = gather(coat);
    std::sort(b.begin(), b.end(), [](const Triangle &x, const Triangle &y) {
      return x.bounds.low.x < y.bounds.low.x;
    });
    Bounds coat_bounds;
    for (const auto &t : b) { coat_bounds.add(t.bounds.low); coat_bounds.add(t.bounds.high); }
    for (const auto &first : a) {
      const auto &x = first.bounds;
      if (x.high.x < coat_bounds.low.x || coat_bounds.high.x < x.low.x ||
          x.high.y < coat_bounds.low.y || coat_bounds.high.y < x.low.y ||
          x.high.z < coat_bounds.low.z || coat_bounds.high.z < x.low.z)
        continue;
      for (const auto &second : b) {
        const auto &y = second.bounds;
        if (y.low.x > x.high.x) break;
        if (y.high.x < x.low.x || x.high.y < y.low.y ||
            y.high.y < x.low.y || x.high.z < y.low.z || y.high.z < x.low.z)
          continue;
        for (int edge = 0; edge < 3; ++edge)
          if (proper_segment_triangle(first.p[edge], first.p[(edge + 1) % 3],
                                      second.p[0], second.p[1], second.p[2],
                                      model.rig.height) ||
              proper_segment_triangle(second.p[edge], second.p[(edge + 1) % 3],
                                      first.p[0], first.p[1], first.p[2],
                                      model.rig.height))
            throw std::runtime_error(
                context +
                " anatomical shoulder crosses coat between triangles " +
                std::to_string(first.offset / 3) + " and " +
                std::to_string(second.offset / 3));
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
  const WardrobeCrossings wardrobe(model);
  const ShoulderCheck shoulders(model);
  shoulders.check(bind, label(model) + " bind pose: ");
  wardrobe.check(bind, label(model) + " bind pose: ");
  knees.check(bind, label(model) + " bind pose: ");
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
      knees.check(output, context);
      wardrobe.check(output, context);
      shoulders.check(output, context);
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
        shoulders.check(output, label(model) + " head turn yaw " +
                                    std::to_string(yaw) + " pitch " +
                                    std::to_string(pitch) + " frame " +
                                    std::to_string(frame) + ": ");
      }
  require(exact(bind, model.bind_mesh),
          "Animation never mutates the immutable bind geometry");
  std::cout << label(model) << ": " << poses
            << " actual skinned idle/walk/run/talk/crouch/crouch-walk poses\n";
}

void check_distinct_identity(const CharacterProfile &tessa) {
  const auto &m = tessa.model;
  for (auto id : {"NpcCivA", "NpcCivB"}) {
    const auto role = std::string_view(id) == "NpcCivB"
                          ? CharacterRole::Market
                          : CharacterRole::Commuter;
    const auto other = make_character_profile(m.rig.height, role, id, m.lod);
    require(
        other.individualized && other.model.seed != m.seed &&
            !exact(other.model.appearance, m.appearance) &&
            !exact(other.model.rig, m.rig),
        "Tessa retains her own stable identity, appearance and anatomical rig");
    Bounds a, b;
    for (const auto &v : m.bind_mesh.vertices)
      if (surface_at(v.uv) == int(CharacterSurface::Face))
        a.add(v.position);
    for (const auto &v : other.model.bind_mesh.vertices)
      if (surface_at(v.uv) == int(CharacterSurface::Face))
        b.add(v.position);
    require(length((a.high - a.low) - (b.high - b.low)) > m.rig.height * .002f,
            "Tessa has independently morphed anatomy, not merely a new hair or "
            "cloth tint");
  }
  const auto generic =
      make_character_model(m.rig.height, m.role, m.seed, m.lod);
  require(!exact(m.bind_mesh, generic.bind_mesh) && !exact(m.rig, generic.rig),
          "Registered Tessa profile replaces the generic body and rig");
}

void generic_identity_fallback() {
  struct Identity {
    const char *id;
    CharacterRole role;
  };
  using R = CharacterRole;
  constexpr Identity roster[]{{"unregistered_hale_security", R::Security},
                              {"unregistered_bank_staff", R::BankStaff},
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
                              {"npccivc", R::Commuter},
                              {"NpcCivC_extra", R::Commuter},
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
        if (static_cast<R>(r) != R::Commuter)
          check(h, static_cast<R>(r), "NpcCivC", lod);
      check(h, R::Commuter, std::string_view("NpcCivC\0extra", 13), lod);
    }
  std::cout << cases
            << " bit-exact generic identity/role/height/LOD fallback cases\n";
}
void input_validation() {
  for (auto id : {"NpcCivC", "unknown_identity"})
    for (auto lod : {CharacterLod::Near, CharacterLod::Far})
      for (float height : {0.f, -1.f, std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::infinity(),
                           -std::numeric_limits<float>::infinity()}) {
        bool threw = false;
        try {
          make_character_profile(height, CharacterRole::Commuter, id, lod);
        } catch (const std::invalid_argument &) {
          threw = true;
        }
        require(
            threw,
            "Invalid height is rejected for individual and generic identities");
      }
  for (auto id : {"NpcCivC", "unknown_identity"}) {
    bool bad_role = false, bad_lod = false;
    try {
      make_character_profile(1.65f, CharacterRole::Count, id,
                             CharacterLod::Near);
    } catch (const std::invalid_argument &) {
      bad_role = true;
    }
    try {
      make_character_profile(1.65f, CharacterRole::Commuter, id,
                             static_cast<CharacterLod>(255));
    } catch (const std::invalid_argument &) {
      bad_lod = true;
    }
    require(bad_role && bad_lod, "Invalid role and LOD fail explicitly");
  }
}
} // namespace

int main() {
  try {
    input_validation();
    generic_identity_fallback();
    for (float height : heights) {
      const auto near = make_character_profile(height, CharacterRole::Commuter,
                                               "NpcCivC", CharacterLod::Near);
      const auto far = make_character_profile(height, CharacterRole::Commuter,
                                              "NpcCivC", CharacterLod::Far);
      check_geometry(near);
      check_geometry(far);
      check_socket_fit(near.model);
      check_socket_fit(far.model);
      check_hair_cards(near.model);
      check_hair_cards(far.model);
      check_satchel_flap(near.model);
      check_satchel_flap(far.model);
      check_coherent_trouser_shell(near.model);
      check_coherent_trouser_shell(far.model);
      check_periodic_seam_copies(near.model);
      check_periodic_seam_copies(far.model);
      for (auto surface : {CharacterSurface::Pants, CharacterSurface::Skin,
                           CharacterSurface::Rubber, CharacterSurface::Metal}) {
        check_closed_surface_seams(near.model, surface);
        check_closed_surface_seams(far.model, surface);
      }
      check_lods_and_materials(near, far);
      if (height == 1.65f)
        check_distinct_identity(near);
      check_anatomical_head(near.model);
      check_anatomical_head(far.model);
      for (const auto *profile : {&near, &far}) {
        const auto repeated = make_character_profile(
            height, CharacterRole::Commuter, "NpcCivC", profile->model.lod);
        require(exact(profile->model, repeated.model) &&
                    profile->material.textures == repeated.material.textures &&
                    exact(profile->model.appearance,
                          character_appearance(character_seed("NpcCivC"))),
                "Tessa generation is bit-deterministic and retains her seeded "
                "identity and immutable textures");
      }
      check_animation(near.model);
      check_animation(far.model);
    }
    std::cout << "Tessa character geometry and deformation checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    count_allocations = false;
    std::cerr << "Tessa character geometry and deformation check failed: "
              << e.what() << '\n';
    return 1;
  }
}
