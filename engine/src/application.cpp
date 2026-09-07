#include "fury/application.hpp"

#include "fury/collision.hpp"
#include "fury/log.hpp"
#include "fury/math.hpp"
#include "fury/platform.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

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

  Log::info(std::string("Fury 3.9.0 on ") + platform_name());
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
  float mid = m_config.lod_mid_distance;
  if (mid <= 0.f && cull > 0.f) {
    mid = cull * 0.5f;
  }
  const float mid2 = mid > 0.f ? mid * mid : 0.f;
  const Vec3 cam = m_camera.position;
  const Vec3 fwd = m_camera.forward();

  struct DrawItem {
    const Mesh* mesh{nullptr};
    Mat4 model{};
    Material material{};
    int tex_key{0};
    float metallic{0.f};
    float roughness{0.f};
  };
  std::vector<DrawItem> items;
  items.reserve(m_scene.entities().size());

  for (const auto& e : m_scene.entities()) {
    if (!e.visible || !e.mesh) {
      continue;
    }

    const float dx = e.transform.position.x - cam.x;
    const float dy = e.transform.position.y - cam.y;
    const float dz = e.transform.position.z - cam.z;
    const float d2 = dx * dx + dy * dy + dz * dz;

    if (cull2 > 0.f && d2 > cull2) {
      continue;
    }

    // Optional sector hide — drop outdoor props when player is deep indoors.
    if (m_config.sector_hide) {
      const Vec3& c = e.transform.position;
      const Aabb& f = m_config.sector_focus;
      if (std::fabs(c.x - f.center.x) > f.half_extents.x ||
          std::fabs(c.y - f.center.y) > f.half_extents.y ||
          std::fabs(c.z - f.center.z) > f.half_extents.z) {
        continue;
      }
    }

    // Occlusion-lite: skip if world AABB is fully behind the camera plane.
    {
      Vec3 wc = e.transform.position + e.collider.center;
      Vec3 h = e.collider.half_extents;
      if (h.x <= 1e-4f && h.y <= 1e-4f && h.z <= 1e-4f) {
        h = {1.f, 1.f, 1.f};
      }
      h.x *= e.transform.scale.x;
      h.y *= e.transform.scale.y;
      h.z *= e.transform.scale.z;
      const float rdx = wc.x - cam.x;
      const float rdy = wc.y - cam.y;
      const float rdz = wc.z - cam.z;
      const float center_d = rdx * fwd.x + rdy * fwd.y + rdz * fwd.z;
      const float extent =
          std::fabs(h.x * fwd.x) + std::fabs(h.y * fwd.y) + std::fabs(h.z * fwd.z);
      if (d2 > 4.f && (center_d + extent) < -0.25f) {
        continue;
      }
    }

    // LOD stub: beyond mid range, use lod_mesh or skip tagged detail props.
    const Mesh* mesh = e.mesh;
    if (mid2 > 0.f && d2 > mid2) {
      if (e.lod_mesh) {
        mesh = e.lod_mesh;
      } else if (e.detail) {
        continue;
      }
    }

    DrawItem item;
    item.mesh = mesh;
    item.model = e.transform.matrix();
    item.material = e.material;
    item.tex_key = static_cast<int>(e.material.texture);
    item.metallic = e.material.metallic;
    item.roughness = e.material.roughness;
    items.push_back(item);
  }

  // Batching stub: sort by texture/material to reduce binds (future: GPU instancing).
  std::sort(items.begin(), items.end(),
            [](const DrawItem& a, const DrawItem& b) {
              if (a.tex_key != b.tex_key) {
                return a.tex_key < b.tex_key;
              }
              if (a.metallic != b.metallic) {
                return a.metallic < b.metallic;
              }
              return a.roughness < b.roughness;
            });

  for (const DrawItem& item : items) {
    m_renderer.draw_mesh(*item.mesh, item.model, item.material);
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

    if (input.key_v && !m_camera.vehicle_seated) {
      m_camera.third_person = !m_camera.third_person;
      Log::info(m_camera.third_person ? "Camera: third person"
                                      : "Camera: first person");
    }

    m_prev_cam_pos = m_camera.position;
    m_camera.update(input, dt);

    if (m_config.enable_collision && !m_camera.fly_mode) {
      const auto solids = m_scene.collect_solids();
      const float radius =
          m_camera.vehicle_seated ? m_config.player_radius * 1.8f
                                  : m_config.player_radius;
      const Vec3 pre_resolve = m_camera.position;
      m_camera.position = resolve_player_collision(
          m_camera.position, radius, solids,
          m_camera.vehicle_seated ? 1.55f : 1.7f);
      m_camera.position.y = m_camera.vehicle_seated ? 1.55f : 1.7f;
      // Slide: drop velocity component that pushed into the solid.
      const float cdx = m_camera.position.x - pre_resolve.x;
      const float cdz = m_camera.position.z - pre_resolve.z;
      const float c2 = cdx * cdx + cdz * cdz;
      if (c2 > 1e-8f) {
        const float inv = 1.f / std::sqrt(c2);
        const float nx = cdx * inv;
        const float nz = cdz * inv;
        const float into = m_camera.velocity.x * nx + m_camera.velocity.z * nz;
        if (into < 0.f) {
          m_camera.velocity.x -= nx * into;
          m_camera.velocity.z -= nz * into;
        }
      }
    }

    if (on_update) {
      on_update(dt, input);
    }

    // Directional shadow map(s) — 1 cascade med/low, 2 on high; no-op soft/llvmpipe.
    m_renderer.set_camera_position(m_camera.position);
    const int shadow_cascades = m_renderer.shadow_cascade_count();
    for (int cascade = 0; cascade < shadow_cascades; ++cascade) {
      if (!m_renderer.begin_shadow_pass(cascade)) {
        break;
      }
      if (on_render) {
        on_render();
      } else {
        draw_scene();
      }
      m_renderer.end_shadow_pass();
    }

    m_renderer.begin_frame(m_config.clear_color);
    m_renderer.set_time(elapsed);
    const float aspect = static_cast<float>(m_window.width()) /
                         static_cast<float>((std::max)(1, m_window.height()));
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
