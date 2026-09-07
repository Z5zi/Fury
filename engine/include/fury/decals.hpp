#pragma once

/// Flat ground-mark stub: bullet-hole-like dark spots and tire skids that fade.

#include "fury/math.hpp"
#include "fury/mesh.hpp"
#include "fury/scene.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace fury {

enum class DecalKind : std::uint8_t {
  BulletHole = 0,
  SkidMark,
};

struct Decal {
  Vec3 position{};
  float yaw{0.f};
  float life{0.f};
  float max_life{4.f};
  float size_x{0.35f};
  float size_z{0.35f};
  Vec3 color{0.06f, 0.05f, 0.05f};
  DecalKind kind{DecalKind::BulletHole};
};

/// Cap-limited fading flat quads (prototype — not AAA decals).
class DecalSystem {
 public:
  static constexpr int kMaxDecals = 64;

  void spawn_bullet_hole(const Vec3& pos, float size = 0.28f);
  void spawn_skid_mark(const Vec3& pos, float yaw, float length = 1.35f,
                       float width = 0.22f);
  void update(float dt);
  void sync_scene(Scene& scene, Mesh* quad_mesh,
                  const std::string& name_prefix = "FXDecal");
  bool active() const { return !m_decals.empty(); }
  int count() const { return static_cast<int>(m_decals.size()); }

 private:
  std::vector<Decal> m_decals;
  unsigned m_rng{0xDEC41u};
  float next_rand();
  void push_capped(Decal d);
};

}  // namespace fury
