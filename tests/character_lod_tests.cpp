#include "fury/application.hpp"
#include "fury/log.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace fury;
using Image = std::vector<std::uint8_t>;

void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

Mesh proxy_mesh(float z = 0.f) {
  Mesh mesh;
  mesh.vertices = {{{-1, -1, z}, {0, 0, 1}, {.05f, 1.f, .05f}},
                   {{1, -1, z}, {0, 0, 1}, {.05f, 1.f, .05f}},
                   {{1, 1, z}, {0, 0, 1}, {.05f, 1.f, .05f}},
                   {{-1, 1, z}, {0, 0, 1}, {.05f, 1.f, .05f}}};
  mesh.indices = {0, 1, 2, 0, 2, 3};
  return mesh;
}

AppConfig config(RenderBackendKind backend) {
  AppConfig result;
  result.window.title = "Application character LOD regression";
  result.window.width = 64;
  result.window.height = 48;
  result.window.resizable = false;
  result.prefer_opengl = false;
  result.preferred_backend = backend;
  result.capture_mouse = false;
  result.enable_collision = false;
  result.log_fps = false;
  result.max_frames = 1;
  result.fixed_timestep = 1.f / 60.f;
  result.freeze_render_time = true;
  result.cull_distance = 90.f;
  result.lod_mid_distance = 10.f;
  return result;
}

// Exercise the actual Application::draw_scene path, including cache reuse,
// backend submission and framebuffer rendering. No private hooks or replacement
// visibility implementation: CPU-ray instance/triangle statistics tell us which
// meshes reached the backend, even for offscreen shadow casters.
struct Fixture {
  Application app;
  Mesh* primary{};
  Mesh* proxy{};
  Image image;

  explicit Fixture(RenderBackendKind backend = RenderBackendKind::CpuRayTracing)
      : app(config(backend)) {
    app.camera().position = {};
    require(app.init(), "Initialize real application fixture");
    require(app.renderer().backend_kind() == backend, "Requested backend is active without fallback");
    primary = app.scene().add_mesh(make_box({2, 2, 2}, {1.f, .05f, .05f}));
    proxy = app.scene().add_mesh(proxy_mesh());
    Lighting light;
    light.ambient = {.6f, .6f, .6f};
    light.sun_intensity = .8f;
    light.ao_strength = 0.f;
    light.fog_start = 150.f;
    light.fog_end = 200.f;
    light.enable_shadows = light.enable_reflections = light.enable_bloom = false;
    app.renderer().set_lighting(light);
    if (backend == RenderBackendKind::CpuRayTracing) {
      auto rendering = app.renderer().settings();
      rendering.trace_mode = TraceMode::RayTraced;
      rendering.samples_per_pixel = 1;
      rendering.max_bounces = 1;
      rendering.accumulate = rendering.denoise = rendering.vsync = false;
      require(app.renderer().configure(rendering), "Configure deterministic CPU-ray fixture");
    }
    app.on_hud = [&] {
      int width{}, height{};
      require(app.renderer().read_rgb_framebuffer(image, width, height), "Read actual application frame");
      require(width == 64 && height == 48 && image.size() == 64 * 48 * 3,
              "Complete application framebuffer dimensions");
    };
  }

  Entity& add(float z = -8.f, float distance = 0.f, bool with_proxy = true, bool detail = false) {
    Entity entity;
    entity.mesh = primary;
    entity.lod_mesh = with_proxy ? proxy : nullptr;
    entity.lod_distance = distance;
    entity.detail = detail;
    entity.transform.position.z = z;
    entity.material.double_sided = true;
    return app.scene().add_entity(entity);
  }

  void draw(unsigned instances, unsigned triangles, const std::string& label) {
    require(app.run() == 0, label + ": bounded real application run succeeds");
    if (app.renderer().backend_kind() != RenderBackendKind::CpuRayTracing) return;
    const auto statistics = app.renderer().statistics();
    require(statistics.validation_errors == 0 && statistics.software_ray_tracing &&
                !statistics.hardware_ray_tracing && statistics.rays_traced > 0,
            label + ": valid CPU-ray frame was rendered");
    require(statistics.instance_count == instances && statistics.triangle_count == triangles,
            label + ": expected " + std::to_string(instances) + " instances / " +
                std::to_string(triangles) + " triangles, got " +
                std::to_string(statistics.instance_count) + " / " +
                std::to_string(statistics.triangle_count));
  }
};

void threshold_and_fallback() {
  Fixture fixture;
  auto& entity = fixture.add(-8.f, 4.f);
  fixture.draw(1, 2, "Entity override selects the cheaper proxy before the global threshold");
  for (const float z : {-4.99f, -5.f, -5.01f, -4.99f}) {
    entity.transform.position.z = z;
    fixture.draw(1, z < -5.f ? 2 : 12,
                 "Nearest primary-bound threshold is strict and reversible");
  }
  for (const float invalid : {0.f, -4.f, std::numeric_limits<float>::quiet_NaN(),
                             std::numeric_limits<float>::infinity(),
                             -std::numeric_limits<float>::infinity()}) {
    entity.lod_distance = invalid;
    entity.transform.position.z = -8.f;
    fixture.draw(1, 12, "Nonpositive/nonfinite override inherits explicit global near choice");
    entity.transform.position.z = -15.f;
    fixture.draw(1, 2, "Nonpositive/nonfinite override inherits explicit global far choice");
  }
  fixture.app.config().lod_mid_distance = 0.f;
  entity.lod_distance = 0.f;
  entity.transform.position.z = -46.f;
  fixture.draw(1, 12, "Default entity inherits half-cull threshold, including equality");
  entity.transform.position.z = -46.01f;
  fixture.draw(1, 2, "Default entity switches beyond half-cull threshold");
  fixture.app.config().cull_distance = 0.f;
  entity.lod_distance = 4.f;
  fixture.draw(1, 2, "Explicit entity LOD remains active with global distance limits disabled");
  entity.lod_distance = std::numeric_limits<float>::quiet_NaN();
  fixture.draw(1, 12, "Invalid override inherits disabled global LOD");
  entity.lod_distance = std::numeric_limits<float>::max();
  fixture.draw(1, 12, "Largest finite positive threshold remains a valid near choice");
  fixture.app.config().cull_distance = 20.f;
  fixture.draw(0, 0, "Large LOD override never extends world cull distance");
}

void transformed_geometry_and_revisions() {
  Fixture fixture;
  auto& entity = fixture.add(-12.f, 5.f);
  entity.transform.scale = {1.f, 2.f, 8.f};
  fixture.draw(1, 12, "Long scaled primary reaches near range despite distant origin and proxy");
  entity.transform.rotation_euler.y = radians(90.f);
  entity.transform.scale = {-8.f, 2.f, .5f};
  fixture.draw(1, 12, "Rotated mirrored nonuniform bounds select near geometry");
  entity.transform.scale.x = -3.f;
  fixture.draw(1, 2, "Changed transform crosses entity threshold without changing mesh");
  entity.transform = {};
  entity.transform.position = {-20.f, 0.f, -8.f};
  entity.lod_distance = 10.f;
  for (auto& vertex : fixture.primary->vertices) vertex.position.x += 20.f;
  fixture.primary->mark_dirty();
  fixture.draw(1, 12, "Offset geometry uses transformed extents rather than distant entity origin");
  entity.transform.position.x = 0.f;
  for (auto& vertex : fixture.primary->vertices) vertex.position.x -= 20.f;
  fixture.primary->mark_dirty();
  entity.transform.position.z = -12.f;
  entity.lod_distance = 5.f;
  fixture.draw(1, 2, "Undeformed mesh starts beyond entity threshold");
  for (auto& vertex : fixture.primary->vertices) vertex.position.z += 6.f;
  fixture.primary->mark_dirty();
  fixture.draw(1, 12, "Same-count animated revision refreshes bounds and changes real mesh selection");
}

void conservative_visibility() {
  Fixture fixture;
  auto& entity = fixture.add(-20.f, 4.f);
  fixture.app.config().cull_distance = 10.f;
  fixture.draw(0, 0, "Wholly distant primary and proxy remain distance-culled");
  *fixture.proxy = proxy_mesh(18.f);
  fixture.draw(1, 2, "Near proxy bounds preserve visibility while distant primary selects LOD");
  // The proxy is only 2m away, but cannot feed back into the 4m LOD choice.
  fixture.app.config().sector_hide = true;
  fixture.app.config().sector_focus = {{0.f, 0.f, -2.f}, {2.f, 2.f, 1.f}};
  fixture.draw(1, 2, "Primary/proxy union intersecting sector remains visible");
  fixture.app.config().sector_focus.center.x = 50.f;
  fixture.draw(0, 0, "LOD override never bypasses explicit sector exclusion");
  fixture.app.config().sector_hide = false;
  entity.visible = false;
  fixture.draw(0, 0, "Explicit entity invisibility remains authoritative");
  entity.visible = true;
  entity.mesh = nullptr;
  fixture.draw(0, 0, "Entity with no primary mesh is not submitted");
}

bool same_box(const Aabb& a, const Aabb& b) {
  return a.center.x == b.center.x && a.center.y == b.center.y && a.center.z == b.center.z &&
         a.half_extents.x == b.half_extents.x && a.half_extents.y == b.half_extents.y &&
         a.half_extents.z == b.half_extents.z;
}

void ordinary_neighbors_and_colliders() {
  Fixture fixture;
  fixture.add();                         // ordinary near, before the override
  fixture.add(-8.f, 4.f);               // overridden far
  fixture.add();                         // same primary/proxy identities, ordinary near
  fixture.add(-8.f, 1.f, false, true);  // detail-only: preserve global near threshold
  fixture.add(-15.f, 100.f, false, true);  // detail-only: preserve global far hiding
  fixture.add(-15.f, 100.f);            // overridden near
  fixture.add(-15.f);                   // ordinary far, after long override
  auto& solid = fixture.app.scene().entities()[1];
  solid.solid = true;
  solid.collider = {{.2f, .3f, .4f}, {.25f, .6f, .3f}};
  const auto before = fixture.app.scene().collect_solids();
  fixture.draw(6, 52, "Overrides leave preceding/following ordinary entities and detail hiding unchanged");
  require(fixture.app.config().lod_mid_distance == 10.f && fixture.app.config().cull_distance == 90.f,
          "Entity overrides cannot mutate application settings");
  const auto after = fixture.app.scene().collect_solids();
  require(before.size() == 1 && after.size() == 1 && same_box(before.front(), after.front()),
          "Drawing a different character LOD leaves gameplay collider collection unchanged");
  auto& entities = fixture.app.scene().entities();
  std::reverse(entities.begin(), entities.end());
  fixture.draw(6, 52, "Reversing entity traversal cannot leak either short or long overrides");
  for (auto& entity : entities) entity.lod_distance = 0.f;
  fixture.draw(6, 52, "Removing all overrides restores ordinary global behavior");
  for (auto& entity : entities) entity.lod_mesh = nullptr;
  fixture.draw(6, 72, "Ordinary non-detail entities without proxies retain their primary meshes");
}

void shadow_caster_retention() {
  Fixture fixture;
  fixture.add(8.f, 4.f);
  fixture.add(8.f);
  fixture.add(15.f);
  fixture.add(8.f, 1.f, false, true);
  fixture.draw(4, 28, "CPU ray retains behind-camera overridden and ordinary shadow casters");
  fixture.app.config().cull_distance = 10.f;
  fixture.draw(3, 26, "Offscreen caster retention still respects intentional distance range");
  fixture.app.config().sector_hide = true;
  fixture.app.config().sector_focus = {{0.f, 0.f, -8.f}, {2.f, 2.f, 2.f}};
  fixture.draw(0, 0, "Offscreen caster retention still respects sector limits");
}

void actual_lod_pixels(RenderBackendKind backend) {
  Fixture fixture(backend);
  auto& entity = fixture.add(-8.f, 4.f);
  fixture.draw(1, 2, "Render actual entity-selected proxy");
  const Image far = fixture.image;
  fixture.app.on_render = [&] {
    fixture.app.renderer().draw_mesh(*fixture.proxy, entity.transform.matrix(), entity.material, 1);
  };
  fixture.draw(1, 2, "Render explicit proxy reference");
  require(fixture.image == far, "Application-selected proxy exactly matches direct proxy framebuffer");
  fixture.app.on_render = {};
  entity.lod_distance = 10.f;
  fixture.draw(1, 12, "Render actual entity-selected primary");
  const Image near = fixture.image;
  require(near != far, "The two LOD meshes produce visibly different real pixels");
  fixture.app.on_render = [&] {
    fixture.app.renderer().draw_mesh(*fixture.primary, entity.transform.matrix(), entity.material, 1);
  };
  fixture.draw(1, 12, "Render explicit primary reference");
  require(fixture.image == near, "Application-selected primary exactly matches direct primary framebuffer");
}
}  // namespace

int main(int, char**) {
  try {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    SDL_setenv("FURY_CPU_THREADS", "2", 1);
    SDL_setenv("FURY_SURFACE_DETAIL", "0", 1);
    Log::set_level(LogLevel::Error);
    require(Entity{}.lod_distance == 0.f, "Existing entities inherit global LOD by default");
    threshold_and_fallback();
    transformed_geometry_and_revisions();
    conservative_visibility();
    ordinary_neighbors_and_colliders();
    shadow_caster_retention();
    actual_lod_pixels(RenderBackendKind::Software);
    actual_lod_pixels(RenderBackendKind::CpuRayTracing);
    std::cout << "Character LOD application checks passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Character LOD application check failed: " << error.what() << '\n';
    return 1;
  }
}
