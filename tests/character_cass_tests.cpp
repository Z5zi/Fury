#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include "fury/render_settings.hpp"
#include "fury/generated/cass_anatomy.hpp"
#include "fury/generated/cass_hands.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
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
  require(p.individualized && m.seed == 0x513cdaa9 &&
              m.role == CharacterRole::Fence,
          "Cass stable identity/role");
  require(m.dual_quaternion_skinning && p.lod_distance == 12,
          "Cass rig/LOD contract");
  require(mesh.vertices.size() == m.influences.size(),
          "All vertices have weights");
  require(mesh.indices.size() / 3 <=
              (m.lod == CharacterLod::Near ? 40000 : 16000),
          "Cass explicit triangle budget");
  require(same(m.appearance.skin, {.90f, .69f, .52f}) &&
              same(m.appearance.hair, {.27f, .11f, .052f}) &&
              same(m.appearance.shirt, {.23f, .39f, .37f}) &&
              same(m.appearance.trousers, {.19f, .23f, .20f}) &&
              same(m.appearance.accent, {.43f, .32f, .45f}) &&
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
                 CharacterSurface::HairShortStrands, CharacterSurface::Shirt,
                 CharacterSurface::Pants, CharacterSurface::Jacket,
                 CharacterSurface::Rubber, CharacterSurface::Leather})
    require(counts[int(s)] > 0, "Required actual surface present");
}
void check_dispatch_and_materials() {
  const auto near = make_character_profile(1.78f, CharacterRole::Fence, "NpcFence", CharacterLod::Near);
  const auto far = make_character_profile(1.78f, CharacterRole::Fence, "NpcFence", CharacterLod::Far);
  for (const auto *p : {&near, &far}) {
    require(p->material.textures == character_surface_textures() &&
                p->material.alpha_cutoff == .4f && p->material.roughness == 1 &&
                p->material.metallic == 1 && p->material.normal_scale == 1 &&
                p->material.world_uv_scale == 0 && p->material.opacity == 1 &&
                !p->material.alpha_blend && same(p->material.albedo, {1, 1, 1}),
            "Actual Cass uses the existing shared UV0 material with production hair cutout");
    require(p->model.seed == 0x513cdaa9 && p->model.role == CharacterRole::Fence &&
                p->individualized && p->lod_distance == 12,
            "Exact case-sensitive Cass identity and reviewed LOD threshold");
  }
  require(far.model.bind_mesh.indices.size()*2 < near.model.bind_mesh.indices.size(),
          "Far removes more than half of actual shipping triangles");
  for (std::size_t i = 0; i < character_bone_count; ++i) {
    const auto &a = near.model.rig.joints[i], &b = far.model.rig.joints[i];
    require(a.parent == b.parent && same(a.bind_position, b.bind_position, 0),
            "Both LODs use identical actual rig pivots and hierarchy");
    require(a.parent >= -1 && a.parent < int(i) && finite(a.bind_position),
            "Finite acyclic parent-first rig");
  }
  for (const std::string &id : {std::string("npcFence"), std::string("NPCFENCE"),
       std::string("NpcFence_extra"), std::string("NpcFence\0extra", 14), std::string("")}) {
    const auto profile = make_character_profile(1.78f, CharacterRole::Fence, id, CharacterLod::Near);
    const auto generic = make_character_model(1.78f, CharacterRole::Fence, character_seed(id), CharacterLod::Near);
    require(!profile.individualized && profile.lod_distance == 0 &&
            profile.model.bind_mesh.indices == generic.bind_mesh.indices &&
            profile.model.bind_mesh.vertices.size() == generic.bind_mesh.vertices.size(),
            "Near-matching identities keep the generic fallback");
    for (std::size_t i = 0; i < generic.bind_mesh.vertices.size(); ++i) {
      const auto &a = profile.model.bind_mesh.vertices[i], &b = generic.bind_mesh.vertices[i];
      require(same(a.position,b.position,0) && same(a.normal,b.normal,0) && same(a.color,b.color,0) &&
              a.uv.x == b.uv.x && a.uv.y == b.uv.y, "Fallback preserves actual geometry/UV/color bits");
    }
  }
  for (unsigned role = 0; role < unsigned(CharacterRole::Count); ++role) {
    if (role == unsigned(CharacterRole::Fence)) continue;
    require(!make_character_profile(1.78f, CharacterRole(role), "NpcFence", CharacterLod::Near).individualized,
            "Cass registration never steals another gameplay appearance role");
  }
  for (float height : {0.f, -1.f, std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
    bool rejected = false;
    try { (void)make_character_profile(height, CharacterRole::Fence, "NpcFence", CharacterLod::Near); }
    catch (const std::invalid_argument &) { rejected = true; }
    require(rejected, "Invalid height is rejected before constructing geometry");
  }
}

void head(const CharacterModel &m) {
  const auto &mesh = m.bind_mesh;
  std::size_t cursor = 0;
  std::vector<bool> seen(cass_anatomy::positions.size());
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
    if (surface_at(mesh.vertices[mesh.indices[t]].uv) !=
        int(CharacterSurface::Face))
      continue;
    require(cursor + 2 < cass_anatomy::indices.size(),
            "No added face triangles");
    for (int k = 0; k < 3; ++k) {
      auto i = cass_anatomy::indices[cursor + k];
      auto &v = mesh.vertices[mesh.indices[t + k]];
      auto uv =
          cass_anatomy::uv_coordinates[cass_anatomy::uv_indices[cursor + k]];
      auto expected = character_surface_uv(CharacterSurface::Face, uv.x, uv.y);
      require(
          same(v.position, cass_anatomy::positions[i] * m.rig.height, 1e-7f) &&
              std::fabs(v.uv.x - expected.x) < 1e-7f &&
              std::fabs(v.uv.y - expected.y) < 1e-7f,
          "Original head position/corner UV/winding correspondence");
      seen[i] = true;
    }
    cursor += 3;
  }
  require(cursor == cass_anatomy::indices.size() &&
              std::all_of(seen.begin(), seen.end(), [](bool a) { return a; }),
          "Every source face retained");
  require(same(m.rig.joints[bone_index(CharacterBone::Head)].bind_position,
               cass_anatomy::head_center * m.rig.height),
          "Head joint belongs to Cass source");
}
// The lip boundary is already above Cass's old neck blend. The visible
// attention failure came from the deeper internal mouth floor, so derive the
// entire mouth chart from canonical source UVs and include every UV corner.
std::set<unsigned> mouth_runtime_vertices(const CharacterModel &m) {
  std::set<unsigned> source_mouth, runtime, seen;
  for (unsigned t = 0; t < cass_anatomy::indices.size(); t += 3) {
    const auto uv = cass_anatomy::uv_coordinates[cass_anatomy::uv_indices[t]];
    if (uv.x > .75f && uv.x < .93f && uv.y < .13645f)
      for (unsigned k = 0; k < 3; ++k) source_mouth.insert(cass_anatomy::indices[t + k]);
  }
  require(source_mouth.size() == 418, "Exact 418-position canonical internal-mouth chart");
  unsigned cursor = 0;
  for (unsigned t = 0; t < m.bind_mesh.indices.size(); t += 3) {
    if (surface_at(m.bind_mesh.vertices[m.bind_mesh.indices[t]].uv) != int(CharacterSurface::Face)) continue;
    for (unsigned k = 0; k < 3; ++k, ++cursor) {
      require(cursor < cass_anatomy::indices.size(), "Mouth correspondence retains source corner stream");
      const auto source = cass_anatomy::indices[cursor];
      if (source_mouth.count(source)) { seen.insert(source); runtime.insert(m.bind_mesh.indices[t + k]); }
    }
  }
  require(cursor == cass_anatomy::indices.size() && seen == source_mouth && runtime.size() == 484,
          "Every mouth position and all 484 source UV corners remain present");
  return runtime;
}
float rigid_head_attention_error(const CharacterModel &m, const std::set<unsigned> &mouth) {
  auto posed = m.bind_mesh;
  float maximum = 0;
  for (float yaw : {-.9f, 0.f, .9f}) for (float pitch : {-.4f, 0.f, .4f}) {
    CharacterAnimationSample sample;
    sample.seed = m.seed; sample.phase_cycles = .5; sample.idle_time = .125;
    sample.talk_weight = sample.attention_weight = 1;
    sample.attention_yaw = yaw; sample.attention_pitch = pitch;
    const auto pose = sample_character_animation(m.rig, sample);
    apply_character_pose(m, pose, posed);
    for (auto i : mouth) {
      const auto expected = transform_point(pose.skin_matrices[bone_index(CharacterBone::Head)],
                                             m.bind_mesh.vertices[i].position);
      maximum = std::max(maximum, length(posed.vertices[i].position - expected));
    }
  }
  return maximum / m.rig.height;
}
float mouth_attention_error(const CharacterModel &m) {
  return rigid_head_attention_error(m, mouth_runtime_vertices(m));
}
void mouth_attention_regression(const CharacterModel &m) {
  const float error = mouth_attention_error(m);
  require(error < 2e-6f, "Complete source mouth stays rigid to Head in nine attention samples");
  auto old_weights = m;
  for (auto i : mouth_runtime_vertices(old_weights)) {
    const float y = old_weights.bind_mesh.vertices[i].position.y / old_weights.rig.height;
    const float t = std::clamp((y - .357f) / .027f, 0.f, 1.f);
    old_weights.influences[i] = {std::uint8_t(CharacterBone::Head), std::uint8_t(CharacterBone::Neck), t*t*(3-2*t)};
  }
  const float old_error = mouth_attention_error(old_weights);
  std::cout << "Mouth rigid-Head error/h " << error << "; original-weight negative control " << old_error << '\n';
  require(old_error > .001f, "Mouth regression rejects the original neck-blend shear");
}

void exterior_jaw_attention_regression(const CharacterModel &m) {
  std::set<unsigned> mouth, jaw, runtime;
  for (unsigned t = 0; t < cass_anatomy::indices.size(); t += 3) {
    const auto uv = cass_anatomy::uv_coordinates[cass_anatomy::uv_indices[t]];
    if (uv.x > .75f && uv.x < .93f && uv.y < .13645f)
      for (unsigned k = 0; k < 3; ++k) mouth.insert(cass_anatomy::indices[t+k]);
  }
  // Independent measured exterior-chin witness around the source jaw landmark.
  // The cavity is excluded so making only the mouth rigid cannot pass this.
  for (unsigned i = 0; i < cass_anatomy::positions.size(); ++i)
    if (!mouth.count(i) && length(cass_anatomy::positions[i] - cass_anatomy::jaw_center) <= .020f)
      jaw.insert(i);
  require(jaw.size() == 308, "Exact 308-position exterior jaw witness");
  unsigned cursor = 0;
  for (unsigned t = 0; t < m.bind_mesh.indices.size(); t += 3) {
    if (surface_at(m.bind_mesh.vertices[m.bind_mesh.indices[t]].uv) != int(CharacterSurface::Face)) continue;
    for (unsigned k = 0; k < 3; ++k, ++cursor)
      if (jaw.count(cass_anatomy::indices[cursor])) runtime.insert(m.bind_mesh.indices[t+k]);
  }
  require(runtime.size() == 308, "All exterior jaw runtime corners are checked");
  const float error = rigid_head_attention_error(m, runtime);
  require(error < 2e-6f, "Exterior source jaw stays rigid to Head in nine attention samples");
  auto original = m;
  for (auto i : runtime) {
    const float y = original.bind_mesh.vertices[i].position.y / original.rig.height;
    const float t = std::clamp((y - .357f) / .027f, 0.f, 1.f);
    original.influences[i] = {std::uint8_t(CharacterBone::Head),std::uint8_t(CharacterBone::Neck),t*t*(3-2*t)};
  }
  const float old_error = rigid_head_attention_error(original, runtime);
  std::cout << "Exterior jaw rigid-Head error/h " << error << "; original-weight negative control " << old_error << '\n';
  require(old_error > .001f, "Exterior-jaw test rejects the original chin shear");
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
  // Double predicates avoid false "interior" contacts at a sewn shared
  // corner after a rigid transform. Source/posed vertex values remain float;
  // no geometric displacement or contact exclusion is introduced here.
  using D = std::array<double, 3>;
  const auto vector = [](Vec3 v) { return D{v.x, v.y, v.z}; };
  const auto sub = [](D x, D y) { return D{x[0]-y[0], x[1]-y[1], x[2]-y[2]}; };
  const auto crossd = [](D x, D y) {
    return D{x[1]*y[2]-x[2]*y[1], x[2]*y[0]-x[0]*y[2], x[0]*y[1]-x[1]*y[0]};
  };
  const auto dotd = [](D x, D y) { return x[0]*y[0]+x[1]*y[1]+x[2]*y[2]; };
  const D direction = sub(vector(q), vector(p)), e1 = sub(vector(b), vector(a)),
          e2 = sub(vector(c), vector(a));
  const D cross_direction = crossd(direction, e2);
  const double determinant = dotd(e1, cross_direction);
  if (std::fabs(determinant) < double(h)*h*h*1e-15) return false;
  const D offset = sub(vector(p), vector(a)), cross_offset = crossd(offset, e1);
  const double u = dotd(offset, cross_direction)/determinant,
               v = dotd(direction, cross_offset)/determinant,
               t = dotd(e2, cross_offset)/determinant;
  constexpr double epsilon = 1e-7;
  return u > epsilon && v > epsilon && u+v < 1-epsilon &&
         t > epsilon && t < 1-epsilon;
}

std::array<std::vector<Vec3>, 2> source_eye_openings() {
  std::map<std::pair<std::uint32_t, std::uint32_t>, unsigned> charts;
  for (std::size_t i = 0; i < cass_anatomy::indices.size(); i += 3) {
    const auto uv = cass_anatomy::uv_coordinates[cass_anatomy::uv_indices[i]];
    const unsigned chart = uv.x < .08f ? 2 : uv.x < .16f ? 1 : 0;
    for (int c = 0; c < 3; ++c) {
      auto a = cass_anatomy::indices[i + c],
           b = cass_anatomy::indices[i + (c + 1) % 3];
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
      result[side].push_back(cass_anatomy::positions[i]);
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
    if (surface == int(CharacterSurface::Eyes)) {
      const auto &mesh = model.bind_mesh;
      float furthest = -1;
      for (int k = 0; k < 3; ++k)
        furthest =
            std::max(furthest, mesh.vertices[mesh.indices[t + k]].position.z /
                                   model.rig.height);
      if (furthest < cass_anatomy::eye_centers[0].z +
                         cass_anatomy::eye_helper_half_extents[0].z + .001f)
        eyes.push_back(t);
    }
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
  require(!cap.empty() && face.size() == cass_anatomy::indices.size() / 3,
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

CharacterAnimationSample cass_sample(int mode, int frame, float height) {
  CharacterAnimationSample s;
  s.seed = 0x513cdaa9;
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
void check_animation(const CharacterModel &m, bool require_seams = true) {
  const auto &bind = m.bind_mesh;
  Mesh posed = bind;
  const auto *storage = posed.vertices.data();
  const auto *indices = posed.indices.data();
  const auto identity = posed.geometry_identity;
  std::size_t poses = 0, seam_events = 0;
  float min_area_ratio = 1, max_seam_gap = 0;
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
          m.rig, cass_sample(mode, frame, m.rig.height));
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
      for (auto [a, b] : seams) {
        if (!same(posed.vertices[a].position, posed.vertices[b].position, 1e-7f)) {
          const auto pa = posed.vertices[a].position, pb = posed.vertices[b].position;
          const auto source = bind.vertices[a].position;
          const auto weights = m.influences[a];
          ++seam_events;
          max_seam_gap = std::max(max_seam_gap, length(pa-pb));
          if (seam_events <= 3) std::cout << std::setprecision(12) << "SEAM witness mode " << mode << " frame " << frame
                    << " vertices " << a << '/' << b << " gap " << length(pa-pb)
                    << " source " << source.x << ',' << source.y << ',' << source.z
                    << " posed A " << pa.x << ',' << pa.y << ',' << pa.z
                    << " B " << pb.x << ',' << pb.y << ',' << pb.z
                    << " weights " << int(weights.first) << ',' << int(weights.second) << ','
                    << weights.first_weight << ',' << int(weights.third) << ',' << weights.third_weight << '\n';
        }
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
      auto s = cass_sample(3, 9, m.rig.height);
      s.attention_yaw = yaw * .30f;
      s.attention_pitch = pitch * .2f;
      auto pose = sample_character_animation(m.rig, s);
      apply_character_pose(m, pose, posed);
      for (const auto &v : posed.vertices)
        require(finite(v.position) && finite(v.normal) && std::fabs(length(v.normal)-1) < .001f,
                "Finite actual bounded attention geometry");
      for (auto [a, b] : seams) if (!same(posed.vertices[a].position, posed.vertices[b].position, 1e-7f)) {
        ++seam_events;
        max_seam_gap = std::max(max_seam_gap, length(posed.vertices[a].position-posed.vertices[b].position));
      }
      ++poses;
    }
  std::cout << label(m) << " height " << m.rig.height << ": " << poses
            << " poses, " << seams.size()
            << " welded corner pairs, minimum area ratio " << min_area_ratio
            << ", strict seam events " << seam_events << ", max gap meters " << max_seam_gap << '\n';
  // Collect every pose before failing, so a sub-micron inherited cache witness
  // cannot prevent the remaining motion/attention geometry from being checked.
  if (require_seams)
    require(seam_events == 0, "Actual pose opens a sewn seam (strict 1e-7 m check; all poses evaluated)");
}

void hand_source_contract(const CharacterModel &m) {
  const auto &mesh = m.bind_mesh;
  const float h = m.rig.height;
  for (float side : {-1.f, 1.f}) {
    auto expected = [&](auto const &indices, auto const &uvs) {
      std::multiset<std::tuple<float, float, float, float, float>> corners;
      for (std::size_t k = 0; k < indices.size(); ++k) {
        auto p = cass_hands::positions[indices[k]];
        p.x *= side;
        const float a = side * 1.12f, c = std::cos(a), sn = std::sin(a);
        p = {c * p.x + sn * p.z, p.y, -sn * p.x + c * p.z};
        p += Vec3{side * .163f, -.043f, .018f};
        p = p * h;
        auto uv = cass_hands::uv_coordinates[uvs[k]];
        uv = character_surface_uv(CharacterSurface::Skin, uv.x, uv.y);
        corners.emplace(p.x, p.y, p.z, uv.x, uv.y);
      }
      return corners;
    };
    const auto need =
        m.lod == CharacterLod::Near
            ? expected(cass_hands::near_indices, cass_hands::near_uv_indices)
            : expected(cass_hands::far_indices, cass_hands::far_uv_indices);
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

struct CassContacts {
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
          q.p[k] = mesh.vertices[mesh.indices[i + k]].position * (1 / h);
          q.bounds.add(q.p[k]);
        }
        out.push_back(q);
      }
      return out;
    };
    auto a = triangles(aa), b = triangles(bb);
    using Key = std::tuple<int, int, int>;
    std::map<Key, std::vector<unsigned>> grid;
    auto range = [](const Bounds &q) {
      std::array<int, 6> r{};
      r = {
          int(std::floor(q.low.x / .025f)),  int(std::floor(q.low.y / .025f)),
          int(std::floor(q.low.z / .025f)),  int(std::floor(q.high.x / .025f)),
          int(std::floor(q.high.y / .025f)), int(std::floor(q.high.z / .025f))};
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
                                             q.p[1], q.p[2], 1) ||
                     proper_segment_triangle(q.p[k], q.p[(k + 1) % 3], p.p[0],
                                             p.p[1], p.p[2], 1);
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

// Independent proper triangle/triangle layer tests use the actual shipping
// mesh, not analytic envelopes. Expected cloth attachment seams meet only at
// boundaries; the strict interior predicate deliberately excludes those.
void check_contacts(const CharacterModel &model, bool all_poses, int modes = 6) {
  const auto &bind = model.bind_mesh;
  const float h = model.rig.height;
  std::map<std::string, std::vector<std::size_t>> groups;
  for (std::size_t t = 0; t < bind.indices.size(); t += 3) {
    const auto &a = bind.vertices[bind.indices[t]];
    const auto &b = bind.vertices[bind.indices[t + 1]];
    const auto &c = bind.vertices[bind.indices[t + 2]];
    const int surface = surface_at(a.uv);
    const float low = std::min({a.position.y, b.position.y, c.position.y}) / h;
    const float high = std::max({a.position.y, b.position.y, c.position.y}) / h;
    if (surface == int(CharacterSurface::Face)) groups["face"].push_back(t);
    if (surface == int(CharacterSurface::HairCrop)) groups["scalp"].push_back(t);
    if (surface == int(CharacterSurface::Jacket)) {
      groups["jacket"].push_back(t);
      // The sewn collar root mixes teal and facing vertices. Classify the
      // entire triangle by any facing corner rather than mislabeling its
      // first teal corner as a separate underlying cloth layer.
      if (a.color.z > a.color.y * 1.2f || b.color.z > b.color.y * 1.2f ||
          c.color.z > c.color.y * 1.2f) groups["facing"].push_back(t);
      else groups["teal"].push_back(t);
    }
    if (surface == int(CharacterSurface::Pants)) groups["pants"].push_back(t);
    if (surface == int(CharacterSurface::Shirt) && low > .24f)
      groups["scarf"].push_back(t);
    if (surface == int(CharacterSurface::Skin) && high < 0)
      groups["hands"].push_back(t);
    if (surface == int(CharacterSurface::Leather) ||
        (surface == int(CharacterSurface::Rubber) && high < -.40f))
      groups["boots"].push_back(t);
    const bool lens = surface == int(CharacterSurface::Eyes) &&
                     (same(a.color, {.029f, .039f, .041f}) ||
                      same(a.color, Vec3{.029f, .039f, .041f} * .82f));
    if ((surface == int(CharacterSurface::Rubber) && low > .40f) || lens)
      groups["glasses"].push_back(t);
  }
  struct Pair { const char *a, *b; };
  const std::array<Pair, 10> pairs{{{"glasses", "face"}, {"glasses", "scalp"},
      {"scarf", "face"}, {"jacket", "face"}, {"scarf", "jacket"},
      {"facing", "teal"}, {"hands", "pants"}, {"jacket", "pants"},
      {"hands", "jacket"}, {"boots", "pants"}}};
  for (const auto &pair : pairs)
    require(!groups[pair.a].empty() && !groups[pair.b].empty(),
            std::string("Contact coverage has actual ") + pair.a + "/" + pair.b + " triangles");
  CassContacts contacts;
  Mesh mesh = bind;
  std::size_t failures = 0, poses = 0;
  const auto inspect = [&](const std::string &pose) {
    for (const auto &pair : pairs) {
      std::pair<std::size_t, std::size_t> first{};
      const auto count = contacts.count(mesh, h, groups[pair.a], groups[pair.b], first);
      if (count) {
        ++failures;
        std::cout << label(model) << " CONTACT " << pose << " " << pair.a << "/" << pair.b
                  << " = " << count << " first " << first.first << "/" << first.second << '\n';
      }
    }
    ++poses;
  };
  inspect("bind");
  if (all_poses) {
    for (int mode = 0; mode < modes; ++mode)
      for (int frame = 0; frame < 32; frame += 4) {
        apply_character_pose(model, sample_character_animation(
            model.rig, cass_sample(mode, frame, h)), mesh);
        inspect(std::to_string(mode) + ":" + std::to_string(frame));
      }
    for (int yaw : {-3, 0, 3}) for (int pitch : {-2, 0, 2}) {
      auto sample = cass_sample(3, 9, h);
      sample.attention_yaw = yaw * .30f;
      sample.attention_pitch = pitch * .2f;
      apply_character_pose(model, sample_character_animation(model.rig, sample), mesh);
      inspect("attention:" + std::to_string(yaw) + ":" + std::to_string(pitch));
    }
  }
  std::cout << label(model) << " contact poses " << poses << ", layer cases " << poses * pairs.size()
            << ", failures " << failures << std::endl;
  Mesh penetrated = bind;
  std::set<std::uint32_t> glass_vertices;
  for (auto t : groups["glasses"]) for (int k = 0; k < 3; ++k)
    glass_vertices.insert(bind.indices[t + k]);
  for (auto i : glass_vertices) penetrated.vertices[i].position.z -= .009f * h;
  std::pair<std::size_t, std::size_t> injected_first{};
  const auto injected = contacts.count(penetrated, h, groups["glasses"], groups["face"], injected_first);
  require(injected > 0, "Injected glasses/face penetration must be detected as a negative control");
  std::cout << label(model) << " injected glasses contact witnesses " << injected << std::endl;
  require(failures == 0, "Actual shipping mesh has proper interior layer crossings");
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
  const std::string mode = argc > 1 ? argv[1] : "";
  if (mode.empty() || mode == "--ordinary" || mode == "--ordinary-geometry") run("Cass dispatch/material contract", [&] { check_dispatch_and_materials(); });
  const bool diagnostic = !mode.empty() && mode != "--ordinary" && mode != "--ordinary-geometry";
  const std::vector<float> heights =
      diagnostic ? std::vector<float>{1.78f}
                 : std::vector<float>{1.35f, 1.78f, 2.10f};
  for (float height : heights)
    for (auto lod : {CharacterLod::Near, CharacterLod::Far}) {
      const auto p =
          make_character_profile(height, CharacterRole::Fence, "NpcFence", lod);
      const auto &m = p.model;
      if (argc == 5 && mode == "--dump-pose") {
        auto posed = m;
        const int sample_mode = std::stoi(argv[3]), sample_frame = std::stoi(argv[4]);
        const auto sample = cass_sample(sample_mode, sample_frame, height);
        const auto pose = sample_character_animation(m.rig, sample);
        apply_character_pose(m, pose, posed.bind_mesh);
        const auto prefix = std::string(argv[2]) + (lod == CharacterLod::Near ? "-near" : "-far");
        dump(posed, prefix + ".bin");
        std::ofstream json(prefix + ".pose.json");
        json << std::setprecision(9) << "{\n  \"source_fingerprint\": \"" << build_source_fingerprint()
             << "\",\n  \"identity\": \"NpcFence\",\n  \"mode\": " << sample_mode
             << ",\n  \"frame\": " << sample_frame << ",\n  \"height\": " << height
             << ",\n  \"phase_cycles\": " << sample.phase_cycles
             << ",\n  \"idle_time\": " << sample.idle_time << ",\n  \"speed\": " << sample.speed
             << ",\n  \"move_weight\": " << sample.move_weight
             << ",\n  \"crouch_weight\": " << sample.crouch_weight
             << ",\n  \"attention_yaw\": " << sample.attention_yaw
             << ",\n  \"attention_pitch\": " << sample.attention_pitch
             << ",\n  \"attention_weight\": " << sample.attention_weight
             << ",\n  \"matrix_layout\": \"column-major 4x4 float\",\n  \"joints\": [";
        for (std::size_t i = 0; i < character_bone_count; ++i) {
          if (i) json << ',';
          const auto &joint = m.rig.joints[i];
          json << "\n    {\"index\":" << i << ",\"parent\":" << joint.parent
               << ",\"bind_position\":[" << joint.bind_position.x << ',' << joint.bind_position.y << ',' << joint.bind_position.z
               << "],\"skin_matrix\":[";
          for (int k = 0; k < 16; ++k) { if (k) json << ','; json << pose.skin_matrices[i].m[k]; }
          json << "]}";
        }
        json << "\n  ]\n}\n";
        require(bool(json), "Exact pose matrix diagnostic write");
        continue;
      }
      if (argc == 3 && mode == "--dump") {
        dump(m, std::string(argv[2]) +
                    (lod == CharacterLod::Near ? "-near.bin" : "-far.bin"));
        continue;
      }
      if (mode == "--contacts" || mode == "--bind-contacts") {
        run(label(m) + " contacts", [&] { check_contacts(m, mode == "--contacts"); });
        continue;
      }
      if (mode == "--cap") {
        run(label(m) + " cap", [&] { check_crop_coverage(m); });
        continue;
      }
      std::cout << "CASE height " << height << " " << label(m) << std::endl;
      run(label(m) + " geometry", [&] { geometry(p); });
      run(label(m) + " source head", [&] { head(m); });
      run(label(m) + " source hands", [&] { hand_source_contract(m); });
      run(label(m) + " complete-mouth attention rigidity", [&] { mouth_attention_regression(m); });
      run(label(m) + " exterior-jaw attention rigidity", [&] { exterior_jaw_attention_regression(m); });
      run(label(m) + " sockets without lens masking",
          [&] { check_socket_fit(m); });
      run(label(m) + " opaque scalp", [&] { check_crop_coverage(m); });
      if (mode != "--geometry")
        run(label(m) + (mode == "--ordinary-geometry" ? " finite animation only (strict cache-seam assertion excluded, separately failed)" : " animation"),
            [&] { check_animation(m, mode != "--ordinary-geometry"); });
      if (mode.empty())
        run(label(m) + " contacts (including full crouch-walk stress)", [&] { check_contacts(m, true); });
      if (mode == "--ordinary" || mode == "--ordinary-geometry")
        run(label(m) + " ordinary contacts (full crouch-walk excluded and separately failed)", [&] { check_contacts(m, true, 5); });
    }
  return failures ? 1 : 0;
}
