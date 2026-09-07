#pragma once

/// Simple CPU particle bursts (billboard-ish quads as small boxes) for FX.

#include "fury/math.hpp"
#include "fury/mesh.hpp"
#include "fury/scene.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace fury {

enum class ParticleKind : std::uint8_t {
  Burst = 0,   ///< Gold celebration sparks (heist success)
  Rain,        ///< Weather streaks
  Smoke,       ///< SmokePellet grey puff
  Spark,       ///< Breach / impact sparks
  TireDust,    ///< Road dust while driving
};

struct CpuParticle {
  Vec3 position{};
  Vec3 velocity{};
  float life{0.f};
  float max_life{1.f};
  float size{0.3f};
  Vec3 color{1.f, 0.85f, 0.25f};
  float emissive{2.5f};
  ParticleKind kind{ParticleKind::Burst};
  /// Legacy alias — prefer kind == ParticleKind::Rain.
  bool rain{false};
};

/// Emits short-lived FX quads (smoke, sparks, tire dust, rain, celebration).
class ParticleSystem {
 public:
  static constexpr int kMaxParticles = 280;

  void emit_burst(const Vec3& origin, int count, float speed = 7.f);
  /// Downward rain streaks around the camera (weather stub).
  void emit_rain_streaks(const Vec3& around, int count, float radius = 18.f);
  /// Grey expanding puff for SmokePellet (X).
  void emit_smoke_puff(const Vec3& origin, int count = 22);
  /// Hot sparks on vault/safe breach.
  void emit_sparks(const Vec3& origin, int count = 36, float speed = 9.f);
  /// Low road dust behind / under a moving vehicle.
  void emit_tire_dust(const Vec3& origin, const Vec3& drive_dir, int count = 4);

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
  void trim_to_cap();
};

}  // namespace fury
