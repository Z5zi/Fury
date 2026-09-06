#pragma once

#include "fury/math.hpp"
#include "fury/mesh.hpp"

#include <cstdint>
#include <memory>
#include <string>

struct SDL_Window;

namespace fury {

struct Color {
  std::uint8_t r{30};
  std::uint8_t g{30};
  std::uint8_t b{40};
  std::uint8_t a{255};
};

struct Lighting {
  Vec3 sun_direction{-0.4f, -0.85f, -0.3f};  // direction toward the ground
  Vec3 sun_color{1.f, 0.96f, 0.88f};
  float sun_intensity{1.15f};
  Vec3 ambient{0.20f, 0.23f, 0.30f};
  float fog_start{35.f};
  float fog_end{130.f};
  Vec3 fog_color{0.52f, 0.64f, 0.82f};
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
  virtual void draw_mesh(const Mesh& mesh, const Mat4& model,
                         const Material& material) = 0;
  virtual void end_frame() = 0;
  virtual void upload_mesh(Mesh& mesh) = 0;
  virtual void resize(int width, int height) = 0;
  virtual RenderBackendKind kind() const = 0;
  virtual const char* name() const = 0;
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
  void draw_mesh(const Mesh& mesh, const Mat4& model,
                 const Material& material = {});
  void end_frame();
  void upload_mesh(Mesh& mesh);
  void resize(int width, int height);

  bool valid() const { return m_backend != nullptr; }
  RenderBackendKind backend_kind() const;
  const char* backend_name() const;

  Lighting& lighting() { return m_lighting; }
  const Lighting& lighting() const { return m_lighting; }

 private:
  std::unique_ptr<IRenderBackend> m_backend;
  Lighting m_lighting{};
};

std::unique_ptr<IRenderBackend> create_gl_backend();
std::unique_ptr<IRenderBackend> create_software_backend();

}  // namespace fury
