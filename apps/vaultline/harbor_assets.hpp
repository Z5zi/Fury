#pragma once

#include <fury/fury.hpp>
#include <fury/gltf.hpp>

#include <string>
#include <unordered_map>
#include <vector>

/// Harbor Metro / HMPD / Meridian Mutual content bridge (no GTA IP).
/// Central name→path(+lod) registry with GLB→OBJ→procedural fallbacks.
namespace harbor {

enum class VehicleVisualType {
  HmpdCruiser,
  CivSedan,
  CivHatch,
  CivVan,
};

struct HarborAssetDesc {
  const char* name;
  const char* glb;      ///< under assets/meshes/
  const char* lod_glb;  ///< optional LOD1 (nullable)
  const char* obj;      ///< OBJ fallback (nullable)
};

/// Logical stems used by the Meridian Mutual vertical slice.
const HarborAssetDesc* find_asset(const char* name);
const HarborAssetDesc* all_assets(std::size_t& out_count);

struct LoadedHarborMesh {
  fury::Mesh* mesh{nullptr};
  fury::Mesh* lod_mesh{nullptr};
  fury::Material material{};
  bool from_asset{false};
  bool used_fallback{false};
};

/// Multi-primitive placement cache (preserves per-part materials for hero props).
struct HarborPrimPart {
  fury::Mesh* mesh{nullptr};
  fury::Material material{};
};

struct HarborPrimSet {
  std::vector<HarborPrimPart> parts;
  bool from_asset{false};
  bool used_fallback{false};
};

/// Resolve path prefixes the same way as load_obj_asset.
bool resolve_mesh_path(const char* relative, std::string& out_path);

/// Load GLB (preferred) → OBJ → fallback mesh. Filters ground_walk/curb helpers.
LoadedHarborMesh load_harbor_mesh(fury::Scene& scene, const char* asset_name,
                                  fury::Mesh fallback,
                                  const char* log_label = nullptr);

/// Load all non-helper glTF primitives as separate meshes (hero bank/props).
HarborPrimSet load_harbor_prims(fury::Scene& scene, const char* asset_name,
                                fury::Mesh fallback,
                                const char* log_label = nullptr);

/// Place a merged mesh entity with optional box collider.
void place_merged(fury::Scene& scene, const LoadedHarborMesh& loaded,
                  const char* entity_name, const fury::Vec3& pos, float yaw = 0.f,
                  bool solid = false, const fury::Vec3& collider_size = {},
                  bool detail = false, const char* tag = nullptr,
                  const fury::Vec3& scale = {1.f, 1.f, 1.f});

/// Place every prim part at root pose (parts already in asset-local space).
void place_prims(fury::Scene& scene, const HarborPrimSet& set,
                 const char* name_prefix, const fury::Vec3& pos, float yaw = 0.f,
                 bool detail = true, const char* tag = nullptr);

/// Phase 1 — Meridian Mutual interior kit pieces (readable lobby→security→vault).
void spawn_meridian_mutual(fury::Scene& scene);

/// Phase 2 — Bank-block street dressing (ATM, benches, bollards, etc.).
void spawn_meridian_block(fury::Scene& scene);

/// Phase 4 — Rear-alley getaway spawn (civ van/sedan). Returns world position.
fury::Vec3 spawn_meridian_getaway(fury::Scene& scene, VehicleVisualType kind);

/// Convenience: vehicle visual name for registry lookup.
const char* vehicle_asset_name(VehicleVisualType kind);

/// Expected acceptance log lines (also emitted by spawn helpers).
inline constexpr const char* kLogMeridianMutual = "Meridian Mutual loaded";
inline constexpr const char* kLogVaultDoor = "Vault door loaded";
inline constexpr const char* kLogHmpdCruiser = "HMPD cruiser loaded";
inline constexpr const char* kLogGetaway = "Getaway vehicles loaded";
inline constexpr const char* kLogStreetKit = "Street kit loaded";

}  // namespace harbor
