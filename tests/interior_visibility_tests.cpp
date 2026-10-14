#include "fury/interior.hpp"
#include "fury/visibility.hpp"
#include "../apps/vaultline/npc_roster.hpp"
#include "../apps/vaultline/npc_presentation.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace fury;
int checks = 0;
void require(bool condition, const char* message) {
  ++checks;
  if (!condition) throw std::runtime_error(message);
}
bool close(float a, float b) { return std::fabs(a - b) < 2.e-5f; }
bool same(Vec3 a, Vec3 b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}
bool same_box(const Aabb& a, const Aabb& b) {
  return same(a.center, b.center) && same(a.half_extents, b.half_extents);
}
Aabb old_sector(const InteriorZone& zone) {
  return {zone.center, {zone.half_extents.x * 1.1f,
      zone.half_extents.y * 1.15f, zone.half_extents.z * 1.1f}};
}
InteriorZone zone(Vec3 center, Vec3 half, const char* tag = "room") {
  InteriorZone result;
  result.center = center;
  result.half_extents = half;
  result.tag = tag;
  return result;
}
bool same_zone_lighting(const Lighting& a, const Lighting& b) {
  if (!same(a.ambient, b.ambient) || a.sun_intensity != b.sun_intensity ||
      a.shadow_strength != b.shadow_strength || a.point_light_count != b.point_light_count)
    return false;
  for (int i = 0; i < Lighting::kMaxPointLights; ++i) {
    const auto& x = a.point_lights[i];
    const auto& y = b.point_lights[i];
    if (!same(x.position, y.position) || !same(x.color, y.color) ||
        x.intensity != y.intensity || x.radius != y.radius) return false;
  }
  return true;
}

void catalog_and_lighting() {
  const auto catalog = make_harbor_interiors();
  const auto* desk = catalog.zone_at({-2.2f, 2.14f, -8.285f});
  require(desk == &catalog.zones[1], "Security desk remains the selected lighting sub-zone");
  const auto bank = catalog.visibility_sector(*desk);
  require(close(bank.center.x, -.1f) && close(bank.center.y, 2.8f) &&
      close(bank.center.z, -10.8f) && close(bank.half_extents.x, 9.13f) &&
      close(bank.half_extents.y, 3.91f) && close(bank.half_extents.z, 8.58f),
      "Bank sector includes the connected vault, desk and lobby with existing margins");
  for (std::size_t i = 0; i < catalog.zones.size(); ++i) {
    const auto& selected = catalog.zones[i];
    Lighting before;
    InteriorCatalog::apply_zone_lighting(before, selected);
    const auto sector = catalog.visibility_sector(selected);
    Lighting after;
    InteriorCatalog::apply_zone_lighting(after, selected);
    require(same_zone_lighting(before, after), "Visibility calculation leaves local lighting unchanged");
    if (i < 3) require(same_box(sector, bank), "Every bank sub-zone yields the same complete room");
    else require(same_box(sector, old_sector(selected)),
        "Jewelry, loft and depot retain their exact original single-zone sectors");
  }
  require(catalog.zone_at({-2.2f, 2.14f, -8.285f}) == desk,
      "Visibility calculation does not alter first-match lighting selection");
}

void connectivity() {
  InteriorCatalog catalog;
  // Reverse the connecting chain: discovering the far face-adjacent zone
  // requires traversing its neighbor rather than one pass against the seed.
  catalog.zones = {zone({0, 0, 0}, {1, 1, 1}), zone({4, 0, 0}, {1, 1, 1}),
      zone({2, 0, 0}, {1, 1, 1}), zone({6.01f, 0, 0}, {1, 1, 1}),
      zone({6, 0, 0}, {1, 1, 1}, "other"), zone({100, 0, 0}, {1, 1, 1})};
  const auto joined = catalog.visibility_sector(catalog.zones[0]);
  require(close(joined.center.x, 2.f) && close(joined.half_extents.x, 3.3f),
      "Same-tag face-adjacent zones connect transitively without bridging an authored gap");
  require(same_box(catalog.visibility_sector(catalog.zones[5]), old_sector(catalog.zones[5])),
      "Disconnected room with the same tag stays separate");
  require(same_box(catalog.visibility_sector(catalog.zones[4]), old_sector(catalog.zones[4])),
      "A touching room with a different tag stays separate");

  // An L-shaped component has an empty corner inside its enclosing box. A
  // separate component there, with a tail outside the box, must not join it.
  catalog.zones = {zone({0, 0, 0}, {1, 1, 1}), zone({2, 0, 0}, {1, 1, 1}),
      zone({2, 0, 2}, {1, 1, 1}), zone({-.5f, 0, 2.5f}, {.2f, .2f, .2f}),
      zone({-2, 0, 2.5f}, {1.5f, .2f, .2f})};
  const auto elbow = catalog.visibility_sector(catalog.zones[0]);
  require(close(elbow.center.x, 1.f) && close(elbow.center.z, 1.f) &&
      close(elbow.half_extents.x, 2.2f) && close(elbow.half_extents.z, 2.2f),
      "Connectivity uses original boxes, not the accumulating union");

  catalog.zones = {zone({0, 0, 0}, {1, 1, 1}), zone({2, 2, 0}, {1, 1, 1}),
      zone({2, 2, 2}, {1, 1, 1})};
  require(same_box(catalog.visibility_sector(catalog.zones[0]), old_sector(catalog.zones[0])),
      "Edge/corner contact alone does not join adjoining room volumes");
}

void exterior_culling() {
  const auto catalog = make_harbor_interiors();
  WorldVisibilitySettings settings;
  settings.camera_position = {-2.2f, 2.14f, -8.285f};
  settings.camera_forward = normalize(Vec3{0, .84f, -8.9f} - settings.camera_position);
  settings.cull_distance = 90.f;
  settings.sector_hide = true;
  settings.sector_focus = catalog.visibility_sector(*catalog.zone_at(settings.camera_position));
  const GeometryBounds outside{{{10.f, 1.f, -8.9f}, {.2f, .5f, .2f}}, true};
  require(!evaluate_world_visibility(settings, outside).visible,
      "Complete-room sector still culls nearby geometry outside the room");
  settings.sector_hide = false;
  require(evaluate_world_visibility(settings, outside).visible,
      "Nearby exterior fixture is rejected by the sector, not distance or facing");
  const GeometryBounds distant{{{200.f, 1.f, -8.9f}, {.2f, .5f, .2f}}, true};
  require(!evaluate_world_visibility(settings, distant).visible,
      "Exterior distance culling remains active with sector hiding disabled");
}

void lia_route(bool individual) {
  const auto catalog = make_harbor_interiors();
  Scene scene;
  NpcSystem npcs;
  vaultline::NpcPresentation presentation(true, individual);
  for (const auto& spec : vaultline::make_npc_roster()) {
    if (spec.agent.entity_name != "NpcTeller") continue;
    npcs.add(spec.agent);
    Entity e;
    e.name = spec.agent.entity_name;
    e.transform.position = spec.agent.position;
    presentation.install(scene, e, spec.agent.height,
        vaultline::npc_character_role(e.name, spec.agent.kind), spec.color);
    scene.add_entity(std::move(e));
  }
  require(npcs.agents().size() == 1, "Regression uses the actual production Lia roster");
  auto& npc = npcs.agents()[0];
  auto& entity = scene.entities()[0];
  require(entity.mesh && entity.lod_mesh && entity.mesh != entity.lod_mesh,
      "Regression uses real, distinct near and far character meshes");
  Vec3 offset{-2.6f, .18f, .7f};
  const float elevation = 25.f * 3.14159265359f / 180.f;
  const float horizontal = std::hypot(offset.x, offset.z);
  const float raised = std::cos(elevation) * horizontal - std::sin(elevation) * offset.y;
  offset = {offset.x * raised / horizontal,
      std::sin(elevation) * horizontal + std::cos(elevation) * offset.y,
      offset.z * raised / horizontal};
  MeshBoundsCache cache;
  int old_missing = 0;
  for (int frame = 1; frame <= 192; ++frame) {
    const Vec3 previous_camera = npc.position + offset;
    const auto* selected = catalog.zone_at(previous_camera);
    require(selected == &catalog.zones[1], "Route camera stays in the desk lighting sub-zone");
    // Production chooses lighting/sector before updating the NPC, then follows
    // its new position before drawing. Keep that ordering in the regression.
    npcs.apply_schedules(true);
    npcs.update(1.f / 12.f, .45f, previous_camera, 70.f);
    entity.transform.position = npc.position;
    entity.transform.rotation_euler.y = npc.yaw;
    presentation.advance(entity.name, {npc.position, npc.yaw, npc.actual_speed,
        npc.travel_distance, npc.move_weight, npc.turn_rate, 0.f, npc.on_duty},
        1.f / 12.f, previous_camera, 45.f);
    WorldVisibilitySettings settings;
    settings.camera_position = npc.position + offset;
    settings.camera_forward = normalize(npc.position - settings.camera_position);
    settings.cull_distance = 90.f;
    settings.lod_mid_distance = entity.lod_distance > 0.f ? entity.lod_distance : 45.f;
    settings.sector_hide = true;
    settings.sector_focus = old_sector(*selected);
    cache.begin_frame();
    const auto near = cache.world_bounds(*entity.mesh, entity.transform.matrix());
    const auto far = cache.world_bounds(*entity.lod_mesh, entity.transform.matrix());
    require(near.valid && far.valid, "Both posed production meshes have valid world bounds");
    const bool old_visible = evaluate_world_visibility(settings, near, &far).visible;
    require(old_visible == (frame < 86 || frame > 97),
        "Original lighting-sector policy reproduces exactly missing frames 86 through 97");
    if (!old_visible) ++old_missing;
    settings.sector_focus = catalog.visibility_sector(*selected);
    require(evaluate_world_visibility(settings, near, &far).visible,
        "Complete-room sector retains Lia throughout travel and the same-position turnaround");
    cache.end_frame();
  }
  require(old_missing == 12, "Regression reproduces the captured one-second disappearance");
  require(std::fabs(npc.position.x - .0884454101324) < 1.e-6 &&
      std::fabs(npc.travel_distance - 4.03270234168) < 1.e-6,
      "Route endpoint and travel match the original recorded world-state audit");
}
}  // namespace

int main() {
  try {
    catalog_and_lighting();
    connectivity();
    exterior_culling();
    lia_route(false);
    lia_route(true);
    std::cout << "interior_visibility: " << checks << " checks passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "interior_visibility: " << error.what() << '\n';
    return 1;
  }
}
