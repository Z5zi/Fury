#pragma once
#include "fury/mesh.hpp"
#include "fury/renderer.hpp"
#include "fury/gltf.hpp"
#include <memory>
#include <vector>

struct CoastalScene {
  struct Instance { fury::Mesh* mesh; fury::Mat4 model; fury::Material material; };
  std::vector<std::unique_ptr<fury::Mesh>> meshes;
  std::vector<Instance> instances;
  fury::GltfAsset pier_asset;
  fury::GltfAsset tree_asset;
  fury::Mesh* deformation_mesh{};
  std::vector<fury::Vertex> undeformed_vertices;
  void create(const std::string& pier_path={},const std::string& tree_path={});
  void draw(fury::Renderer& renderer, float animation_time=0);
  void deform(float time);
};
