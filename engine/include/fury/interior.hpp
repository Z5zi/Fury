#pragma once

/// Interior lighting zones + door trigger volumes for Vaultline (2.5.0).
/// Original Harbor Metro interiors only — no third-party IP.

#include "fury/collision.hpp"
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

  /// Lighting sub-zones can overlap inside one room. Cull against the whole
  /// connected room, while zone_at still selects the original local lighting.
  Aabb visibility_sector(const InteriorZone& seed) const {
    std::vector<const InteriorZone*> connected{&seed};
    Vec3 lo = seed.center - seed.half_extents;
    Vec3 hi = seed.center + seed.half_extents;
    for (std::size_t next = 0; next < connected.size(); ++next) {
      const auto& current = *connected[next];
      for (const auto& candidate : zones) {
        if (!seed.tag || !candidate.tag ||
            std::strcmp(seed.tag, candidate.tag) != 0 ||
            std::find(connected.begin(), connected.end(), &candidate) != connected.end())
          continue;
        // Use authored volumes, never the accumulated box or expanded margins:
        // either can bridge a gap to a disconnected room with the same tag.
        const float ox = current.half_extents.x + candidate.half_extents.x -
                         std::fabs(current.center.x - candidate.center.x);
        const float oy = current.half_extents.y + candidate.half_extents.y -
                         std::fabs(current.center.y - candidate.center.y);
        const float oz = current.half_extents.z + candidate.half_extents.z -
                         std::fabs(current.center.z - candidate.center.z);
        // Volume overlap or a shared face connects zones; an edge/corner alone
        // does not establish an adjoining room volume.
        if (ox < 0.f || oy < 0.f || oz < 0.f ||
            int(ox > 0.f) + int(oy > 0.f) + int(oz > 0.f) < 2)
          continue;
        connected.push_back(&candidate);
        const Vec3 a = candidate.center - candidate.half_extents;
        const Vec3 b = candidate.center + candidate.half_extents;
        lo = {(std::min)(lo.x, a.x), (std::min)(lo.y, a.y), (std::min)(lo.z, a.z)};
        hi = {(std::max)(hi.x, b.x), (std::max)(hi.y, b.y), (std::max)(hi.z, b.z)};
      }
    }
    // Retain the existing margins, and the exact old result for a single zone.
    const Vec3 center = connected.size() == 1 ? seed.center : (lo + hi) * .5f;
    const Vec3 half = connected.size() == 1 ? seed.half_extents : (hi - lo) * .5f;
    return {center, {half.x * 1.1f, half.y * 1.15f, half.z * 1.1f}};
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

  // Meridian Mutual — mission lighting sub-zones (most specific first for zone_at).
  // Vault antechamber: dramatic gold + cool spill.
  {
    InteriorZone z;
    z.tag = "bank";
    z.center = {0.f, 2.6f, -15.2f};
    z.half_extents = {6.5f, 3.2f, 3.4f};
    z.ambient_boost = 1.25f;
    z.exterior_dim = 0.18f;
    z.extra_light_count = 3;
    z.extra_lights[0] = {{0.f, 3.4f, -14.4f}, {1.f, 0.72f, 0.28f}, 2.15f, 11.f};
    z.extra_lights[1] = {{0.f, 2.8f, -16.2f}, {0.55f, 0.72f, 1.05f}, 1.65f, 10.f};
    z.extra_lights[2] = {{-3.5f, 3.0f, -14.8f}, {0.9f, 0.55f, 0.25f}, 1.1f, 8.f};
    cat.zones.push_back(z);
  }
  // Security desk / restricted corridor: cooler cyan-white.
  {
    InteriorZone z;
    z.tag = "bank";
    z.center = {-4.2f, 2.6f, -10.5f};
    z.half_extents = {4.2f, 3.2f, 4.0f};
    z.ambient_boost = 1.35f;
    z.exterior_dim = 0.24f;
    z.extra_light_count = 2;
    z.extra_lights[0] = {{-5.2f, 3.6f, -9.8f}, {0.62f, 0.82f, 1.05f}, 1.9f, 11.f};
    z.extra_lights[1] = {{-3.5f, 3.4f, -12.0f}, {0.55f, 0.75f, 1.0f}, 1.45f, 9.f};
    cat.zones.push_back(z);
  }
  // Lobby / entrance: warm brass fill (fallback for rest of bank shell).
  {
    InteriorZone z;
    z.tag = "bank";
    z.center = {0.f, 2.8f, -8.5f};
    z.half_extents = {8.2f, 3.4f, 5.5f};
    z.ambient_boost = 1.58f;
    z.exterior_dim = 0.34f;
    z.extra_light_count = 3;
    z.extra_lights[0] = {{-3.5f, 3.8f, -6.5f}, {1.f, 0.92f, 0.72f}, 1.95f, 13.f};
    z.extra_lights[1] = {{3.5f, 3.8f, -6.5f}, {1.f, 0.92f, 0.72f}, 1.95f, 13.f};
    z.extra_lights[2] = {{0.f, 3.9f, -4.8f}, {1.f, 0.95f, 0.8f}, 1.55f, 12.f};
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
