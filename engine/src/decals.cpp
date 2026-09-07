#include "fury/decals.hpp"

#include <algorithm>
#include <cmath>

namespace fury {

float DecalSystem::next_rand() {
  m_rng = m_rng * 1664525u + 1013904223u;
  return static_cast<float>((m_rng >> 8) & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
}

void DecalSystem::push_capped(Decal d) {
  if (static_cast<int>(m_decals.size()) >= kMaxDecals) {
    m_decals.erase(m_decals.begin());
  }
  m_decals.push_back(d);
}

void DecalSystem::spawn_bullet_hole(const Vec3& pos, float size) {
  Decal d;
  d.position = {pos.x, 0.035f + next_rand() * 0.02f, pos.z};
  d.yaw = next_rand() * 6.2831853f;
  d.max_life = 5.5f + next_rand() * 3.5f;
  d.life = d.max_life;
  const float s = size * (0.75f + next_rand() * 0.5f);
  d.size_x = s;
  d.size_z = s * (0.85f + next_rand() * 0.3f);
  const float g = 0.04f + next_rand() * 0.06f;
  d.color = {g, g * 0.9f, g * 0.85f};
  d.kind = DecalKind::BulletHole;
  push_capped(d);
}

void DecalSystem::spawn_skid_mark(const Vec3& pos, float yaw, float length,
                                  float width) {
  Decal d;
  d.position = {pos.x, 0.03f, pos.z};
  d.yaw = yaw;
  d.max_life = 3.2f + next_rand() * 2.4f;
  d.life = d.max_life;
  d.size_x = width * (0.85f + next_rand() * 0.3f);
  d.size_z = length * (0.85f + next_rand() * 0.35f);
  const float g = 0.05f + next_rand() * 0.07f;
  d.color = {g, g * 0.92f, g * 0.88f};
  d.kind = DecalKind::SkidMark;
  push_capped(d);
}

void DecalSystem::update(float dt) {
  for (auto& d : m_decals) {
    d.life -= dt;
  }
  m_decals.erase(std::remove_if(m_decals.begin(), m_decals.end(),
                                [](const Decal& d) { return d.life <= 0.f; }),
                 m_decals.end());
}

void DecalSystem::sync_scene(Scene& scene, Mesh* quad_mesh,
                             const std::string& name_prefix) {
  if (!quad_mesh) return;

  for (auto& e : scene.entities()) {
    if (e.name.rfind(name_prefix, 0) == 0) {
      e.visible = false;
    }
  }

  for (std::size_t i = 0; i < m_decals.size(); ++i) {
    const auto& d = m_decals[i];
    const std::string name = name_prefix + std::to_string(i);
    Entity* ent = scene.find_by_name(name);
    if (!ent) {
      Entity e;
      e.name = name;
      e.mesh = quad_mesh;
      e.tag = "decal";
      e.detail = true;
      scene.add_entity(std::move(e));
      ent = scene.find_by_name(name);
      if (!ent) continue;
    }
    const float t = std::clamp(d.life / std::max(d.max_life, 0.001f), 0.f, 1.f);
    // Fade albedo toward black*t so marks wash out over life.
    const float fade = t * t;  // ease-out dark → gone
    ent->mesh = quad_mesh;
    ent->transform.position = d.position;
    ent->transform.rotation_euler = {0.f, d.yaw, 0.f};
    // Flat thin quad on the ground (Y scale tiny).
    if (d.kind == DecalKind::SkidMark) {
      ent->transform.scale = {d.size_x, 0.02f, d.size_z};
    } else {
      ent->transform.scale = {d.size_x, 0.018f, d.size_z};
    }
    ent->material.albedo = {d.color.x * fade, d.color.y * fade, d.color.z * fade};
    ent->material.emissive = 0.f;
    ent->material.metallic = 0.05f;
    ent->material.roughness = 0.92f;
    ent->material.wetness = 0.f;
    ent->visible = fade > 0.02f;
  }
}

}  // namespace fury
