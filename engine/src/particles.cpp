#include "fury/particles.hpp"

#include <algorithm>
#include <cmath>

namespace fury {

float ParticleSystem::next_rand() {
  m_rng = m_rng * 1664525u + 1013904223u;
  return static_cast<float>((m_rng >> 8) & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
}

void ParticleSystem::emit_burst(const Vec3& origin, int count, float speed) {
  m_particles.reserve(m_particles.size() + static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    const float u = next_rand() * 6.2831853f;
    const float v = next_rand() * 3.1415926f;
    const float sp = speed * (0.45f + next_rand() * 0.85f);
    CpuParticle p;
    p.position = origin;
    p.velocity = {std::cos(u) * std::sin(v) * sp,
                  (0.55f + next_rand()) * sp,
                  std::sin(u) * std::sin(v) * sp};
    p.max_life = 0.55f + next_rand() * 0.7f;
    p.life = p.max_life;
    p.size = 0.18f + next_rand() * 0.28f;
    p.color = {0.95f + next_rand() * 0.2f, 0.75f + next_rand() * 0.2f,
               0.15f + next_rand() * 0.2f};
    p.emissive = 2.0f + next_rand() * 2.5f;
    m_particles.push_back(p);
  }
}

void ParticleSystem::update(float dt) {
  for (auto& p : m_particles) {
    p.life -= dt;
    p.velocity.y -= 9.5f * dt;
    p.position += p.velocity * dt;
  }
  m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(),
                                   [](const CpuParticle& p) {
                                     return p.life <= 0.f;
                                   }),
                    m_particles.end());
}

void ParticleSystem::sync_scene(Scene& scene, Mesh* quad_mesh,
                                const std::string& name_prefix) {
  if (!quad_mesh) return;

  // Hide previously spawned FX entities first.
  for (auto& e : scene.entities()) {
    if (e.name.rfind(name_prefix, 0) == 0) {
      e.visible = false;
    }
  }

  for (std::size_t i = 0; i < m_particles.size(); ++i) {
    const auto& p = m_particles[i];
    const std::string name = name_prefix + std::to_string(i);
    Entity* ent = scene.find_by_name(name);
    if (!ent) {
      Entity e;
      e.name = name;
      e.mesh = quad_mesh;
      e.tag = "fx";
      scene.add_entity(std::move(e));
      ent = scene.find_by_name(name);
      if (!ent) continue;
    }
    const float t = std::clamp(p.life / std::max(p.max_life, 0.001f), 0.f, 1.f);
    ent->mesh = quad_mesh;
    ent->transform.position = p.position;
    ent->transform.scale = {p.size, p.size, p.size};
    ent->material.albedo = p.color;
    ent->material.emissive = p.emissive * t;
    ent->material.metallic = 0.9f;
    ent->material.roughness = 0.25f;
    ent->visible = true;
  }
}

}  // namespace fury
