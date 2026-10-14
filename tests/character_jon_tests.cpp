#include "fury/character_profile.hpp"
#include "fury/character_animation.hpp"
#include "fury/character_surface.hpp"
#include "fury/generated/jon_anatomy.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <iterator>
#include <set>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace fury;
constexpr std::array<float, 3> heights{{1.35f, 1.70f, 2.10f}};
constexpr std::size_t surface_count = static_cast<std::size_t>(CharacterSurface::Count);

void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}
// Dense-asset validation visits millions of vertices; construct diagnostic
// strings only for a failed element so sanitizer runs stay inexpensive.
void require_element(bool condition, const std::string& context, const char* message,
                     std::size_t element) {
  if (!condition) throw std::runtime_error(context + message + std::to_string(element));
}
bool finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
bool exact(float a, float b) { return std::memcmp(&a, &b, sizeof(a)) == 0; }
bool exact(Vec3 a, Vec3 b) { return exact(a.x, b.x) && exact(a.y, b.y) && exact(a.z, b.z); }
bool exact(const CharacterInfluence& a, const CharacterInfluence& b) {
  return a.first == b.first && a.second == b.second && a.third == b.third &&
         exact(a.first_weight, b.first_weight) && exact(a.third_weight, b.third_weight);
}
bool exact(const Vertex& a, const Vertex& b) {
  return exact(a.position, b.position) && exact(a.normal, b.normal) && exact(a.color, b.color) &&
         exact(a.uv.x, b.uv.x) && exact(a.uv.y, b.uv.y) && exact(a.opacity, b.opacity);
}
bool exact(const Mesh& a, const Mesh& b) {
  if (a.indices != b.indices || a.vertices.size() != b.vertices.size()) return false;
  for (std::size_t i = 0; i < a.vertices.size(); ++i)
    if (!exact(a.vertices[i], b.vertices[i])) return false;
  return true;
}
bool exact(const CharacterRig& a, const CharacterRig& b) {
  if (!exact(a.height, b.height)) return false;
  for (std::size_t i = 0; i < character_bone_count; ++i)
    if (a.joints[i].parent != b.joints[i].parent ||
        !exact(a.joints[i].bind_position, b.joints[i].bind_position)) return false;
  return true;
}
bool exact(const CharacterAppearance& a, const CharacterAppearance& b) {
  return exact(a.skin, b.skin) && exact(a.hair, b.hair) && exact(a.eyes, b.eyes) &&
         exact(a.shirt, b.shirt) && exact(a.trousers, b.trousers) && exact(a.shoes, b.shoes) &&
         exact(a.accent, b.accent) && exact(a.shoulder_scale, b.shoulder_scale) &&
         exact(a.waist_scale, b.waist_scale) && exact(a.face_scale, b.face_scale) &&
         a.hair_style == b.hair_style && a.face_style == b.face_style;
}
bool exact(const CharacterModel& a, const CharacterModel& b) {
  if (!exact(a.bind_mesh, b.bind_mesh) || !exact(a.rig, b.rig) ||
      !exact(a.appearance, b.appearance) || a.seed != b.seed || a.role != b.role ||
      a.lod != b.lod || a.dual_quaternion_skinning != b.dual_quaternion_skinning ||
      a.influences.size() != b.influences.size()) return false;
  for (std::size_t i = 0; i < a.influences.size(); ++i)
    if (!exact(a.influences[i], b.influences[i])) return false;
  return true;
}
struct Bounds {
  Vec3 low{1000, 1000, 1000}, high{-1000, -1000, -1000};
  void add(Vec3 p) {
    low = {std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
    high = {std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z)};
  }
};
int surface_at(Vec2 uv) {
  if (!std::isfinite(uv.x) || !std::isfinite(uv.y)) return -1;
  for (std::size_t i = 0; i < surface_count; ++i) {
    const auto s = static_cast<CharacterSurface>(i);
    const auto lo = character_surface_uv(s, 0, 0), hi = character_surface_uv(s, 1, 1);
    if (uv.x >= lo.x - 1.e-7f && uv.x <= hi.x + 1.e-7f &&
        uv.y >= lo.y - 1.e-7f && uv.y <= hi.y + 1.e-7f) return static_cast<int>(i);
  }
  return -1;
}
std::string label(const CharacterModel& m) {
  return std::string(m.lod == CharacterLod::Near ? "near" : "far") +
         " height " + std::to_string(m.rig.height);
}

void check_geometry(const CharacterProfile& profile) {
  const auto& m = profile.model;
  const auto& mesh = m.bind_mesh;
  const float h = m.rig.height;
  const auto context = label(m) + ": ";
  require(profile.individualized && m.role == CharacterRole::Market &&
              m.seed == character_seed("NpcCivB"), context + "actual Jon identity/role");
  require(!mesh.vertices.empty() && !mesh.indices.empty() && mesh.indices.size() % 3 == 0,
          context + "nonempty indexed triangles");
  require(m.influences.size() == mesh.vertices.size(), context + "one influence per vertex");
  Bounds bounds;
  std::array<std::size_t, surface_count> surfaces{};
  std::array<bool, character_bone_count> used_bones{};
  std::array<float, 2> soles{{h, h}};
  std::vector<int> vertex_surfaces;
  vertex_surfaces.reserve(mesh.vertices.size());
  for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
    const auto& v = mesh.vertices[i];
    const auto& w = m.influences[i];
    const auto where = context + "vertex " + std::to_string(i);
    require(finite(v.position) && finite(v.normal) && finite(v.color), where + " is finite");
    require(std::fabs(length(v.normal) - 1.f) < .0002f, where + " has a unit normal");
    require(v.color.x >= 0 && v.color.y >= 0 && v.color.z >= 0 &&
                v.color.x <= 1 && v.color.y <= 1 && v.color.z <= 1 && exact(v.opacity, 1.f),
            where + " has bounded opaque material values");
    const int s = surface_at(v.uv);
    require(s >= 0, where + " UV is inside an atlas content region, beyond its gutter");
    vertex_surfaces.push_back(s);
    ++surfaces[static_cast<std::size_t>(s)];
    require(w.first < character_bone_count && w.second < character_bone_count &&
                w.third < character_bone_count && std::isfinite(w.first_weight) &&
                std::isfinite(w.third_weight) && w.first_weight >= 0 && w.third_weight >= 0 &&
                w.first_weight + w.third_weight <= 1.f,
            where + " has valid normalized bone weights");
    if (w.first_weight > 0) used_bones[w.first] = true;
    if (w.first_weight + w.third_weight < 1) used_bones[w.second] = true;
    if (w.third_weight > 0) used_bones[w.third] = true;
    if (s == static_cast<int>(CharacterSurface::HairStrands))
      require(w.first == bone_index(CharacterBone::Head) && w.first_weight == 1 && w.third_weight == 0,
              where + " strand card remains rigidly attached to the skull");
    if (w.first == bone_index(CharacterBone::LeftFoot)) soles[0] = std::min(soles[0], v.position.y);
    if (w.first == bone_index(CharacterBone::RightFoot)) soles[1] = std::min(soles[1], v.position.y);
    bounds.add(v.position);
  }
  // Jon has cloth straps/buttons and no metal hardware. Require his nine
  // base materials, not every region later appended to the shared atlas.
  for (auto surface : {CharacterSurface::Face, CharacterSurface::Jacket,
                       CharacterSurface::Hair, CharacterSurface::Skin,
                       CharacterSurface::Shirt, CharacterSurface::Pants,
                       CharacterSurface::Leather, CharacterSurface::Eyes,
                       CharacterSurface::Rubber}) {
    const auto i = static_cast<std::size_t>(surface);
    require(surfaces[i] > 0, context + "modeled atlas surface " + std::to_string(i));
  }
  const auto card_vertices = surfaces[static_cast<std::size_t>(CharacterSurface::HairStrands)];
  require((m.lod == CharacterLod::Near && card_vertices > 0) ||
              (m.lod == CharacterLod::Far && card_vertices == 0),
          context + "near-only strand cards supplement the retained opaque hair volume");
  for (bool used : used_bones) require(used, context + "every rig joint deforms actual vertices");
  const Vec3 extent = (bounds.high - bounds.low) * (1.f / h);
  require(extent.x > .28f && extent.x < .50f && extent.z > .12f && extent.z < .25f,
          context + "sensible nonzero body, fingers, apron and shoe extents");
  require(std::fabs(bounds.low.y / h + .5f) < .00001f &&
              std::fabs(bounds.high.y / h - .5f) < .001f,
          context + "requested height is preserved from ground to hair crown");
  for (float sole : soles) require(std::fabs(sole / h + .5f) < .00001f,
                                  context + "both feet have grounded sole geometry");
  double volume = 0;
  for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
    const auto a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
    const auto where = context + "triangle " + std::to_string(i / 3);
    require(a < mesh.vertices.size() && b < mesh.vertices.size() && c < mesh.vertices.size(),
            where + " indices in range");
    require(a != b && a != c && b != c, where + " three distinct vertex indices");
    require(vertex_surfaces[a] == vertex_surfaces[b] && vertex_surfaces[b] == vertex_surfaces[c],
            where + " cannot interpolate across material atlas regions");
    const auto& va = mesh.vertices[a];
    const auto& vb = mesh.vertices[b];
    const auto& vc = mesh.vertices[c];
    const auto face = cross(vb.position - va.position, vc.position - va.position);
    require(length(face) > h * h * 1.e-12f, where + " nondegenerate geometry");
    require(dot(face, va.normal + vb.normal + vc.normal) > 0,
            where + " winding agrees with surface normals");
    volume += dot(va.position, cross(vb.position, vc.position)) / 6.;
  }
  // This sums authored overlapping shells, not a boolean union/body-volume measurement.
  require(volume > h * h * h * .003 && volume < h * h * h * .07,
          context + "outward shells enclose a positive plausible signed volume");
  for (std::size_t i = 0; i < character_bone_count; ++i) {
    const auto& joint = m.rig.joints[i];
    require(joint.parent >= -1 && joint.parent < static_cast<int>(i) && finite(joint.bind_position),
            context + "finite acyclic parent-first rig");
  }
  std::cout << context << mesh.vertices.size() << " vertices / " << mesh.indices.size() / 3
            << " triangles, extent " << extent.x << "," << extent.y << "," << extent.z
            << ", signed shell volume/h^3 " << volume / (h * h * h) << '\n';
}

void check_lods_and_materials(const CharacterProfile& near, const CharacterProfile& far) {
  const auto& a = near.model;
  const auto& b = far.model;
  require(exact(a.rig, b.rig) && exact(a.appearance, b.appearance) && a.seed == b.seed &&
              a.dual_quaternion_skinning && b.dual_quaternion_skinning,
          "Near/far Jon retains bit-identical rig, identity and opt-in dual-quaternion skinning");
  // Retain the intact 8,876-triangle anatomical head in both LODs. The far
  // ceiling explicitly includes that cost; reduction still exceeds 50% overall.
  require(a.bind_mesh.indices.size() / 3 <= 40000 && b.bind_mesh.indices.size() / 3 <= 16000,
          "Individual asset respects reviewed CPU cap: 40,000 near / 16,000 far triangles");
  require(b.bind_mesh.indices.size() * 2 < a.bind_mesh.indices.size() &&
              b.bind_mesh.vertices.size() * 2 < a.bind_mesh.vertices.size(),
          "Far LOD eliminates more than half of actual vertices and triangles");
  require(!exact(a.bind_mesh, b.bind_mesh), "LOD changes real geometry");
  std::array<std::size_t, surface_count> av{}, bv{};
  for (const auto& v : a.bind_mesh.vertices) ++av.at(static_cast<std::size_t>(surface_at(v.uv)));
  for (const auto& v : b.bind_mesh.vertices) ++bv.at(static_cast<std::size_t>(surface_at(v.uv)));
  require(av[static_cast<std::size_t>(CharacterSurface::Face)] ==
              bv[static_cast<std::size_t>(CharacterSurface::Face)],
          "Both LODs retain the same anatomical head instead of decimating eyelids/lips");
  for (auto surface : {CharacterSurface::Hair, CharacterSurface::Skin,
                       CharacterSurface::Jacket, CharacterSurface::Leather}) {
    const auto i = static_cast<std::size_t>(surface);
    require(av[i] > bv[i] * 3 / 2, "Near LOD retains additional anatomy, hair and clothing geometry");
  }
  const auto texture = character_surface_textures();
  for (const auto* p : {&near, &far}) {
    const auto& material = p->material;
    require(material.textures == texture && material.textures->base_color.valid() &&
                material.textures->normal.valid() && material.textures->metallic_roughness.valid(),
            "Actual profile binds shared base color, normal and roughness/metallic maps");
    require(exact(material.albedo, {1, 1, 1}) && material.roughness == 1 && material.metallic == 1 &&
                material.normal_scale == 1 && material.world_uv_scale == 0 &&
                material.opacity == 1 && !material.alpha_blend &&
                ((p->model.lod == CharacterLod::Near && material.alpha_cutoff == .4f) ||
                 (p->model.lod == CharacterLod::Far &&
                  (material.alpha_cutoff < 0 || material.alpha_cutoff == .4f))),
            "Profile preserves vertex tint/UV0 PBR and cuts out near strand masks without blending");
    require(std::isfinite(p->lod_distance) && p->lod_distance > 0 &&
                p->lod_distance == near.lod_distance,
            "Both LOD profiles agree on a finite positive individual switch distance");
  }
}

void check_anatomical_head(const CharacterModel& model) {
  const float h = model.rig.height;
  const auto context = label(model) + " anatomical head: ";
  require(length(model.rig.joints[bone_index(CharacterBone::Head)].bind_position -
                 jon_anatomy::head_center * h) < h * 1.e-6f &&
              length(model.rig.joints[bone_index(CharacterBone::Neck)].bind_position -
                 jon_anatomy::neck_center * h) < h * 1.e-6f,
          context + "head and neck rig pivots use the morphed source landmarks");
  std::map<std::array<float, 3>, std::uint32_t> source_positions;
  for (std::uint32_t i = 0; i < jon_anatomy::positions.size(); ++i) {
    const auto p = jon_anatomy::positions[i] * h;
    source_positions.emplace(std::array<float, 3>{{p.x, p.y, p.z}}, i);
  }
  require(source_positions.size() == jon_anatomy::positions.size(),
          context + "source geometry has unambiguous canonical positions");
  std::vector<std::uint32_t> canonical(model.bind_mesh.vertices.size(), UINT32_MAX);
  std::vector<std::uint32_t> first_copy(jon_anatomy::positions.size(), UINT32_MAX);
  std::vector<bool> retained(jon_anatomy::positions.size(), false);
  std::array<Bounds, 2> eye_bounds;
  std::array<std::size_t, 2> eye_vertices{};
  for (std::size_t i = 0; i < model.bind_mesh.vertices.size(); ++i) {
    const auto& v = model.bind_mesh.vertices[i];
    const auto surface = surface_at(v.uv);
    if (surface == static_cast<int>(CharacterSurface::Face)) {
      const auto found = source_positions.find({{v.position.x, v.position.y, v.position.z}});
      require(found != source_positions.end(), context + "every face vertex preserves a source position");
      canonical[i] = found->second;
      retained[found->second] = true;
      auto& first = first_copy[found->second];
      if (first == UINT32_MAX) first = static_cast<std::uint32_t>(i);
      else {
        require(length(v.normal - model.bind_mesh.vertices[first].normal) < 1.e-5f &&
                    exact(model.influences[i], model.influences[first]),
                context + "UV seam copies preserve smooth normals and identical skin weights");
      }
      if (v.position.y > .399f * h) {
        const auto& influence = model.influences[i];
        require(influence.first == bone_index(CharacterBone::Head) && influence.first_weight == 1,
                context + "facial features above the neck blend remain rigidly attached to the skull");
      }
    } else if (surface == static_cast<int>(CharacterSurface::Eyes)) {
      const auto side = v.position.x > 0 ? 0u : 1u;
      eye_bounds[side].add(v.position);
      ++eye_vertices[side];
      require(model.influences[i].first == bone_index(CharacterBone::Head) &&
                  model.influences[i].first_weight == 1,
              context + "eyeballs and iris geometry follow the same head transform as sockets");
    }
  }
  for (bool exists : retained) require(exists, context + "no source head position is dropped in either LOD");
  std::vector<std::uint32_t> actual_indices;
  const auto face_uv_min = character_surface_uv(CharacterSurface::Face, 0, 0);
  const auto face_uv_max = character_surface_uv(CharacterSurface::Face, 1, 1);
  for (std::size_t i = 0; i < model.bind_mesh.indices.size(); i += 3) {
    const auto a = model.bind_mesh.indices[i], b = model.bind_mesh.indices[i + 1],
               c = model.bind_mesh.indices[i + 2];
    if (canonical[a] == UINT32_MAX) continue;
    require(canonical[b] != UINT32_MAX && canonical[c] != UINT32_MAX,
            context + "anatomy triangles use only preserved anatomy positions");
    const float u0 = model.bind_mesh.vertices[a].uv.x,
                u1 = model.bind_mesh.vertices[b].uv.x,
                u2 = model.bind_mesh.vertices[c].uv.x;
    require_element(std::max({u0, u1, u2}) - std::min({u0, u1, u2}) <=
                        (face_uv_max.x - face_uv_min.x) * .5f,
                    context, "source UV chart triangle stays within half the Face region: ", i / 3);
    const auto source_corner = actual_indices.size();
    require(source_corner + 2 < jon_anatomy::uv_indices.size(), context + "no added anatomy triangle");
    const std::uint32_t corners[]{a, b, c};
    for (std::size_t corner = 0; corner < 3; ++corner) {
      const auto uv = jon_anatomy::uv_coordinates[jon_anatomy::uv_indices[source_corner + corner]];
      const auto expected = character_surface_uv(CharacterSurface::Face, uv.x, uv.y);
      const auto actual = model.bind_mesh.vertices[corners[corner]].uv;
      require(exact(actual.x, expected.x) && exact(actual.y, expected.y),
              context + "original per-corner UV seams survive atlas mapping");
    }
    actual_indices.insert(actual_indices.end(), {canonical[a], canonical[b], canonical[c]});
  }
  require(actual_indices.size() == jon_anatomy::indices.size() &&
              std::equal(actual_indices.begin(), actual_indices.end(), jon_anatomy::indices.begin()),
          context + "all source triangles survive intact and in their original winding");
  for (std::size_t side = 0; side < 2; ++side) {
    require(eye_vertices[side] > 0, context + "both source eye sockets contain actual eye geometry");
    const auto center = jon_anatomy::eye_centers[side] * h;
    const auto helper = jon_anatomy::eye_helper_half_extents[side] * h;
    const auto& bounds = eye_bounds[side];
    const auto middle = (bounds.low + bounds.high) * .5f;
    require(std::fabs(middle.x - center.x) < h * .0005f &&
                std::fabs(middle.y - center.y) < h * .0005f,
            context + "eyeballs are centered at the independent morphed eye landmarks");
    require(std::fabs(bounds.low.x - (center.x - helper.x)) < h * .0015f &&
                std::fabs(bounds.low.y - (center.y - helper.y)) < h * .0015f &&
                std::fabs(bounds.low.z - (center.z - helper.z)) < h * .0015f &&
                bounds.high.z > center.z && bounds.high.z < center.z + helper.z + h * .002f,
            context + "globe size and iris layers fit the source helper volume");
  }
}

// The trouser cylinders are capped authored shells. Weld only floating-point
// coordinate drift at UV seams, then require every geometric edge to close.
// This catches non-periodic angular noise that opens or overlaps the u=0/1 seam.
void check_pants_seams(const CharacterModel& model) {
  const auto& mesh = model.bind_mesh;
  std::map<std::array<std::int64_t, 3>, std::uint32_t> welded;
  std::vector<std::uint32_t> canonical(mesh.vertices.size(), UINT32_MAX);
  for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
    if (surface_at(mesh.vertices[i].uv) != static_cast<int>(CharacterSurface::Pants)) continue;
    const auto p = mesh.vertices[i].position * (1.f / model.rig.height);
    const std::array<std::int64_t, 3> key{{std::llround(p.x * 1.e6),
                                         std::llround(p.y * 1.e6),
                                         std::llround(p.z * 1.e6)}};
    const auto index = static_cast<std::uint32_t>(welded.size());
    canonical[i] = welded.emplace(key, index).first->second;
  }
  struct Edge { unsigned count{0}; int orientation{0}; };
  std::map<std::pair<std::uint32_t, std::uint32_t>, Edge> edges;
  for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
    if (canonical[mesh.indices[i]] == UINT32_MAX) continue;
    for (int corner = 0; corner < 3; ++corner) {
      auto a = canonical[mesh.indices[i + corner]], b = canonical[mesh.indices[i + (corner + 1) % 3]];
      require(a != UINT32_MAX && b != UINT32_MAX && a != b,
              label(model) + " pants triangle preserves distinct geometric corners");
      const int direction = a < b ? 1 : -1;
      if (b < a) std::swap(a, b);
      auto& edge = edges[{a, b}]; ++edge.count; edge.orientation += direction;
    }
  }
  require(!edges.empty(), "Pants have real shell edges");
  for (const auto& edge : edges)
    require(edge.second.count == 2 && edge.second.orientation == 0,
            label(model) + " pants seam must be closed, with two oppositely directed faces per geometric edge");
}

// A face-normal disagreement alone cannot distinguish a folded mesh from the
// approximation used to skin bind normals. Check actual triangle crossings.
bool proper_segment_triangle(Vec3 p, Vec3 q, Vec3 a, Vec3 b, Vec3 c, float h) {
  const auto direction = q - p, e1 = b - a, e2 = c - a;
  const auto cross_direction = cross(direction, e2);
  const float determinant = dot(e1, cross_direction);
  if (std::fabs(determinant) < h * h * h * 1.e-12f) return false;
  const auto offset = p - a, cross_offset = cross(offset, e1);
  const float u = dot(offset, cross_direction) / determinant,
              v = dot(direction, cross_offset) / determinant,
              t = dot(e2, cross_offset) / determinant;
  // Boundary contact and shared edges are not proper interior intersections.
  constexpr float epsilon = 1.e-5f;
  return u > epsilon && v > epsilon && u + v < 1 - epsilon &&
         t > epsilon && t < 1 - epsilon;
}
struct KneeCrossingCheck {
  const CharacterModel& model;
  std::vector<std::size_t> triangles;
  std::vector<std::pair<std::size_t, std::size_t>> pairs;
  explicit KneeCrossingCheck(const CharacterModel& source) : model(source) {
    const auto& mesh = model.bind_mesh;
    const float h = model.rig.height;
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
      const auto a = mesh.indices[i], b = mesh.indices[i + 1], c = mesh.indices[i + 2];
      if (surface_at(mesh.vertices[a].uv) != static_cast<int>(CharacterSurface::Pants)) continue;
      const auto center = (mesh.vertices[a].position + mesh.vertices[b].position +
                           mesh.vertices[c].position) * (1.f / (3.f * h));
      if (center.y > -.40f && center.y < -.065f) triangles.push_back(i);
    }
    for (std::size_t a = 0; a < triangles.size(); ++a)
      for (std::size_t b = a + 1; b < triangles.size(); ++b) {
        const auto ta = triangles[a], tb = triangles[b];
        if (mesh.vertices[mesh.indices[ta]].position.x *
            mesh.vertices[mesh.indices[tb]].position.x < 0) continue;
        bool adjacent = false;
        for (int c = 0; c < 3; ++c) for (int d = 0; d < 3; ++d)
          if (length(mesh.vertices[mesh.indices[ta + c]].position -
                     mesh.vertices[mesh.indices[tb + d]].position) < h * 1.e-6f) adjacent = true;
        if (!adjacent) pairs.emplace_back(a, b);
      }
    require(!triangles.empty() && !pairs.empty(), "Both actual knee neighborhoods have candidate triangle pairs");
  }
  void check(const Mesh& mesh, const std::string& context) const {
    std::vector<Bounds> bounds(triangles.size());
    for (std::size_t i = 0; i < triangles.size(); ++i)
      for (int corner = 0; corner < 3; ++corner)
        bounds[i].add(mesh.vertices[mesh.indices[triangles[i] + corner]].position);
    for (const auto& pair : pairs) {
      const auto& a = bounds[pair.first]; const auto& b = bounds[pair.second];
      if (a.high.x < b.low.x || b.high.x < a.low.x || a.high.y < b.low.y ||
          b.high.y < a.low.y || a.high.z < b.low.z || b.high.z < a.low.z) continue;
      Vec3 first[3], second[3];
      for (int i = 0; i < 3; ++i) {
        first[i] = mesh.vertices[mesh.indices[triangles[pair.first] + i]].position;
        second[i] = mesh.vertices[mesh.indices[triangles[pair.second] + i]].position;
      }
      for (int edge = 0; edge < 3; ++edge)
        if (proper_segment_triangle(first[edge], first[(edge + 1) % 3], second[0], second[1], second[2], model.rig.height) ||
            proper_segment_triangle(second[edge], second[(edge + 1) % 3], first[0], first[1], first[2], model.rig.height))
          throw std::runtime_error(context + "proper knee self-intersection between triangles " +
                                   std::to_string(triangles[pair.first] / 3) + " and " +
                                   std::to_string(triangles[pair.second] / 3));
    }
  }
};

CharacterAnimationSample sample_for(int mode, int frame, float h, std::uint32_t seed) {
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
  if (mode == 4 || mode == 5) sample.crouch_weight = 1;
  return sample;
}
void check_animation(const CharacterModel& model) {
  const float h = model.rig.height;
  const auto bind = model.bind_mesh;
  const KneeCrossingCheck knees(model);
  knees.check(bind, label(model) + " bind pose: ");
  Mesh output = bind;
  output.gpu_vao = 101; output.gpu_vbo = 102; output.gpu_ibo = 103; output.gpu_uploaded = true;
  const auto* vertices = output.vertices.data();
  const auto* indices = output.indices.data();
  const auto vertex_capacity = output.vertices.capacity(), index_capacity = output.indices.capacity();
  const auto identity = output.geometry_identity;
  constexpr const char* modes[]{"idle", "walk", "run", "talk/attention", "crouch", "crouch-walk"};
  std::size_t poses = 0;
  for (int mode = 0; mode < 6; ++mode) {
    std::vector<Vec3> previous(output.vertices.size());
    float largest_change = 0;
    for (int frame = 0; frame <= 32; ++frame) {
      const auto sample = sample_for(mode, frame, h, model.seed);
      const auto pose = sample_character_animation(model.rig, sample);
      const auto context = label(model) + " " + modes[mode] + " frame " + std::to_string(frame) + ": ";
      for (const auto& matrix : pose.skin_matrices)
        for (float value : matrix.m) require(std::isfinite(value), context + "finite skin matrix");
      apply_character_pose(model, pose, output);
      require(output.vertices.data() == vertices && output.indices.data() == indices &&
                  output.vertices.capacity() == vertex_capacity && output.indices.capacity() == index_capacity &&
                  output.vertices.size() == bind.vertices.size() && output.indices == bind.indices &&
                  output.geometry_identity == identity && output.gpu_vao == 101 &&
                  output.gpu_vbo == 102 && output.gpu_ibo == 103 && output.gpu_uploaded,
              context + "skinning keeps topology, storage and renderer handles stable");
      const auto revision = output.geometry_revision;
      require(!apply_character_pose(model, pose, output) && output.geometry_revision == revision,
              context + "identical pose does not rebuild or dirty geometry twice");
      for (std::size_t i = 0; i < output.vertices.size(); ++i) {
        const auto& v = output.vertices[i];
        const auto& source = bind.vertices[i];
        require_element(finite(v.position) && finite(v.normal) && std::fabs(length(v.normal) - 1) < .0002f,
                        context, "finite skinned vertex/unit normal ", i);
        require_element(length(v.position) < h * 1.1f, context, "no exploding deformed vertex ", i);
        require_element(exact(v.color, source.color) && exact(v.uv.x, source.uv.x) &&
                    exact(v.uv.y, source.uv.y) && exact(v.opacity, source.opacity),
                        context, "skinning preserves exact color/UV/opacity at vertex ", i);
        const auto bone = model.influences[i].first;
        if (bone == bone_index(CharacterBone::LeftFoot) || bone == bone_index(CharacterBone::RightFoot))
          require_element(v.position.y >= -.5001f * h, context,
                          "actual shoe geometry stays above ground at vertex ", i);
        if (frame) {
          const float displacement = length(v.position - previous[i]);
          require_element(displacement < h * .14f, context, "adjacent sampled frames do not snap at vertex ", i);
          largest_change = std::max(largest_change, displacement);
        }
        previous[i] = v.position;
      }
      for (std::size_t i = 0; i < output.indices.size(); i += 3) {
        const auto a = output.vertices[output.indices[i]].position;
        const auto b = output.vertices[output.indices[i + 1]].position;
        const auto c = output.vertices[output.indices[i + 2]].position;
        require_element(length(cross(b - a, c - a)) > h * h * 1.e-13f,
                        context, "skinning cannot collapse triangle ", i / 3);
      }
      if (mode == 2 || mode == 4 || mode == 5)
        knees.check(output, context);
      ++poses;
    }
    require(largest_change > h * .000001f, std::string(modes[mode]) + " changes actual model vertices");
  }
  require(exact(bind, model.bind_mesh), "Animation never mutates the immutable bind geometry");
  std::cout << label(model) << ": " << poses << " actual skinned idle/walk/run/talk/crouch/crouch-walk poses\n";
}

// Derive the lip seam from the exact canonical charts, rather than a broad
// coordinate box or a helper joint that lives inside the mouth.
std::set<unsigned> lip_runtime_vertices(const CharacterModel& m) {
  std::set<unsigned> external, interior, source_lip, runtime;
  for (unsigned t = 0; t < jon_anatomy::indices.size(); t += 3) {
    const auto uv = jon_anatomy::uv_coordinates[jon_anatomy::uv_indices[t]];
    const bool head = uv.x > .64f && uv.y >= .14869f && uv.y <= .81764f;
    const bool mouth = uv.x > .75f && uv.x < .93f && uv.y < .13645f;
    for (unsigned k = 0; k < 3; ++k) {
      if (head) external.insert(jon_anatomy::indices[t + k]);
      if (mouth) interior.insert(jon_anatomy::indices[t + k]);
    }
  }
  std::set_intersection(external.begin(), external.end(), interior.begin(),
                        interior.end(), std::inserter(source_lip, source_lip.end()));
  require(source_lip.size() == 38, "Exact 38-source-vertex anatomical lip seam");
  unsigned cursor = 0;
  std::set<unsigned> observed_source;
  for (unsigned t = 0; t < m.bind_mesh.indices.size(); t += 3) {
    if (surface_at(m.bind_mesh.vertices[m.bind_mesh.indices[t]].uv) !=
        int(CharacterSurface::Face)) continue;
    for (unsigned k = 0; k < 3; ++k, ++cursor) {
      require(cursor < jon_anatomy::indices.size(), "Runtime head does not exceed canonical corner stream");
      const auto source_index = jon_anatomy::indices[cursor];
      if (source_lip.count(source_index)) {
        observed_source.insert(source_index);
        runtime.insert(m.bind_mesh.indices[t + k]);
      }
    }
  }
  require(cursor == jon_anatomy::indices.size(), "Runtime head retains the entire canonical corner stream");
  require(observed_source == source_lip && runtime.size() == 77,
          "All 38 source lip positions retain all 77 runtime UV corners");
  return runtime;
}
float lip_attention_error(const CharacterModel& m) {
  const auto lip = lip_runtime_vertices(m);
  auto mesh = m.bind_mesh;
  float maximum = 0;
  for (float yaw : {-.9f, 0.f, .9f})
    for (float pitch : {-.4f, 0.f, .4f}) {
      CharacterAnimationSample sample;
      sample.seed = m.seed; sample.phase_cycles = .125; sample.idle_time = .125;
      sample.talk_weight = 1; sample.attention_weight = 1;
      sample.attention_yaw = yaw; sample.attention_pitch = pitch;
      const auto pose = sample_character_animation(m.rig, sample);
      apply_character_pose(m, pose, mesh);
      for (const auto vertex : lip) {
        const auto expected = transform_point(pose.skin_matrices[bone_index(CharacterBone::Head)],
                                               m.bind_mesh.vertices[vertex].position);
        maximum = std::max(maximum, length(mesh.vertices[vertex].position - expected));
      }
    }
  return maximum / m.rig.height;
}
void lip_attention_regression(const CharacterModel& m) {
  const float error = lip_attention_error(m);
  std::cout << "Exact lip attention maximum rigid-Head error/h " << error << '\n';
  require(error < 2e-6f, "All source lip corners stay rigid to Head in nine attention samples");
  auto broken = m;
  for (const auto vertex : lip_runtime_vertices(broken)) {
    const float y = broken.bind_mesh.vertices[vertex].position.y / broken.rig.height;
    const float t = std::clamp((y - .355f) / .044f, 0.f, 1.f);
    broken.influences[vertex] = {std::uint8_t(CharacterBone::Head),
                                std::uint8_t(CharacterBone::Neck), t * t * (3 - 2 * t)};
  }
  const float old_error = lip_attention_error(broken);
  std::cout << "Old Jon lip-weight negative control rigid-Head error/h " << old_error << '\n';
  require(old_error > .001f, "Regression detects the original lip-shear weighting");
}

// This independent horizontal-ray check covers the unambiguous upper crown,
// above the ears and deliberately shaped hairline. It caught actual pale scalp
// holes in the original coarse Far shell; it does not pretend the ear region
// should be covered in hair or certify every strand/card intersection.
struct CrownTriangle { Vec3 a,b,c; float low_y, high_y; };
struct CrownCoverage { unsigned samples{}, outside{}; float maximum_gap{}; };
float crown_ray(Vec3 origin, Vec3 direction, const CrownTriangle& t) {
  const auto e=t.b-t.a, f=t.c-t.a, q=cross(direction,f);
  const float determinant=dot(e,q);
  if(std::fabs(determinant)<1e-12f) return -1;
  const auto v=origin-t.a; const float u=dot(v,q)/determinant;
  const auto s=cross(v,e);
  const float w=dot(direction,s)/determinant, distance=dot(f,s)/determinant;
  return u>=-1e-6f && w>=-1e-6f && u+w<=1+1e-6f && distance>0 ? distance : -1;
}
CrownCoverage opaque_crown_coverage(const CharacterModel& model) {
  std::vector<CrownTriangle> hair;
  const auto& mesh=model.bind_mesh;
  for(std::size_t i=0;i<mesh.indices.size();i+=3) {
    if(surface_at(mesh.vertices[mesh.indices[i]].uv)!=int(CharacterSurface::Hair)) continue;
    CrownTriangle t;
    t.a=mesh.vertices[mesh.indices[i]].position*(1/model.rig.height);
    t.b=mesh.vertices[mesh.indices[i+1]].position*(1/model.rig.height);
    t.c=mesh.vertices[mesh.indices[i+2]].position*(1/model.rig.height);
    t.low_y=std::min({t.a.y,t.b.y,t.c.y}); t.high_y=std::max({t.a.y,t.b.y,t.c.y});
    hair.push_back(t);
  }
  CrownCoverage result;
  auto sample=[&](Vec3 p) {
    if(p.y<.445f) return; // Excludes ears, sideburn roots and lower nape deliberately.
    const float t=std::clamp((p.y-.46f)/.033f,0.f,1.f);
    const float z=jon_anatomy::head_center.z+
        (jon_anatomy::crown_joint.z-jon_anatomy::head_center.z)*t*t*(3-2*t);
    Vec3 direction{p.x,0,p.z-z}; const float radius=length(direction);
    if(radius<1e-6f) return;
    const float angle=std::atan2(direction.z,direction.x);
    const float front=std::max(0.f,std::sin(angle)), back=std::max(0.f,-std::sin(angle));
    const float hairline=.434f+.031f*front-.039f*back-.004f*std::cos(angle)*front+
        .001f*std::sin(angle*7)*(.3f+back);
    if(p.y<hairline+.004f) return; // Never mistake the exposed forehead for a hole.
    direction=direction*(1/radius); float cover=-1;
    for(const auto& triangle:hair)
      if(p.y>=triangle.low_y-1e-7f && p.y<=triangle.high_y+1e-7f)
        cover=std::max(cover,crown_ray({0,p.y,z},direction,triangle));
    const float gap=radius-cover;
    ++result.samples; result.maximum_gap=std::max(result.maximum_gap,gap);
    if(gap>2e-6f) ++result.outside;
  };
  for(auto index:std::set<unsigned>(jon_anatomy::indices.begin(),jon_anatomy::indices.end()))
    sample(jon_anatomy::positions[index]);
  for(std::size_t t=0;t<jon_anatomy::indices.size();t+=3)
    sample((jon_anatomy::positions[jon_anatomy::indices[t]]+
            jon_anatomy::positions[jon_anatomy::indices[t+1]]+
            jon_anatomy::positions[jon_anatomy::indices[t+2]])*(1.f/3));
  return result;
}
void check_opaque_crown(const CharacterModel& model) {
  const auto result=opaque_crown_coverage(model);
  std::cout<<label(model)<<": opaque crown "<<result.samples<<" source vertex/triangle samples, "
           <<result.outside<<" uncovered, maximum gap/h "<<result.maximum_gap<<'\n';
  require(result.samples>300 && result.outside==0,"Opaque upper crown covers the actual source scalp");
  if(model.rig.height==1.70f) {
    auto broken=model;
    for(auto& v:broken.bind_mesh.vertices)
      if(surface_at(v.uv)==int(CharacterSurface::Hair)) {
        v.position.x*=.8f;
        const float z=jon_anatomy::head_center.z*model.rig.height;
        v.position.z=z+(v.position.z-z)*.8f;
      }
    require(opaque_crown_coverage(broken).outside>30,
            "Shrunken opaque-hair negative control exposes source scalp");
  }
}

void generic_identity_fallback() {
  struct Identity { const char* id; CharacterRole role; };
  using R = CharacterRole;
  constexpr Identity roster[]{{"unregistered_hale_security", R::Security}, {"unregistered_bank_staff", R::BankStaff}, {"unregistered_desk_guard", R::Security},
      {"unregistered_metro_security", R::Security}, {"unregistered_enforcer", R::Enforcer},
      {"unregistered_commuter", R::Commuter}, {"unregistered_alley_security", R::Security}, {"unregistered_market", R::Market},
      {"unregistered_dock", R::Dock}, {"unregistered_east_dock", R::Dock}, {"unregistered_fence", R::Fence},
      {"unregistered_crew_tech", R::CrewTech}, {"unregistered_scout", R::CrewScout}, {"unregistered_player", R::Player},
      {"unregistered_ghost", R::Ghost}, {"unknown_identity", R::Enforcer},
      {"npccivb", R::Market}, {"NpcCivB_extra", R::Market}, {"", R::Commuter}};
  std::size_t cases = 0;
  auto check = [&](float h, R role, std::string_view id, CharacterLod lod) {
    const auto profile = make_character_profile(h, role, id, lod);
    const auto generic = make_character_model(h, role, character_seed(id), lod);
    require(!profile.individualized && profile.lod_distance == 0 && !profile.material.textures,
            "Unregistered identity/role retains default generic profile behavior");
    require(exact(profile.model, generic), "Generic fallback preserves every geometry/rig/weight/appearance bit: " +
                                              std::string(id) + " " + character_role_name(role));
    ++cases;
  };
  for (float h : heights) for (auto lod : {CharacterLod::Near, CharacterLod::Far}) {
    for (const auto& identity : roster) check(h, identity.role, identity.id, lod);
    for (unsigned r = 0; r < static_cast<unsigned>(R::Count); ++r)
      if (static_cast<R>(r) != R::Market)
        check(h, static_cast<R>(r), "NpcCivB", lod);
    check(h, R::Market, std::string_view("NpcCivB\0extra", 13), lod);
  }
  std::cout << cases << " bit-exact generic identity/role/height/LOD fallback cases\n";
}
void input_validation() {
  for (auto id : {"NpcCivB", "unknown_identity"})
    for (auto lod : {CharacterLod::Near, CharacterLod::Far})
      for (float height : {0.f, -1.f, std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()}) {
        bool threw = false;
        try { make_character_profile(height, CharacterRole::Market, id, lod); }
        catch (const std::invalid_argument&) { threw = true; }
        require(threw, "Invalid height is rejected for individual and generic identities");
      }
  for (auto id : {"NpcCivB", "unknown_identity"}) {
    bool bad_role = false, bad_lod = false;
    try { make_character_profile(1.70f, CharacterRole::Count, id, CharacterLod::Near); }
    catch (const std::invalid_argument&) { bad_role = true; }
    try { make_character_profile(1.70f, CharacterRole::Market, id, static_cast<CharacterLod>(255)); }
    catch (const std::invalid_argument&) { bad_lod = true; }
    require(bad_role && bad_lod, "Invalid role and LOD fail explicitly");
  }
}
}  // namespace

int main() {
  try {
    input_validation();
    generic_identity_fallback();
    for (float height : heights) {
      const auto near = make_character_profile(height, CharacterRole::Market, "NpcCivB", CharacterLod::Near);
      const auto far = make_character_profile(height, CharacterRole::Market, "NpcCivB", CharacterLod::Far);
      check_geometry(near); check_geometry(far);
      check_pants_seams(near.model); check_pants_seams(far.model);
      check_lods_and_materials(near, far);
      check_anatomical_head(near.model); check_anatomical_head(far.model);
      lip_attention_regression(near.model); lip_attention_regression(far.model);
      check_opaque_crown(near.model); check_opaque_crown(far.model);
      for (const auto* profile : {&near, &far}) {
        const auto repeated = make_character_profile(height, CharacterRole::Market, "NpcCivB", profile->model.lod);
        require(exact(profile->model, repeated.model) && profile->material.textures == repeated.material.textures &&
                    exact(profile->model.appearance, character_appearance(character_seed("NpcCivB"))),
                "Jon generation is bit-deterministic and retains his seeded identity and immutable textures");
      }
      check_animation(near.model); check_animation(far.model);
    }
    std::cout << "Jon character geometry and deformation checks passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Jon character geometry and deformation check failed: " << e.what() << '\n';
    return 1;
  }
}
