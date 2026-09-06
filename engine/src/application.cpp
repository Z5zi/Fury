#include "fury/application.hpp"

#include "fury/collision.hpp"
#include "fury/log.hpp"
#include "fury/math.hpp"
#include "fury/platform.hpp"

#include <SDL.h>

#include <algorithm>
#include <sstream>

namespace fury {

Application::Application(AppConfig config) : m_config(std::move(config)) {}

Application::~Application() {
  m_input.set_mouse_captured(false);
  m_renderer.destroy();
  m_window.destroy();
  if (m_initialized) {
    SDL_Quit();
    m_initialized = false;
  }
}

bool Application::init() {
  if (m_initialized) {
    return true;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
    Log::error(std::string("SDL_Init failed: ") + SDL_GetError());
    return false;
  }
  m_initialized = true;

  Log::info(std::string("Fury 1.0.0 on ") + platform_name());
  Log::info(std::string("Math backend: ") +
            (math_uses_asm() ? "x86_64 NASM (fury_dot3_asm)" : "C++ fallback"));

  const Vec3 a{1.f, 2.f, 3.f};
  const Vec3 b{4.f, 5.f, 6.f};
  {
    std::ostringstream oss;
    oss << "dot({1,2,3},{4,5,6}) = " << dot(a, b) << " (expect 32)";
    Log::info(oss.str());
  }

  bool use_gl = m_config.prefer_opengl;
  WindowDesc desc = m_config.window;
  desc.opengl = use_gl;

  if (!m_window.create(desc)) {
    if (use_gl) {
      Log::warn("OpenGL window failed; retrying without GL");
      desc.opengl = false;
      if (!m_window.create(desc)) {
        return false;
      }
      use_gl = false;
    } else {
      return false;
    }
  }

  if (!m_renderer.create(m_window.handle(), m_window.width(), m_window.height(),
                         m_window.opengl())) {
    if (m_window.opengl()) {
      Log::warn("Recreating window for software renderer");
      m_renderer.destroy();
      m_window.destroy();
      desc.opengl = false;
      if (!m_window.create(desc)) {
        return false;
      }
      if (!m_renderer.create(m_window.handle(), m_window.width(),
                            m_window.height(), false)) {
        return false;
      }
    } else {
      return false;
    }
  }

  {
    std::ostringstream oss;
    oss << "Active renderer: " << m_renderer.backend_name();
    Log::info(oss.str());
  }

  // Outdoor default lighting (can be overridden by apps)
  Lighting lit;
  lit.fog_color = {m_config.clear_color.r / 255.f,
                   m_config.clear_color.g / 255.f,
                   m_config.clear_color.b / 255.f};
  m_renderer.set_lighting(lit);

  if (m_config.capture_mouse) {
    m_input.set_mouse_captured(true);
  }

  m_timer.reset();
  m_fps_log_timer = 0.f;
  m_prev_cam_pos = m_camera.position;
  return true;
}

void Application::request_quit() { m_running = false; }

void Application::draw_scene() {
  const float cull = m_config.cull_distance;
  const float cull2 = cull > 0.f ? cull * cull : 0.f;
  const Vec3 cam = m_camera.position;
  const Vec3 fwd = m_camera.forward();
  for (const auto& e : m_scene.entities()) {
    if (!e.visible || !e.mesh) {
      continue;
    }
    if (cull2 > 0.f) {
      const float dx = e.transform.position.x - cam.x;
      const float dy = e.transform.position.y - cam.y;
      const float dz = e.transform.position.z - cam.z;
      const float d2 = dx * dx + dy * dy + dz * dz;
      if (d2 > cull2) {
        continue;
      }
      // Cheap frustum reject: behind camera (with slack for large props).
      if (d2 > 25.f && (dx * fwd.x + dy * fwd.y + dz * fwd.z) < -8.f) {
        continue;
      }
    }
    m_renderer.draw_mesh(*e.mesh, e.transform.matrix(), e.material);
  }
}

int Application::run() {
  if (!m_initialized && !init()) {
    return 1;
  }

  for (auto& mesh_ptr : m_scene.meshes()) {
    if (mesh_ptr) {
      m_renderer.upload_mesh(*mesh_ptr);
    }
  }

  m_running = true;
  m_prev_cam_pos = m_camera.position;
  Log::info("Entering main loop (Esc to quit; click to capture mouse)");

  float elapsed = 0.f;

  while (m_running) {
    InputState input{};
    m_input.poll(input);
    m_last_input = input;
    if (input.quit_requested) {
      m_running = false;
      break;
    }

    const float dt = m_timer.tick();
    elapsed += dt;

    bool f_consumed = false;
    if (on_pre_update) {
      f_consumed = on_pre_update(dt, input);
    }

    if (input.key_f && !f_consumed && !m_camera.vehicle_seated) {
      m_camera.fly_mode = !m_camera.fly_mode;
      Log::info(m_camera.fly_mode ? "Camera: fly mode" : "Camera: walk mode");
    }

    m_prev_cam_pos = m_camera.position;
    m_camera.update(input, dt);

    if (m_config.enable_collision && !m_camera.fly_mode) {
      const auto solids = m_scene.collect_solids();
      const float radius =
          m_camera.vehicle_seated ? m_config.player_radius * 1.8f
                                  : m_config.player_radius;
      m_camera.position = resolve_player_collision(
          m_camera.position, radius, solids,
          m_camera.vehicle_seated ? 1.55f : 1.7f);
      m_camera.position.y = m_camera.vehicle_seated ? 1.55f : 1.7f;
    }

    if (on_update) {
      on_update(dt, input);
    }

    m_renderer.begin_frame(m_config.clear_color);
    m_renderer.set_time(elapsed);
    const float aspect = static_cast<float>(m_window.width()) /
                         static_cast<float>(std::max(1, m_window.height()));
    m_renderer.set_camera_position(m_camera.position);
    m_renderer.set_view_proj(m_camera.view_matrix(),
                             m_camera.projection_matrix(aspect));

    if (on_render) {
      on_render();
    } else {
      draw_scene();
    }

    if (on_hud) {
      on_hud();
    }

    m_renderer.end_frame();

    if (m_config.log_fps) {
      m_fps_log_timer += dt;
      if (m_fps_log_timer >= m_config.fps_log_interval) {
        std::ostringstream oss;
        oss << "FPS: " << m_timer.fps() << "  pos=(" << m_camera.position.x
            << ", " << m_camera.position.y << ", " << m_camera.position.z
            << ")  [" << m_renderer.backend_name() << "]";
        Log::info(oss.str());
        m_fps_log_timer = 0.f;
      }
    }
  }

  Log::info("Shutdown");
  return 0;
}

}  // namespace fury
