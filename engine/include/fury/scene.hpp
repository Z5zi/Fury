#pragma once

#include "fury/mesh.hpp"
#include "fury/transform.hpp"

#include <memory>
#include <string>
#include <vector>

namespace fury {

struct Entity {
  std::string name;
  Transform transform;
  Mesh* mesh{nullptr};
  bool visible{true};
  // Optional gameplay tag (e.g. "bank", "vault", "escape")
  std::string tag;
};

class Scene {
 public:
  Mesh* add_mesh(Mesh mesh);
  Entity& add_entity(Entity entity);

  std::vector<std::unique_ptr<Mesh>>& meshes() { return m_meshes; }
  const std::vector<std::unique_ptr<Mesh>>& meshes() const { return m_meshes; }
  std::vector<Entity>& entities() { return m_entities; }
  const std::vector<Entity>& entities() const { return m_entities; }

  Entity* find_by_tag(const std::string& tag);
  Entity* find_by_name(const std::string& name);

 private:
  std::vector<std::unique_ptr<Mesh>> m_meshes;
  std::vector<Entity> m_entities;
};

}  // namespace fury
