#include "fury/visibility.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fury {
namespace {

bool finite(const Vec3& v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool valid_aabb(const Aabb& box) {
  return finite(box.center) && finite(box.half_extents) &&
         box.half_extents.x >= 0.f && box.half_extents.y >= 0.f &&
         box.half_extents.z >= 0.f;
}

// Round outwards, including the rounding error in the stored center. Using
// double intermediates avoids narrowing a box after rotation/large translation.
GeometryBounds bounds_from_limits(const double lo[3], const double hi[3]) {
  float center[3], half[3];
  for (int axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(lo[axis]) || !std::isfinite(hi[axis]) || lo[axis] > hi[axis])
      return {};
    center[axis] = static_cast<float>(lo[axis] * .5 + hi[axis] * .5);
    const double extent = std::max(double(center[axis]) - lo[axis],
                                   hi[axis] - double(center[axis]));
    half[axis] = static_cast<float>(extent);
    if (extent > 0.0)
      half[axis] = std::nextafter(half[axis], std::numeric_limits<float>::infinity());
    if (!std::isfinite(center[axis]) || !std::isfinite(half[axis])) return {};
  }
  return {{{center[0], center[1], center[2]}, {half[0], half[1], half[2]}}, true};
}

GeometryBounds mesh_bounds(const Mesh& mesh) {
  if (mesh.vertices.empty()) return {};
  double lo[3] = {std::numeric_limits<double>::infinity(),
                  std::numeric_limits<double>::infinity(),
                  std::numeric_limits<double>::infinity()};
  double hi[3] = {-lo[0], -lo[1], -lo[2]};
  for (const Vertex& vertex : mesh.vertices) {
    if (!finite(vertex.position)) return {};
    const double p[3] = {vertex.position.x, vertex.position.y, vertex.position.z};
    for (int axis = 0; axis < 3; ++axis) {
      lo[axis] = std::min(lo[axis], p[axis]);
      hi[axis] = std::max(hi[axis], p[axis]);
    }
  }
  return bounds_from_limits(lo, hi);
}

bool overlaps(const Aabb& a, const Aabb& b) {
  return std::fabs(double(a.center.x) - b.center.x) <= double(a.half_extents.x) + b.half_extents.x &&
         std::fabs(double(a.center.y) - b.center.y) <= double(a.half_extents.y) + b.half_extents.y &&
         std::fabs(double(a.center.z) - b.center.z) <= double(a.half_extents.z) + b.half_extents.z;
}

bool fully_behind(const Aabb& box, const Vec3& camera, const Vec3& forward) {
  if (!finite(camera) || !finite(forward)) return false;
  const double norm = std::sqrt(double(forward.x) * forward.x +
                                double(forward.y) * forward.y +
                                double(forward.z) * forward.z);
  if (norm < 1e-8) return false;
  const double center_d = (double(box.center.x) - camera.x) * forward.x +
                          (double(box.center.y) - camera.y) * forward.y +
                          (double(box.center.z) - camera.z) * forward.z;
  const double extent = double(box.half_extents.x) * std::fabs(forward.x) +
                        double(box.half_extents.y) * std::fabs(forward.y) +
                        double(box.half_extents.z) * std::fabs(forward.z);
  return center_d + extent < -.25 * norm;
}

}  // namespace

GeometryBounds transform_geometry_bounds(const GeometryBounds& local, const Mat4& model) {
  if (!local.valid || !valid_aabb(local.aabb)) return {};
  for (float value : model.m) if (!std::isfinite(value)) return {};
  // Entity transforms are affine. A projective matrix cannot use |M| * extent.
  if (model.at(0, 3) != 0.f || model.at(1, 3) != 0.f ||
      model.at(2, 3) != 0.f || model.at(3, 3) != 1.f) return {};
  const double center[3] = {local.aabb.center.x, local.aabb.center.y, local.aabb.center.z};
  const double half[3] = {local.aabb.half_extents.x, local.aabb.half_extents.y,
                        local.aabb.half_extents.z};
  double lo[3], hi[3];
  for (int row = 0; row < 3; ++row) {
    double c = model.at(3, row), extent = 0.0;
    for (int col = 0; col < 3; ++col) {
      c += double(model.at(col, row)) * center[col];
      extent += std::fabs(double(model.at(col, row))) * half[col];
    }
    lo[row] = c - extent;
    hi[row] = c + extent;
  }
  return bounds_from_limits(lo, hi);
}

GeometryBounds union_geometry_bounds(const GeometryBounds& a, const GeometryBounds& b) {
  if (!a.valid || !b.valid || !valid_aabb(a.aabb) || !valid_aabb(b.aabb)) return {};
  const double ac[3] = {a.aabb.center.x, a.aabb.center.y, a.aabb.center.z};
  const double ah[3] = {a.aabb.half_extents.x, a.aabb.half_extents.y, a.aabb.half_extents.z};
  const double bc[3] = {b.aabb.center.x, b.aabb.center.y, b.aabb.center.z};
  const double bh[3] = {b.aabb.half_extents.x, b.aabb.half_extents.y, b.aabb.half_extents.z};
  double lo[3], hi[3];
  for (int axis = 0; axis < 3; ++axis) {
    lo[axis] = std::min(ac[axis] - ah[axis], bc[axis] - bh[axis]);
    hi[axis] = std::max(ac[axis] + ah[axis], bc[axis] + bh[axis]);
  }
  return bounds_from_limits(lo, hi);
}

double geometry_distance_squared(const GeometryBounds& bounds, const Vec3& point) {
  if (!bounds.valid || !valid_aabb(bounds.aabb) || !finite(point)) return 0.0;
  const auto& b = bounds.aabb;
  const double dx = std::max(0.0, std::fabs(double(point.x) - b.center.x) - b.half_extents.x);
  const double dy = std::max(0.0, std::fabs(double(point.y) - b.center.y) - b.half_extents.y);
  const double dz = std::max(0.0, std::fabs(double(point.z) - b.center.z) - b.half_extents.z);
  return dx * dx + dy * dy + dz * dz;
}

void MeshBoundsCache::begin_frame() {
  for (auto& entry : m_entries) entry.second.seen = false;
}

GeometryBounds MeshBoundsCache::local_bounds(const Mesh& mesh) {
  const auto inserted = m_entries.try_emplace(mesh.geometry_identity);
  auto& entry = inserted.first->second;
  if (inserted.second || entry.revision != mesh.geometry_revision ||
      entry.vertex_count != mesh.vertices.size() || entry.index_count != mesh.indices.size()) {
    entry.bounds = mesh_bounds(mesh);
    entry.revision = mesh.geometry_revision;
    entry.vertex_count = mesh.vertices.size();
    entry.index_count = mesh.indices.size();
    ++m_rebuild_count;
  }
  entry.seen = true;
  return entry.bounds;
}

GeometryBounds MeshBoundsCache::world_bounds(const Mesh& mesh, const Mat4& model) {
  return transform_geometry_bounds(local_bounds(mesh), model);
}

void MeshBoundsCache::end_frame() {
  for (auto entry = m_entries.begin(); entry != m_entries.end();) {
    if (!entry->second.seen) entry = m_entries.erase(entry);
    else ++entry;
  }
}

WorldVisibilityResult evaluate_world_visibility(const WorldVisibilitySettings& settings,
                                                const GeometryBounds& primary,
                                                const GeometryBounds* lod, bool detail) {
  WorldVisibilityResult result;
  result.distance_squared = geometry_distance_squared(primary, settings.camera_position);
  const GeometryBounds bounds = lod ? union_geometry_bounds(primary, *lod) : primary;
  const double distance_squared = geometry_distance_squared(bounds, settings.camera_position);
  if (settings.cull_distance > 0.f &&
      distance_squared > double(settings.cull_distance) * settings.cull_distance) {
    result.visible = false;
    return result;
  }
  if (bounds.valid && valid_aabb(bounds.aabb)) {
    // Keep geometry that crosses the indoor sector, even if its origin is outside.
    if (settings.sector_hide && valid_aabb(settings.sector_focus) &&
        !overlaps(bounds.aabb, settings.sector_focus)) {
      result.visible = false;
      return result;
    }
    // Preserve the near-camera safety margin and ray-backend shadow casters.
    if (settings.cull_behind_camera && distance_squared > 4.0 &&
        fully_behind(bounds.aabb, settings.camera_position, settings.camera_forward)) {
      result.visible = false;
      return result;
    }
  }
  const double mid = settings.lod_mid_distance > 0.f ? settings.lod_mid_distance :
                     settings.cull_distance > 0.f ? double(settings.cull_distance) * .5 : 0.0;
  if (mid > 0.0 && result.distance_squared > mid * mid) {
    result.use_lod = lod != nullptr;
    if (!lod && detail) result.visible = false;
  }
  return result;
}

}  // namespace fury
