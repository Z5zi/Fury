#pragma once

#include "fury/collision.hpp"
#include "fury/mesh.hpp"

#include <cstddef>
#include <cstdint>
#include <map>

namespace fury {

/// Bounds for rendering only. Unknown/invalid bounds fail open during culling.
/// These are independent of the deliberately simplified gameplay colliders.
struct GeometryBounds {
  Aabb aabb{{0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}};
  bool valid{false};
};

/// Affine transformation of a local AABB. Supports rotation, mirrored and
/// nonuniform scale, and shear; no inverse or per-frame vertex scan is needed.
GeometryBounds transform_geometry_bounds(const GeometryBounds& local,
                                         const Mat4& model);
GeometryBounds union_geometry_bounds(const GeometryBounds& a,
                                     const GeometryBounds& b);
double geometry_distance_squared(const GeometryBounds& bounds, const Vec3& point);

/// One entry per referenced mesh identity, replaced when geometry_revision or
/// topology counts change. Bracket each draw_scene pass with begin/end_frame:
/// end_frame removes all entries not referenced in that pass. No mesh pointers
/// are retained, and retired meshes/revisions cannot accumulate in this cache.
class MeshBoundsCache {
 public:
  void begin_frame();
  GeometryBounds local_bounds(const Mesh& mesh);
  GeometryBounds world_bounds(const Mesh& mesh, const Mat4& model);
  void end_frame();
  std::size_t size() const { return m_entries.size(); }
  std::uint64_t rebuild_count() const { return m_rebuild_count; }

 private:
  struct Entry {
    GeometryBounds bounds;
    std::uint64_t revision{0};
    std::size_t vertex_count{0};
    std::size_t index_count{0};
    bool seen{false};
  };
  std::map<std::uint64_t, Entry> m_entries;
  std::uint64_t m_rebuild_count{0};
};

struct WorldVisibilitySettings {
  Vec3 camera_position{};
  Vec3 camera_forward{0.f, 0.f, -1.f};
  float cull_distance{0.f};  // <= 0 disables distance culling
  float lod_mid_distance{0.f};  // <= 0 uses half the positive cull distance
  bool cull_behind_camera{true};  // false for CPU ray / DXR shadow casters
  bool sector_hide{false};
  Aabb sector_focus{};
};

struct WorldVisibilityResult {
  bool visible{true};
  bool use_lod{false};
  double distance_squared{0.0};  // camera to primary geometry AABB, not origin
};

/// Distance and LOD use the nearest extent of the geometry. Visibility uses
/// the union of primary/LOD bounds so a larger proxy cannot disappear early;
/// LOD selection uses primary bounds to avoid feedback from proxy dimensions.
WorldVisibilityResult evaluate_world_visibility(
    const WorldVisibilitySettings& settings, const GeometryBounds& primary,
    const GeometryBounds* lod = nullptr, bool detail = false);

}  // namespace fury
