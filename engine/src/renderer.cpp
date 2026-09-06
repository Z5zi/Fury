#include "fury/renderer.hpp"

#include "fury/log.hpp"

#include <SDL.h>

namespace fury {

Renderer::~Renderer() { destroy(); }

bool Renderer::create(SDL_Window* window, int width, int height,
                      bool window_is_opengl) {
  destroy();
  if (!window) {
    Log::error("Renderer::create: null window");
    return false;
  }

  if (window_is_opengl) {
    auto gl = create_gl_backend();
    if (gl && gl->create(window, width, height)) {
      m_backend = std::move(gl);
      m_backend->set_lighting(m_lighting);
      return true;
    }
    Log::warn("OpenGL backend unavailable; falling back to software");
  }

  auto soft = create_software_backend();
  if (!soft || !soft->create(window, width, height)) {
    Log::error("Failed to create software renderer backend");
    return false;
  }
  m_backend = std::move(soft);
  m_backend->set_lighting(m_lighting);
  return true;
}

void Renderer::destroy() {
  if (m_backend) {
    m_backend->destroy();
    m_backend.reset();
  }
}

void Renderer::begin_frame(const Color& clear) {
  if (m_backend) m_backend->begin_frame(clear);
}

void Renderer::set_view_proj(const Mat4& view, const Mat4& proj) {
  if (m_backend) m_backend->set_view_proj(view, proj);
}

void Renderer::set_camera_position(const Vec3& pos) {
  if (m_backend) m_backend->set_camera_position(pos);
}

void Renderer::set_lighting(const Lighting& lighting) {
  m_lighting = lighting;
  if (m_backend) m_backend->set_lighting(lighting);
}

void Renderer::draw_mesh(const Mesh& mesh, const Mat4& model,
                         const Material& material) {
  if (m_backend) m_backend->draw_mesh(mesh, model, material);
}

void Renderer::end_frame() {
  if (m_backend) m_backend->end_frame();
}

void Renderer::upload_mesh(Mesh& mesh) {
  if (m_backend) m_backend->upload_mesh(mesh);
}

void Renderer::resize(int width, int height) {
  if (m_backend) m_backend->resize(width, height);
}

RenderBackendKind Renderer::backend_kind() const {
  return m_backend ? m_backend->kind() : RenderBackendKind::None;
}

const char* Renderer::backend_name() const {
  return m_backend ? m_backend->name() : "None";
}

}  // namespace fury
