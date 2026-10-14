#include "fury/visibility.hpp"
#include "fury/transform.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace fury;
namespace {

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

bool close(double a, double b, double tolerance = 1e-4) {
  return std::fabs(a - b) <= tolerance;
}

Mesh box_vertices(Vec3 lo, Vec3 hi) {
  Mesh mesh;
  for (int corner = 0; corner < 8; ++corner)
    mesh.vertices.push_back({{corner & 1 ? hi.x : lo.x,
                              corner & 2 ? hi.y : lo.y,
                              corner & 4 ? hi.z : lo.z}});
  return mesh;
}

bool contains(const GeometryBounds& bounds, const Vec3& p) {
  const auto& b = bounds.aabb;
  return bounds.valid && std::fabs(double(p.x) - b.center.x) <= b.half_extents.x + 1e-4 &&
         std::fabs(double(p.y) - b.center.y) <= b.half_extents.y + 1e-4 &&
         std::fabs(double(p.z) - b.center.z) <= b.half_extents.z + 1e-4;
}

void distant_origin_ground_and_sector() {
  MeshBoundsCache cache;
  const Mesh ground = box_vertices({-160, 0, -130}, {160, 0, 130});
  cache.begin_frame();
  const auto bounds = cache.world_bounds(ground, Mat4::identity());
  WorldVisibilitySettings settings;
  settings.camera_position = {146, 1.7f, 110};
  settings.camera_forward = {1, 0, 0};
  settings.cull_distance = 90;
  // The old origin-distance cull and 0.5m collider-plane cull both rejected this.
  require(length(settings.camera_position) > settings.cull_distance,
          "Regression fixture origin is beyond the normal 90m cull distance");
  auto result = evaluate_world_visibility(settings, bounds, nullptr, true);
  require(result.visible && !result.use_lod && close(result.distance_squared, 1.7 * 1.7),
          "StreetGrid survives distant origin, behind-origin camera and detail LOD");
  settings.sector_hide = true;
  settings.sector_focus = {{146, 1.7f, 110}, {4, 2, 4}};
  require(evaluate_world_visibility(settings, bounds).visible,
          "Large ground intersecting an indoor sector stays visible outside its origin");
  const auto outside = cache.world_bounds(box_vertices({-1, 0, -1}, {1, 2, 1}),
                                          translate({120, 0, 110}));
  require(!evaluate_world_visibility(settings, outside).visible,
          "Sector hide still excludes fully exterior props inside distance range");
  settings.cull_behind_camera = false;
  require(!evaluate_world_visibility(settings, outside).visible,
          "Ray backends retain the intended explicit sector exclusion");
  settings.sector_hide = false;
  settings.camera_position = {300, 1.7f, 0};
  settings.camera_forward = {-1, 0, 0};
  require(!evaluate_world_visibility(settings, bounds).visible,
          "Ground is rejected when all geometry really is beyond cull distance");
  cache.end_frame();
}

void transformed_extents() {
  MeshBoundsCache cache;
  const Mesh mesh = box_vertices({7, -3, 11}, {19, 5, 13});
  cache.begin_frame();
  const auto local = cache.local_bounds(mesh);
  for (const Vec3 scale_value : {Vec3{-2, 3, .25f}, Vec3{.3f, -4, -2}, Vec3{0, 2, 1}}) {
    Transform transform;
    transform.position = {140, 21, -115};
    transform.rotation_euler = {.31f, 1.12f, -.67f};
    transform.scale = scale_value;
    const Mat4 model = transform.matrix();
    const auto bounds = cache.world_bounds(mesh, model);
    require(bounds.valid && bounds.aabb.half_extents.x >= 0 &&
            bounds.aabb.half_extents.y >= 0 && bounds.aabb.half_extents.z >= 0,
            "Mirroring, nonuniform and zero scale yield valid nonnegative bounds");
    for (const Vertex& vertex : mesh.vertices)
      require(contains(bounds, transform_point(model, vertex.position)),
              "Every rotated, mirrored, nonuniform transformed vertex remains inside bounds");
    const Vec3 expected_center = transform_point(model, local.aabb.center);
    require(close(bounds.aabb.center.x, expected_center.x) &&
            close(bounds.aabb.center.y, expected_center.y) &&
            close(bounds.aabb.center.z, expected_center.z),
            "Local geometry offset is rotated/scaled before world translation");
    WorldVisibilitySettings settings;
    settings.camera_position = transform_point(model, mesh.vertices.front().position);
    settings.cull_distance = .5f;
    require(evaluate_world_visibility(settings, bounds, nullptr, true).visible,
            "Camera on transformed geometry survives cull and LOD despite distant origin");
  }
  Mat4 shear = Mat4::identity(); shear.at(1, 0) = .75f;
  const auto sheared = cache.world_bounds(mesh, shear);
  for (const Vertex& vertex : mesh.vertices)
    require(contains(sheared, transform_point(shear, vertex.position)),
            "Affine shear remains conservative");
  require(cache.rebuild_count() == 1, "Changing entity transforms never rescans mesh vertices");
  cache.end_frame();
}

void revisions_and_cache_lifecycle() {
  MeshBoundsCache cache;
  Mesh mesh = box_vertices({-1, -1, -1}, {1, 1, 1});
  cache.begin_frame();
  cache.local_bounds(mesh);
  for (int instance = 0; instance < 100; ++instance)
    cache.world_bounds(mesh, translate({float(instance), 0, 0}));
  require(cache.rebuild_count() == 1 && cache.size() == 1,
          "Repeated instances and unchanged geometry scan once");
  cache.end_frame();
  cache.begin_frame();
  cache.local_bounds(mesh);
  require(cache.rebuild_count() == 1, "Unchanged geometry stays cached across frames");
  for (Vertex& vertex : mesh.vertices) vertex.position.x += 120;
  mesh.mark_dirty();
  const auto moved = cache.world_bounds(mesh, Mat4::identity());
  require(close(moved.aabb.center.x, 120) && cache.rebuild_count() == 2 && cache.size() == 1,
          "mark_dirty refreshes same-count animated geometry without accumulating revisions");
  WorldVisibilitySettings settings;
  settings.camera_position = {120, 0, 0}; settings.cull_distance = 5;
  require(evaluate_world_visibility(settings, moved).visible,
          "Updated revision immediately changes the actual visibility decision");
  mesh.vertices.push_back({{200, 0, 0}});
  require(contains(cache.local_bounds(mesh), {200, 0, 0}) && cache.rebuild_count() == 3,
          "Topology count is an additional guard for resized geometry");
  const auto previous_identity = mesh.geometry_identity;
  mesh = box_vertices({-30, -2, -2}, {-20, 2, 2});
  require(mesh.geometry_identity != previous_identity, "Replacement mesh has a new identity");
  require(close(cache.local_bounds(mesh).aabb.center.x, -25),
          "Reused Mesh address cannot inherit previous identity bounds");
  cache.end_frame();
  cache.begin_frame();
  cache.local_bounds(mesh);
  cache.end_frame();
  require(cache.size() == 1, "Unreferenced old identity is evicted on the next completed pass");
  for (int frame = 0; frame < 50; ++frame) {
    cache.begin_frame();
    Mesh transient = box_vertices({float(frame), 0, 0}, {float(frame + 1), 1, 1});
    cache.local_bounds(transient);
    cache.end_frame();
    require(cache.size() == 1, "Scene replacement cannot grow the persistent cache");
  }
  cache.begin_frame(); cache.end_frame();
  require(cache.size() == 0, "Empty/invisible scene releases cached bounds deterministically");
}

void lod_distance_and_proxy_bounds() {
  MeshBoundsCache cache;
  cache.begin_frame();
  const auto primary = cache.world_bounds(box_vertices({-2, -2, 75}, {2, 2, 125}), Mat4::identity());
  const auto lod = cache.world_bounds(box_vertices({-1, -1, 99}, {1, 1, 101}), Mat4::identity());
  WorldVisibilitySettings settings;
  settings.camera_forward = {0, 0, 1};
  settings.cull_distance = 200; settings.lod_mid_distance = 80;
  require(evaluate_world_visibility(settings, primary, nullptr, true).visible,
          "Large detail batched geometry is retained when a near extent is inside LOD range");
  require(!evaluate_world_visibility(settings, primary, &lod, true).use_lod,
          "LOD distance measures primary geometry extent instead of origin or proxy extent");
  settings.lod_mid_distance = 70;
  auto result = evaluate_world_visibility(settings, primary, &lod, true);
  require(result.visible && result.use_lod, "Wholly distant primary geometry selects its LOD");
  require(!evaluate_world_visibility(settings, primary, nullptr, true).visible,
          "Wholly distant detail still disappears when no LOD mesh exists");
  settings.lod_mid_distance = 0;
  require(!evaluate_world_visibility(settings, primary, &lod).use_lod,
          "Default LOD range is half the positive cull range");
  settings.cull_distance = 140;
  require(evaluate_world_visibility(settings, primary, &lod).use_lod,
          "Default LOD threshold changes with normal quality cull distance");
  settings.cull_distance = 0;
  result = evaluate_world_visibility(settings, primary, nullptr, true);
  require(result.visible && !result.use_lod, "Zero cull and implicit LOD disable both thresholds");
  settings.lod_mid_distance = 70;
  require(evaluate_world_visibility(settings, primary, &lod).use_lod,
          "Explicit LOD continues to work with distance culling disabled");
  settings.cull_distance = 50;
  const auto large_lod = cache.world_bounds(box_vertices({-2, -2, 40}, {2, 2, 125}), Mat4::identity());
  result = evaluate_world_visibility(settings, primary, &large_lod);
  require(result.visible && result.use_lod,
          "Larger LOD proxy crossing cull range is protected by union bounds");
  cache.end_frame();
}

void camera_plane_and_shadow_casters() {
  MeshBoundsCache cache;
  cache.begin_frame();
  const auto behind = cache.world_bounds(box_vertices({-1, -1, -11}, {1, 1, -9}), Mat4::identity());
  const auto crossing = cache.world_bounds(box_vertices({-1, -1, -100}, {1, 1, 3}), Mat4::identity());
  WorldVisibilitySettings settings;
  settings.camera_forward = {0, 0, 1}; settings.cull_distance = 90;
  require(!evaluate_world_visibility(settings, behind).visible,
          "Raster rejects geometry wholly behind the camera beyond near safety margin");
  require(evaluate_world_visibility(settings, crossing).visible,
          "Geometry crossing the camera plane stays visible despite a behind-camera center");
  const auto near = cache.world_bounds(box_vertices({-.1f, -.1f, -1.5f}, {.1f, .1f, -1}), Mat4::identity());
  require(evaluate_world_visibility(settings, near).visible, "Existing 2m camera-plane safety margin remains");
  const auto barely_behind = cache.world_bounds(box_vertices({-.1f, -.1f, -3}, {.1f, .1f, -.1f}),
                                               translate({3, 0, 0}));
  require(evaluate_world_visibility(settings, barely_behind).visible,
          "Existing quarter-meter camera-plane tolerance remains");
  settings.cull_behind_camera = false;
  require(evaluate_world_visibility(settings, behind).visible,
          "CPU ray/DXR retain offscreen shadow casters behind the camera");
  const auto far = cache.world_bounds(box_vertices({-1, -1, -201}, {1, 1, -199}), Mat4::identity());
  require(!evaluate_world_visibility(settings, far).visible,
          "Ray shadow preservation does not disable intentional distance range");
  cache.end_frame();
}

void invalid_geometry_fails_open() {
  MeshBoundsCache cache;
  cache.begin_frame();
  Mesh empty;
  require(!cache.local_bounds(empty).valid, "Empty geometry has unknown bounds");
  Mesh invalid = box_vertices({0, 0, 0}, {1, 1, 1});
  invalid.vertices[0].position.x = std::numeric_limits<float>::quiet_NaN();
  require(!cache.local_bounds(invalid).valid, "Nonfinite vertex makes the whole bound unknown");
  WorldVisibilitySettings settings;
  settings.cull_distance = 1; settings.lod_mid_distance = .5f;
  settings.camera_position = {100, 100, 100}; settings.sector_hide = true;
  require(evaluate_world_visibility(settings, {}, nullptr, true).visible,
          "Unknown geometry fails open for distance, sector, plane and detail thresholds");
  const auto valid = cache.world_bounds(box_vertices({-1, -1, -1}, {1, 1, 1}), Mat4::identity());
  require(!transform_geometry_bounds(valid, perspective(radians(60), 1, .1f, 100)).valid,
          "Projective transforms are not misinterpreted as affine bounds");
  Mat4 bad = Mat4::identity(); bad.at(0, 0) = std::numeric_limits<float>::infinity();
  require(!transform_geometry_bounds(valid, bad).valid, "Nonfinite transform fails open");
  cache.end_frame();
}

}  // namespace

int main() {
  try {
    distant_origin_ground_and_sector();
    transformed_extents();
    revisions_and_cache_lifecycle();
    lod_distance_and_proxy_bounds();
    camera_plane_and_shadow_casters();
    invalid_geometry_fails_open();
    std::cout << "World visibility, transformed geometry, LOD and cache checks passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
