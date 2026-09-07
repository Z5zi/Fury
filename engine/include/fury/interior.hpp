#pragma once

/// Interior lighting zones + door trigger volumes for Vaultline (2.5.0).
/// Original Harbor Metro interiors only — no third-party IP.

#include "fury/math.hpp"
#include "fury/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace fury {

struct InteriorZone {
  /// Gameplay tag: "bank" | "jewelry" | "loft" | "depot"
  const char* tag{"bank"};
  Vec3 center{0.f, 2.f, 0.f};
  Vec3 half_extents{5.f, 3.f, 5.f};
  /// Warm fill when player is inside this volume.
  float ambient_boost{1.45f};
  /// Scale applied to sun_intensity (exterior contribution) while inside.
  float exterior_dim{0.38f};
  /// Extra interior point lights (enabled only while inside).
  int extra_light_count{0};
  PointLight extra_lights[Lighting::kMaxPointLights]{};

  bool contains(const Vec3& p) const {
    return std::fabs(p.x - center.x) <= half_extents.x &&
           std::fabs(p.y - center.y) <= half_extents.y &&
           std::fabs(p.z - center.z) <= half_extents.z;
  }
};

struct DoorTrigger {
  /// Short label for logs / HUD encoding (e.g. "Meridian", "Crown").
  const char* label{"Door"};
  /// Zone tag this door leads into.
  const char* zone_tag{"bank"};
  Vec3 center{0.f, 1.2f, 0.f};
  Vec3 half_extents{2.2f, 1.8f, 1.4f};
  /// Optional snap target just inside the doorway (eye height).
  Vec3 interior_spawn{0.f, 1.7f, 0.f};
  /// If true, Interact (E) snaps the camera to interior_spawn while in volume.
  bool snap_on_interact{true};

  bool contains(const Vec3& p) const {
    return std::fabs(p.x - center.x) <= half_extents.x &&
           std::fabs(p.y - center.y) <= half_extents.y &&
           std::fabs(p.z - center.z) <= half_extents.z;
  }
};

struct InteriorCatalog {
  std::vector<InteriorZone> zones;
  std::vector<DoorTrigger> doors;

  const InteriorZone* zone_at(const Vec3& p) const {
    for (const auto& z : zones) {
      if (z.contains(p)) return &z;
    }
    return nullptr;
  }

  const DoorTrigger* door_at(const Vec3& p) const {
    for (const auto& d : doors) {
      if (d.contains(p)) return &d;
    }
    return nullptr;
  }

  /// Boost ambient, dim sun, and replace point lights with interior extras.
  static void apply_zone_lighting(Lighting& lit, const InteriorZone& zone) {
    lit.ambient = lit.ambient * zone.ambient_boost;
    lit.sun_intensity *= zone.exterior_dim;
    // Soften directional contribution further so interiors read as lit rooms.
    lit.shadow_strength *= 0.55f;
    const int n = (std::min)(zone.extra_light_count, Lighting::kMaxPointLights);
    lit.point_light_count = n;
    for (int i = 0; i < n; ++i) {
      lit.point_lights[i] = zone.extra_lights[i];
    }
  }
};

/// Harbor Metro interior + door catalog used by Vaultline 2.5.0.
inline InteriorCatalog make_harbor_interiors() {
  InteriorCatalog cat;

  // Meridian Mutual bank lobby (center 0,-10; shell ~18x14)
  {
    InteriorZone z;
    z.tag = "bank";
    z.center = {0.f, 2.8f, -10.f};
    z.half_extents = {8.2f, 3.4f, 6.2f};
    z.ambient_boost = 1.55f;
    z.exterior_dim = 0.32f;
    z.extra_light_count = 2;
    z.extra_lights[0] = {{-4.f, 5.2f, -8.f}, {1.f, 0.94f, 0.78f}, 1.85f, 14.f};
    z.extra_lights[1] = {{4.f, 5.2f, -8.f}, {1.f, 0.94f, 0.78f}, 1.85f, 14.f};
    cat.zones.push_back(z);

    DoorTrigger d;
    d.label = "Meridian";
    d.zone_tag = "bank";
    d.center = {0.f, 1.4f, -2.6f};
    d.half_extents = {2.4f, 1.8f, 1.5f};
    d.interior_spawn = {0.f, 1.7f, -6.5f};
    d.snap_on_interact = true;
    cat.doors.push_back(d);
  }

  // Crown & Cutler jewelry (-22, 8; shell ~12x10)
  {
    InteriorZone z;
    z.tag = "jewelry";
    z.center = {-22.f, 2.6f, 8.f};
    z.half_extents = {5.4f, 3.2f, 4.4f};
    z.ambient_boost = 1.5f;
    z.exterior_dim = 0.34f;
    z.extra_light_count = 2;
    z.extra_lights[0] = {{-22.f, 5.2f, 8.f}, {1.f, 0.88f, 0.72f}, 1.95f, 12.f};
    z.extra_lights[1] = {{-22.f, 3.2f, 6.f}, {1.f, 0.75f, 0.85f}, 1.15f, 9.f};
    cat.zones.push_back(z);

    DoorTrigger d;
    d.label = "Crown";
    d.zone_tag = "jewelry";
    d.center = {-22.f, 1.3f, 13.2f};
    d.half_extents = {2.0f, 1.7f, 1.4f};
    d.interior_spawn = {-22.f, 1.7f, 9.5f};
    d.snap_on_interact = true;
    cat.doors.push_back(d);
  }

  // Harbor loft safehouse (42, 52; shell ~11x9) — tag "loft"
  {
    InteriorZone z;
    z.tag = "loft";
    z.center = {42.f, 2.2f, 52.f};
    z.half_extents = {4.6f, 3.0f, 3.8f};
    z.ambient_boost = 1.4f;
    z.exterior_dim = 0.4f;
    z.extra_light_count = 2;
    z.extra_lights[0] = {{43.8f, 2.4f, 51.5f}, {1.f, 0.9f, 0.65f}, 1.6f, 10.f};
    z.extra_lights[1] = {{40.5f, 4.2f, 52.f}, {0.95f, 0.92f, 0.85f}, 1.25f, 11.f};
    cat.zones.push_back(z);

    DoorTrigger d;
    d.label = "Loft";
    d.zone_tag = "loft";
    d.center = {42.f, 1.3f, 56.8f};
    d.half_extents = {1.9f, 1.7f, 1.4f};
    d.interior_spawn = {42.f, 1.7f, 53.5f};
    d.snap_on_interact = true;
    cat.doors.push_back(d);
  }

  // Harbor Armored Depot (58, -48; hall ~16x12)
  {
    InteriorZone z;
    z.tag = "depot";
    z.center = {58.f, 3.0f, -48.f};
    z.half_extents = {7.2f, 4.0f, 5.4f};
    z.ambient_boost = 1.48f;
    z.exterior_dim = 0.3f;
    z.extra_light_count = 2;
    z.extra_lights[0] = {{54.f, 5.8f, -48.f}, {0.95f, 0.92f, 0.7f}, 1.9f, 13.f};
    z.extra_lights[1] = {{62.f, 5.8f, -48.f}, {0.95f, 0.92f, 0.7f}, 1.9f, 13.f};
    cat.zones.push_back(z);

    DoorTrigger d;
    d.label = "Depot";
    d.zone_tag = "depot";
    d.center = {58.f, 1.5f, -41.5f};
    d.half_extents = {2.6f, 2.0f, 1.6f};
    d.interior_spawn = {58.f, 1.7f, -45.5f};
    d.snap_on_interact = true;
    cat.doors.push_back(d);
  }

  return cat;
}

}  // namespace fury
