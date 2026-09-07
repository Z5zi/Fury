#pragma once

#include "fury/math.hpp"
#include "fury/mesh.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct SDL_Window;

namespace fury {

struct Color {
  std::uint8_t r{30};
  std::uint8_t g{30};
  std::uint8_t b{40};
  std::uint8_t a{255};
};

struct PointLight {
  Vec3 position{0.f, 4.5f, 0.f};
  Vec3 color{1.f, 0.92f, 0.65f};
  float intensity{1.4f};
  float radius{18.f};
};

struct Lighting {
  static constexpr int kMaxPointLights = 4;

  Vec3 sun_direction{-0.4f, -0.85f, -0.3f};  // direction toward the ground
  Vec3 sun_color{1.f, 0.96f, 0.88f};
  float sun_intensity{1.15f};
  Vec3 ambient{0.20f, 0.23f, 0.30f};
  float fog_start{35.f};
  float fog_end{130.f};
  Vec3 fog_color{0.52f, 0.64f, 0.82f};
  /// Single-pass SSAO-lite strength (0 = off). Prefer over shadow maps on llvmpipe.
  float ao_strength{0.55f};
  /// Request directional shadow map on GL path (auto-disabled on soft/llvmpipe).
  bool enable_shadows{true};
  float shadow_strength{0.45f};
  /// Planar / screen-space water reflection stub (auto-off on soft/llvmpipe).
  bool enable_reflections{true};
  float reflection_strength{0.55f};
  /// Bloom-lite for emissives (in-shader bright-pass add; skip via flag if heavy).
  bool enable_bloom{true};
  float bloom_strength{0.45f};
  /// Directional shadow map resolution (GL; 512/1024/2048 typical). Soft path ignores.
  int shadow_map_size{1024};
  /// Cascaded shadow stub: 1 = single map (med/low), 2 = near+far (high only). Soft/llvmpipe ignore.
  int shadow_cascade_count{1};
  /// Dynamic lamp point lights (nearest N filled by the app each frame).
  int point_light_count{0};
  PointLight point_lights[kMaxPointLights]{};
};

enum class RenderBackendKind {
  None,
  OpenGL,
  Software,
};

class IRenderBackend {
 public:
  virtual ~IRenderBackend() = default;
  virtual bool create(SDL_Window* window, int width, int height) = 0;
  virtual void destroy() = 0;
  virtual void begin_frame(const Color& clear) = 0;
  virtual void set_view_proj(const Mat4& view, const Mat4& proj) = 0;
  virtual void set_camera_position(const Vec3& pos) = 0;
  virtual void set_lighting(const Lighting& lighting) = 0;
  virtual void set_time(float seconds) = 0;
  virtual void draw_mesh(const Mesh& mesh, const Mat4& model,
                         const Material& material) = 0;
  /// Screen-space filled rect (pixels, origin top-left) for debug / HUD bars.
  virtual void draw_hud_rect(float x, float y, float w, float h,
                             const Color& color) = 0;
  virtual void end_frame() = 0;
  virtual void upload_mesh(Mesh& mesh) = 0;
  virtual void resize(int width, int height) = 0;
  virtual RenderBackendKind kind() const = 0;
  virtual const char* name() const = 0;
  /// Depth-only pass from sun for cascade index; false if shadows unavailable/disabled.
  virtual bool begin_shadow_pass(int /*cascade*/ = 0) { return false; }
  virtual void end_shadow_pass() {}
  virtual bool shadows_active() const { return false; }
  /// Active cascade count (0 = shadows off; 1 = single; 2 = high-quality stub).
  virtual int shadow_cascade_count() const { return 0; }
  /// Resize directional shadow map (no-op if unsupported / unchanged).
  virtual void set_shadow_map_size(int /*size*/) {}
  virtual int shadow_map_size() const { return 0; }
  /// Read current color buffer as tightly packed top-left RGB8 (Vaultline 4.9 screenshot stub).
  /// Call after HUD draw / before end_frame. Returns false if unsupported.
  virtual bool read_rgb_framebuffer(std::vector<std::uint8_t>& /*out_rgb*/, int& /*w*/,
                                    int& /*h*/) {
    return false;
  }
};

/// High-level 3D renderer: tries OpenGL 3.3 core, falls back to software.
class Renderer {
 public:
  Renderer() = default;
  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  bool create(SDL_Window* window, int width, int height, bool window_is_opengl);
  void destroy();

  void begin_frame(const Color& clear);
  void set_view_proj(const Mat4& view, const Mat4& proj);
  void set_camera_position(const Vec3& pos);
  void set_lighting(const Lighting& lighting);
  void set_time(float seconds);
  void draw_mesh(const Mesh& mesh, const Mat4& model,
                 const Material& material = {});
  void draw_hud_rect(float x, float y, float w, float h, const Color& color);
  void end_frame();
  void upload_mesh(Mesh& mesh);
  void resize(int width, int height);

  bool valid() const { return m_backend != nullptr; }
  RenderBackendKind backend_kind() const;
  const char* backend_name() const;

  bool begin_shadow_pass(int cascade = 0);
  void end_shadow_pass();
  bool shadows_active() const;
  int shadow_cascade_count() const;
  void set_shadow_map_size(int size);
  int shadow_map_size() const;
  /// Dump framebuffer RGB (top-left origin) for screenshot stub.
  bool read_rgb_framebuffer(std::vector<std::uint8_t>& out_rgb, int& w, int& h);

  Lighting& lighting() { return m_lighting; }
  const Lighting& lighting() const { return m_lighting; }

 private:
  std::unique_ptr<IRenderBackend> m_backend;
  Lighting m_lighting{};
};

std::unique_ptr<IRenderBackend> create_gl_backend();
std::unique_ptr<IRenderBackend> create_software_backend();

}  // namespace fury
