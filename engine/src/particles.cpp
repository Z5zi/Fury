#include "fury/particles.hpp"

#include <algorithm>
#include <cmath>

namespace fury {

float ParticleSystem::next_rand() {
  m_rng = m_rng * 1664525u + 1013904223u;
  return static_cast<float>((m_rng >> 8) & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
}

void ParticleSystem::trim_to_cap() {
  if (static_cast<int>(m_particles.size()) <= kMaxParticles) {
    return;
  }
  // Drop oldest first (front of vector) so new FX stay visible.
  const auto excess =
      static_cast<std::size_t>(m_particles.size()) -
      static_cast<std::size_t>(kMaxParticles);
  m_particles.erase(m_particles.begin(),
                    m_particles.begin() + static_cast<std::ptrdiff_t>(excess));
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
    p.kind = ParticleKind::Burst;
    p.rain = false;
    m_particles.push_back(p);
  }
  trim_to_cap();
}

void ParticleSystem::emit_rain_streaks(const Vec3& around, int count,
                                       float radius) {
  m_particles.reserve(m_particles.size() + static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    const float ang = next_rand() * 6.2831853f;
    const float rad = next_rand() * radius;
    CpuParticle p;
    p.position = {around.x + std::cos(ang) * rad,
                  around.y + 6.f + next_rand() * 10.f,
                  around.z + std::sin(ang) * rad};
    p.velocity = {(next_rand() - 0.5f) * 1.2f, -(14.f + next_rand() * 10.f),
                  (next_rand() - 0.5f) * 1.2f};
    p.max_life = 0.35f + next_rand() * 0.45f;
    p.life = p.max_life;
    p.size = 0.06f + next_rand() * 0.10f;
    p.color = {0.55f + next_rand() * 0.15f, 0.62f + next_rand() * 0.12f,
               0.78f + next_rand() * 0.15f};
    p.emissive = 0.15f + next_rand() * 0.25f;
    p.kind = ParticleKind::Rain;
    p.rain = true;
    m_particles.push_back(p);
  }
  trim_to_cap();
}

void ParticleSystem::emit_smoke_puff(const Vec3& origin, int count) {
  m_particles.reserve(m_particles.size() + static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    const float u = next_rand() * 6.2831853f;
    const float rad = next_rand() * 1.1f;
    CpuParticle p;
    p.position = {origin.x + std::cos(u) * rad * 0.35f,
                  origin.y + next_rand() * 0.4f,
                  origin.z + std::sin(u) * rad * 0.35f};
    p.velocity = {(next_rand() - 0.5f) * 1.8f, 0.9f + next_rand() * 1.6f,
                  (next_rand() - 0.5f) * 1.8f};
    p.max_life = 1.1f + next_rand() * 1.4f;
    p.life = p.max_life;
    p.size = 0.55f + next_rand() * 0.85f;
    const float g = 0.22f + next_rand() * 0.28f;
    p.color = {g, g + 0.02f, g + 0.04f};
    p.emissive = 0.02f + next_rand() * 0.06f;
    p.kind = ParticleKind::Smoke;
    p.rain = false;
    m_particles.push_back(p);
  }
  trim_to_cap();
}

void ParticleSystem::emit_sparks(const Vec3& origin, int count, float speed) {
  m_particles.reserve(m_particles.size() + static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    const float u = next_rand() * 6.2831853f;
    const float elev = 0.25f + next_rand() * 1.35f;
    const float sp = speed * (0.55f + next_rand() * 0.9f);
    CpuParticle p;
    p.position = origin;
    p.velocity = {std::cos(u) * sp, elev * sp * 0.55f, std::sin(u) * sp};
    p.max_life = 0.28f + next_rand() * 0.45f;
    p.life = p.max_life;
    p.size = 0.08f + next_rand() * 0.14f;
    p.color = {1.0f, 0.45f + next_rand() * 0.4f, 0.08f + next_rand() * 0.18f};
    p.emissive = 3.5f + next_rand() * 3.5f;
    p.kind = ParticleKind::Spark;
    p.rain = false;
    m_particles.push_back(p);
  }
  trim_to_cap();
}

void ParticleSystem::emit_tire_dust(const Vec3& origin, const Vec3& drive_dir,
                                    int count) {
  m_particles.reserve(m_particles.size() + static_cast<std::size_t>(count));
  const float dlen =
      std::sqrt(drive_dir.x * drive_dir.x + drive_dir.z * drive_dir.z);
  const float fx = (dlen > 1e-4f) ? drive_dir.x / dlen : 0.f;
  const float fz = (dlen > 1e-4f) ? drive_dir.z / dlen : 1.f;
  // Perpendicular for left/right tire scatter
  const float px = -fz;
  const float pz = fx;
  for (int i = 0; i < count; ++i) {
    const float side = (next_rand() < 0.5f) ? -1.f : 1.f;
    const float back = 0.6f + next_rand() * 1.4f;
    CpuParticle p;
    p.position = {origin.x - fx * back + px * side * (0.7f + next_rand() * 0.5f),
                  0.08f + next_rand() * 0.12f,
                  origin.z - fz * back + pz * side * (0.7f + next_rand() * 0.5f)};
    p.velocity = {-fx * (0.4f + next_rand() * 1.2f) + (next_rand() - 0.5f) * 0.8f,
                  0.35f + next_rand() * 0.9f,
                  -fz * (0.4f + next_rand() * 1.2f) + (next_rand() - 0.5f) * 0.8f};
    p.max_life = 0.35f + next_rand() * 0.55f;
    p.life = p.max_life;
    p.size = 0.22f + next_rand() * 0.35f;
    const float d = 0.38f + next_rand() * 0.22f;
    p.color = {d * 0.92f, d * 0.82f, d * 0.62f};
    p.emissive = 0.0f;
    p.kind = ParticleKind::TireDust;
    p.rain = false;
    m_particles.push_back(p);
  }
  trim_to_cap();
}

void ParticleSystem::update(float dt) {
  for (auto& p : m_particles) {
    p.life -= dt;
    switch (p.kind) {
      case ParticleKind::Rain:
        break;
      case ParticleKind::Smoke:
        p.velocity.y += 0.6f * dt;  // buoyant rise
        p.velocity.x *= (1.f - 0.55f * dt);
        p.velocity.z *= (1.f - 0.55f * dt);
        p.size += 0.55f * dt;  // expand
        break;
      case ParticleKind::TireDust:
        p.velocity.y -= 4.5f * dt;
        p.velocity.x *= (1.f - 1.2f * dt);
        p.velocity.z *= (1.f - 1.2f * dt);
        break;
      case ParticleKind::Spark:
        p.velocity.y -= 14.f * dt;
        break;
      case ParticleKind::Burst:
      default:
        p.velocity.y -= 9.5f * dt;
        break;
    }
    p.position += p.velocity * dt;
    if (p.kind == ParticleKind::TireDust && p.position.y < 0.04f) {
      p.position.y = 0.04f;
      p.velocity.y = 0.f;
    }
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
    ent->transform.rotation_euler = {0.f, 0.f, 0.f};
    switch (p.kind) {
      case ParticleKind::Rain:
        ent->transform.scale = {p.size * 0.35f, p.size * 3.2f, p.size * 0.35f};
        ent->material.albedo = p.color;
        ent->material.emissive = p.emissive * t;
        ent->material.metallic = 0.15f;
        ent->material.roughness = 0.55f;
        break;
      case ParticleKind::Smoke: {
        const float s = p.size * (0.65f + 0.55f * (1.f - t));
        ent->transform.scale = {s, s * 0.85f, s};
        ent->material.albedo = {p.color.x * (0.55f + 0.45f * t),
                                p.color.y * (0.55f + 0.45f * t),
                                p.color.z * (0.55f + 0.45f * t)};
        ent->material.emissive = p.emissive * t;
        ent->material.metallic = 0.05f;
        ent->material.roughness = 0.92f;
        break;
      }
      case ParticleKind::Spark:
        ent->transform.scale = {p.size * 0.45f, p.size * 1.8f, p.size * 0.45f};
        ent->material.albedo = p.color;
        ent->material.emissive = p.emissive * t;
        ent->material.metallic = 0.95f;
        ent->material.roughness = 0.18f;
        break;
      case ParticleKind::TireDust: {
        const float s = p.size * (0.7f + 0.5f * (1.f - t));
        ent->transform.scale = {s, s * 0.35f, s};
        ent->material.albedo = {p.color.x * t, p.color.y * t, p.color.z * t};
        ent->material.emissive = 0.f;
        ent->material.metallic = 0.02f;
        ent->material.roughness = 0.95f;
        break;
      }
      case ParticleKind::Burst:
      default:
        ent->transform.scale = {p.size, p.size, p.size};
        ent->material.albedo = p.color;
        ent->material.emissive = p.emissive * t;
        ent->material.metallic = 0.9f;
        ent->material.roughness = 0.25f;
        break;
    }
    ent->visible = true;
  }
}

}  // namespace fury
