#pragma once

#include "fury/math.hpp"

#include <cstdint>
#include <string>
#include <memory>
#include <atomic>
#include <vector>

namespace fury {
struct MaterialTextures;

struct Vertex {
  Vec3 position;
  Vec3 normal{0.f, 1.f, 0.f};
  Vec3 color{1.f, 1.f, 1.f};
  Vec2 uv{0.f, 0.f};
  float opacity{1.f};
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
  /// File albedo: crate wood (assets/textures/crate_wood.*)
  Wood = 8,
  /// File albedo: metal barrel (assets/textures/barrel_metal.*)
  BarrelMetal = 9,
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
  /// Dielectric transmission for the DXR path (0 = opaque, 1 = transmissive).
  float transmission{0.f};
  float index_of_refraction{1.5f};
  float opacity{1.f};
  /// Negative means opaque; nonnegative enables alpha-mask visibility testing.
  float alpha_cutoff{-1.f};
  float normal_scale{1.f};
  bool double_sided{true};
  bool alpha_blend{false};
  Vec3 emissive_color{1.f,1.f,1.f};
  /// Immutable decoded glTF maps; ownership is shared across mesh instances.
  std::shared_ptr<const MaterialTextures> textures;
};

inline std::uint64_t next_mesh_identity() {
  static std::atomic<std::uint64_t> identity{1};
  return identity.fetch_add(1,std::memory_order_relaxed);
}
struct Mesh {
  Mesh() = default;
  Mesh(const Mesh& other) : vertices(other.vertices),indices(other.indices),gpu_dirty(true) {}
  Mesh(Mesh&&) noexcept = default;
  Mesh& operator=(Mesh&&) noexcept = default;
  Mesh& operator=(const Mesh& other) {
    if(this!=&other) {
      vertices=other.vertices; indices=other.indices;
      gpu_vao=gpu_vbo=gpu_ibo=0; gpu_uploaded=false; gpu_dirty=true;
      geometry_revision=0; geometry_identity=next_mesh_identity();
    }
    return *this;
  }
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;

  // Optional GPU handles (OpenGL). 0 = not uploaded.
  unsigned int gpu_vao{0};
  unsigned int gpu_vbo{0};
  unsigned int gpu_ibo{0};
  bool gpu_uploaded{false};
  /// When true, next upload/draw refreshes VBO from CPU vertices (walk pose).
  bool gpu_dirty{false};
  /// Increment after changing vertex/index data so modern backends can retain
  /// immutable geometry on the GPU without scanning it every frame.
  std::uint64_t geometry_revision{0};
  std::uint64_t geometry_identity{next_mesh_identity()};
  void mark_dirty() { gpu_dirty=true; ++geometry_revision; }
};

Mesh make_box(const Vec3& size, const Vec3& color);
Mesh make_plane(float width, float depth, const Vec3& color,
                float uv_scale = 1.f);
Mesh make_colored_box(const Vec3& size, const Vec3& color_top,
                      const Vec3& color_side);
/// Capsule-ish AABB body (stacked boxes) — legacy; prefer make_humanoid.
Mesh make_capsule(float radius, float height, const Vec3& color);
/// Low-poly humanoid (box torso/head/limbs/hands/feet + hair).
/// limb_phase radians drives walk swing; breathe_phase idle chest bob;
/// move_weight [0,1] blends walk vs idle (foot plant / stride scale).
Mesh make_humanoid(float height, const Vec3& color, float limb_phase = 0.f,
                   float breathe_phase = 0.f, float move_weight = 1.f);
/// Rebuild humanoid vertices in-place (marks gpu_dirty). Keeps GPU handles.
void pose_humanoid(Mesh& mesh, float height, const Vec3& color, float limb_phase,
                   float breathe_phase = 0.f, float move_weight = 1.f);

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
