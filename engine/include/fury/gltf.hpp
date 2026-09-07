#pragma once
#include "fury/mesh.hpp"
#include <memory>
#include <string>
#include <vector>

namespace fury {
struct GltfPrimitive {
  std::shared_ptr<Mesh> mesh;
  Material material;
  Mat4 transform{Mat4::identity()};
  std::string name;
};
struct GltfAsset {
  std::vector<GltfPrimitive> primitives;
  Vec3 bounds_min{},bounds_max{};
};
/// Import a static glTF 2.0/GLB scene with indexed triangles and metallic/roughness
/// materials. Buffer-view images and PNG/JPEG textures are supported. External
/// resources must stay within the asset directory. Unsupported required extensions,
/// skinning/morphs and unsupported texture coordinate layouts fail explicitly.
/// Transactional: out is unchanged on failure; error contains a human-readable reason.
bool load_gltf(const std::string& path,GltfAsset& out,std::string& error);
}  // namespace fury
