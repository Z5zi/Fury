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

std::vector<Aabb> Scene::collect_solids() const {
  std::vector<Aabb> out;
  out.reserve(m_entities.size());
  for (const auto& e : m_entities) {
    if (!e.solid) continue;
    Aabb world;
    // collider.center is a local offset from the entity origin.
    world.center = e.transform.position + e.collider.center;
    world.half_extents.x = e.collider.half_extents.x * e.transform.scale.x;
    world.half_extents.y = e.collider.half_extents.y * e.transform.scale.y;
    world.half_extents.z = e.collider.half_extents.z * e.transform.scale.z;
    out.push_back(world);
  }
  return out;
}

}  // namespace fury
