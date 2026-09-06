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
  virtual void draw_mesh(const Mesh& mesh, const Mat4& model) = 0;
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

  /// Prefer OpenGL when possible. Falls back to software automatically.
  bool create(SDL_Window* window, int width, int height, bool window_is_opengl);
  void destroy();

  void begin_frame(const Color& clear);
  void set_view_proj(const Mat4& view, const Mat4& proj);
  void draw_mesh(const Mesh& mesh, const Mat4& model);
  void end_frame();
  void upload_mesh(Mesh& mesh);
  void resize(int width, int height);

  bool valid() const { return m_backend != nullptr; }
  RenderBackendKind backend_kind() const;
  const char* backend_name() const;

 private:
  std::unique_ptr<IRenderBackend> m_backend;
};

std::unique_ptr<IRenderBackend> create_gl_backend();
std::unique_ptr<IRenderBackend> create_software_backend();

}  // namespace fury
