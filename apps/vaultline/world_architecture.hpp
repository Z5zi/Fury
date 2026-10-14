#pragma once

#include <cstddef>

namespace fury { class Scene; }

namespace vaultline {

/// Per-call counts. Calling the upgrade again is a no-op for existing buildings.
struct WorldArchitectureStats {
  std::size_t eligible_shells{};
  std::size_t upgraded_shells{};
  std::size_t metro_shells{};
  std::size_t ridge_shells{};
  std::size_t ashcourt_shells{};
  std::size_t north_quay_shells{};
  std::size_t landmark_exteriors{};
  std::size_t protected_shells{};
  std::size_t window_bays{};
  std::size_t hidden_window_strips{};
  std::size_t added_entities{};
  /// New active triangles, including replacement shell meshes (excluding LOD).
  std::size_t triangles{};
};

/// Upgrade only the closed, decorative district boxes and add exterior-only
/// depot/loft trim. Original entity identity, pose, collider and gameplay tags
/// remain unchanged. Call after every district has been built, before play.
WorldArchitectureStats upgrade_world_architecture(fury::Scene& scene);

}  // namespace vaultline
