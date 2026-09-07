#pragma once

#include "fury/math.hpp"

#include <cstdint>
#include <string>
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
  /// When true, next upload/draw refreshes VBO from CPU vertices (walk pose).
  bool gpu_dirty{false};
};

Mesh make_box(const Vec3& size, const Vec3& color);
Mesh make_plane(float width, float depth, const Vec3& color,
                float uv_scale = 1.f);
Mesh make_colored_box(const Vec3& size, const Vec3& color_top,
                      const Vec3& color_side);
/// Capsule-ish AABB body (stacked boxes) — legacy; prefer make_humanoid.
Mesh make_capsule(float radius, float height, const Vec3& color);
/// Low-poly humanoid (box torso/head/limbs). limb_phase radians drives sin swing.
Mesh make_humanoid(float height, const Vec3& color, float limb_phase = 0.f);
/// Rebuild humanoid vertices in-place (marks gpu_dirty). Keeps GPU handles.
void pose_humanoid(Mesh& mesh, float height, const Vec3& color, float limb_phase);

/// Load a simple Wavefront OBJ (v / vt / vn / f). Triangulates n-gons.
/// Vertex colors default to default_color (material albedo tints at draw).
/// Returns false on I/O or empty geometry (out cleared).
bool load_obj(const std::string& path, Mesh& out,
              const Vec3& default_color = Vec3{1.f, 1.f, 1.f});

/// Resolve `assets/meshes/<filename>` from common cwd layouts (repo root / build).
/// Tries several relative prefixes; returns the first path that loads.
bool load_obj_asset(const char* filename, Mesh& out,
                    const Vec3& default_color = Vec3{1.f, 1.f, 1.f});

}  // namespace fury
