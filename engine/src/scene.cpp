#include "fury/scene.hpp"

namespace fury {

Mesh* Scene::add_mesh(Mesh mesh) {
  m_meshes.push_back(std::make_unique<Mesh>(std::move(mesh)));
  return m_meshes.back().get();
}

Entity& Scene::add_entity(Entity entity) {
  m_entities.push_back(std::move(entity));
  return m_entities.back();
}

Entity* Scene::find_by_tag(const std::string& tag) {
  for (auto& e : m_entities) {
    if (e.tag == tag) return &e;
  }
  return nullptr;
}

Entity* Scene::find_by_name(const std::string& name) {
  for (auto& e : m_entities) {
    if (e.name == name) return &e;
  }
  return nullptr;
}

}  // namespace fury
