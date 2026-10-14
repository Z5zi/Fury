#pragma once

#include <fury/scene.hpp>

#include <array>
#include <cstddef>

namespace vaultline {

struct WorldGroundStats {
  bool applied{false};
  bool already_applied{false};
  std::size_t districts{0};
  std::size_t replaced_surfaces{0};
  std::size_t added_entities{0};
  std::size_t added_triangles{0};
  std::size_t terrain_triangles{0};
  std::size_t frontage_walks{0};
  std::size_t curb_segments{0};
  std::size_t road_markings{0};
  std::size_t waterfront_edges{0};
  std::size_t pier_boards{0};
  std::size_t protected_solid_footprints{0};
  std::size_t water_apertures{0};
  float water_aperture_area{0.f};
  // Open area after subtracting the two existing opaque pier-deck footprints.
  float exposed_water_area{0.f};
  // Metro, Ridge, Ashcourt, Depot, Loft, North Quay. Counts authored features,
  // not draw calls: meshes are grouped by district, material and detail tier.
  std::array<std::size_t, 6> district_features{};
};

// Call once after the six original district builders, before caching weather
// materials. Idempotent. Never moves, removes, retags or changes a collider on
// an existing entity; all additions are non-solid, including low visual curbs.
WorldGroundStats upgrade_world_ground(fury::Scene& scene);

}  // namespace vaultline
