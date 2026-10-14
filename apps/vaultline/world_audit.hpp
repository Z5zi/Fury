#pragma once
#include <fury/scene.hpp>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace vaultline {
struct WorldSceneCounts {
  std::uint64_t entities{}, visible_entities{}, triangles{}, unique_mesh_triangles{};
  std::uint64_t solids{}, tagged_entities{}, invalid_indices{}, nonfinite_vertices{}, degenerate_triangles{}, zero_area_triangles{}, nonfinite_instances{};
};
struct ProtectedWorldEntity {
  std::string name, tag;
  bool solid{};
  fury::Transform transform;
  fury::Aabb collider;
};
struct WorldSceneSnapshot {
  WorldSceneCounts counts;
  std::uint64_t geometry_fingerprint{14695981039346656037ull};
  std::vector<ProtectedWorldEntity> protected_entities;
};
using WorldCoverage = std::map<std::string, std::map<std::string,std::uint64_t>>;
WorldSceneSnapshot snapshot_world(const fury::Scene& scene);
bool write_world_audit(const std::string& path, const fury::Scene& scene,
                       const WorldSceneSnapshot& before, bool enabled,
                       const WorldCoverage& coverage);
} // namespace vaultline
