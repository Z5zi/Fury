#pragma once

/// Simple CPU particle bursts (billboard-ish quads as small boxes) for FX.

#include "fury/math.hpp"
#include "fury/mesh.hpp"
#include "fury/scene.hpp"

#include <string>
#include <vector>

namespace fury {

struct CpuParticle {
  Vec3 position{};
  Vec3 velocity{};
  float life{0.f};
  float max_life{1.f};
  float size{0.3f};
  Vec3 color{1.f, 0.85f, 0.25f};
  float emissive{2.5f};
};

/// Emits short-lived gold spark quads on heist success, etc.
class ParticleSystem {
 public:
  void emit_burst(const Vec3& origin, int count, float speed = 7.f);
  void update(float dt);
  /// Ensures scene entities FXParticle0..N track live particles (CPU quads).
  void sync_scene(Scene& scene, Mesh* quad_mesh,
                  const std::string& name_prefix = "FXParticle");
  bool active() const { return !m_particles.empty(); }
  const std::vector<CpuParticle>& particles() const { return m_particles; }

 private:
  std::vector<CpuParticle> m_particles;
  unsigned m_rng{0xC0FFEEu};
  float next_rand();
};

}  // namespace fury
