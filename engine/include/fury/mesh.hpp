#pragma once

#include "fury/math.hpp"

#include <cstdint>
#include <vector>

namespace fury {

struct Vertex {
  Vec3 position;
  Vec3 normal{0.f, 1.f, 0.f};
  Vec3 color{1.f, 1.f, 1.f};
  Vec2 uv{0.f, 0.f};
};

/// Procedural / embedded texture slots used by the lit renderer.
enum class TextureSlot : int {
  None = 0,
  Checker = 1,
  Asphalt = 2,
  Concrete = 3,
  Water = 4,
  Brick = 5,
  Metal = 6,
  Glass = 7,
  Count
};

struct Material {
  Vec3 albedo{1.f, 1.f, 1.f};
  float metallic{0.f};
  float roughness{0.55f};
  /// Self-illumination strength (lamp heads, neon signs). Added after lighting.
  float emissive{0.f};
  TextureSlot texture{TextureSlot::None};
  /// UV scroll speed (units/sec) — used for water / animated surfaces.
  float uv_scroll_u{0.f};
  float uv_scroll_v{0.f};
  /// Wet-road amount [0,1] — drives anisotropic-ish specular streak hack.
  float wetness{0.f};
};

struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;

  // Optional GPU handles (OpenGL). 0 = not uploaded.
  unsigned int gpu_vao{0};
  unsigned int gpu_vbo{0};
  unsigned int gpu_ibo{0};
  bool gpu_uploaded{false};
};

Mesh make_box(const Vec3& size, const Vec3& color);
Mesh make_plane(float width, float depth, const Vec3& color,
                float uv_scale = 1.f);
Mesh make_colored_box(const Vec3& size, const Vec3& color_top,
                      const Vec3& color_side);
/// Capsule-ish AABB body (stacked boxes) for NPC agents.
Mesh make_capsule(float radius, float height, const Vec3& color);

}  // namespace fury
