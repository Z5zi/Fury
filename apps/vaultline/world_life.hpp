#pragma once

#include <fury/scene.hpp>

#include <array>
#include <cstddef>

namespace vaultline {

/// Counts describe this invocation, not an estimate of render-visible triangles.
struct WorldLifeStats {
  std::size_t replaced_entities{};
  std::size_t added_batches{};
  std::size_t plant_instances{};
  std::size_t utility_props{};
  std::size_t containers{};
  std::size_t crane_parts{};
  std::size_t market_parts{};
  std::size_t authored_planters{};
  std::size_t removed_foliage_triangles{};
  std::size_t skipped_authored_planters{};
  std::size_t near_triangles{};
  std::size_t lod_triangles{};
  std::size_t replaced_triangles{};
  /// Harbor, Ridge, Ashcourt, Depot, Loft, North Quay, including existing plants.
  std::array<std::size_t, 6> district_plants{};
  bool already_applied{};
};

/// Run once after all six districts have been assembled. Repeated calls are
/// harmless. Changes only audited primitive meshes; all existing transforms,
/// materials, names, tags, visibility, collision bounds and solid flags survive.
/// Added vegetation is non-solid, local to explicit boundary/planter footprints,
/// opaque geometry (no alpha dependency), and batched by district with a LOD.
WorldLifeStats upgrade_world_life(fury::Scene& scene);

}  // namespace vaultline
