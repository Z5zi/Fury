#pragma once

#include <fury/scene.hpp>
#include <fury/texture.hpp>

#include <cstddef>
#include <memory>

namespace vaultline {

struct WorldWaterStats {
  bool applied{false};
  bool already_applied{false};
  std::size_t updated_surfaces{0};
  std::size_t harbor_surfaces{0};
  std::size_t ridge_surfaces{0};
  std::size_t north_quay_surfaces{0};
  std::size_t preserved_source_materials{0};
  std::size_t shared_map_bytes{0};
  std::size_t added_triangles{0};
};

// Original deterministic, periodic, 16-metre water patch. Returns an independent
// immutable map set, including all additional mip levels (256 -> 1). The scene
// upgrade shares one such set across all three original water planes.
std::shared_ptr<const fury::MaterialTextures> make_world_water_textures();

// Call after upgrade_world_ground only inside the FURY_WORLD_ART=1 branch.
// Changes only material fields on the exact three original Water-slot entities.
// Meshes, vertex colors, UV0, transforms, visibility, colliders and tags stay
// untouched. Imported/source map sets are never replaced. Idempotent per entity.
WorldWaterStats upgrade_world_water(fury::Scene& scene);

}  // namespace vaultline
