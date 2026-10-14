#include "fury/application.hpp"

#include "fury/collision.hpp"
#include "fury/log.hpp"
#include "fury/math.hpp"
#include "fury/platform.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <vector>

namespace fury {
namespace {

bool validate_capture_config(const AppConfig& config) {
  if (!config.capture_sequence_directory.empty() && config.max_frames == 0) {
    Log::error("Frame sequence capture requires max_frames > 0");
    return false;
  }
  if (config.capture_path.find('\0') != std::string::npos ||
      config.capture_sequence_directory.find('\0') != std::string::npos) {
    Log::error("Frame capture paths must not contain NUL characters");
    return false;
  }
  return true;
}

bool prepare_sequence_directory(const std::filesystem::path& directory,
                                const std::string& capture_path) {
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) {
    Log::error("Frame sequence directory failed: " + directory.string() + ": " + error.message());
    return false;
  }
  const bool empty = std::filesystem::is_empty(directory, error);
  if (error || !empty) {
    Log::error("Frame sequence directory must be empty: " + directory.string() +
               (error ? ": " + error.message() : ""));
    return false;
  }
  // Keep an optional last-frame capture outside the sequence directory, so it
  // cannot replace an earlier frame or add an unexpected sequence image.
  if (!capture_path.empty()) {
    const auto last_path = std::filesystem::weakly_canonical(capture_path, error);
    if (error) {
      Log::error("Frame capture path failed: " + capture_path + ": " + error.message());
      return false;
    }
    const auto sequence_path = std::filesystem::weakly_canonical(directory, error);
    if (error) {
      Log::error("Frame sequence path failed: " + directory.string() + ": " + error.message());
      return false;
    }
    const auto relative_last = last_path.lexically_relative(sequence_path);
    if (!relative_last.empty() && *relative_last.begin() != "..") {
      Log::error("Last-frame capture must be outside the frame sequence directory: " + capture_path);
      return false;
    }
  }
  return true;
}

std::filesystem::path sequence_frame_path(const std::filesystem::path& directory,
                                          unsigned frame, unsigned max_frames) {
  const auto digits = (std::max)(std::size_t{6}, std::to_string(max_frames).size());
  std::ostringstream name;
  name << "frame_" << std::setfill('0') << std::setw(static_cast<int>(digits)) << frame << ".ppm";
  return directory / name.str();
}

bool write_rgb_capture(const std::filesystem::path& path,
                       const std::vector<std::uint8_t>& rgb, int width, int height) {
  const auto parent = path.parent_path();
  if (!parent.empty()) {
    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) {
      Log::error("Frame capture directory failed: " + parent.string() + ": " + error.message());
      return false;
    }
  }
  std::ofstream capture(path, std::ios::binary);
  if (!capture) {
    Log::error("Frame capture open failed: " + path.string());
    return false;
  }
  capture << "P6\n" << width << " " << height << "\n255\n";
  capture.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
  // close() also checks errors reported only when the stream's buffer flushes.
  capture.close();
  if (!capture) {
    Log::error("Frame capture write failed: " + path.string());
    return false;
  }
  return true;
}

bool read_rgb_capture(Renderer& renderer, std::vector<std::uint8_t>& rgb,
                      int& width, int& height) {
  if (!renderer.read_rgb_framebuffer(rgb, width, height)) {
    Log::error("Frame capture framebuffer read failed");
    return false;
  }
  const auto max_pixels = (std::numeric_limits<std::size_t>::max)() / 3;
  if (width <= 0 || height <= 0 ||
      static_cast<std::size_t>(width) > max_pixels / static_cast<std::size_t>(height) ||
      rgb.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3 ||
      rgb.size() > static_cast<std::size_t>((std::numeric_limits<std::streamsize>::max)())) {
    Log::error("Frame capture framebuffer has invalid RGB dimensions or size");
    return false;
  }
  return true;
}

}  // namespace

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
  if (!validate_capture_config(m_config)) {
    return false;
  }
  if (m_initialized) {
    return true;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER |
               SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
    Log::error(std::string("SDL_Init failed: ") + SDL_GetError());
    return false;
  }
  m_initialized = true;

  Log::info(std::string("Fury " FURY_VERSION " on ") + platform_name());
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
  const char* requested_backend = std::getenv("FURY_RENDERER");
  if (m_config.preferred_backend==RenderBackendKind::Software || m_config.preferred_backend==RenderBackendKind::Direct3D12 ||
      m_config.preferred_backend==RenderBackendKind::CpuRayTracing ||
      (m_config.preferred_backend==RenderBackendKind::None && requested_backend &&
       (std::strcmp(requested_backend, "dx12") == 0 || std::strcmp(requested_backend, "cpu-ray") == 0 ||
        std::strcmp(requested_backend, "software") == 0))) use_gl = false;
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
                         m_window.opengl(),m_config.preferred_backend)) {
    if (m_window.opengl()) {
      Log::warn("Recreating window for software renderer");
      m_renderer.destroy();
      m_window.destroy();
      desc.opengl = false;
      if (!m_window.create(desc)) {
        return false;
      }
      if (!m_renderer.create(m_window.handle(), m_window.width(),
                            m_window.height(), false,m_config.preferred_backend)) {
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

void Application::draw_scene(bool shadow_pass) {
  WorldVisibilitySettings visibility;
  visibility.camera_position = m_camera.position;
  visibility.camera_forward = m_camera.forward();
  visibility.cull_distance = m_config.cull_distance;
  visibility.lod_mid_distance = m_config.lod_mid_distance;
  visibility.sector_hide = m_config.sector_hide;
  visibility.sector_focus = m_config.sector_focus;
  visibility.cull_behind_camera =
      m_renderer.backend_kind() != RenderBackendKind::Direct3D12 &&
      m_renderer.backend_kind() != RenderBackendKind::CpuRayTracing;
  m_visibility_bounds.begin_frame();

  struct DrawItem {
    const Mesh* mesh{nullptr};
    Mat4 model{};
    Material material{};
    int tex_key{0};
    float metallic{0.f};
    float roughness{0.f};
    std::uint64_t object_id{0};
  };
  std::vector<DrawItem> items;
  items.reserve(m_scene.entities().size());

  const auto finite = [](const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
  };
  const bool clearance_view =
      m_renderer.backend_kind() == RenderBackendKind::Software &&
      !m_camera.third_person && !m_camera.fly_mode && !m_camera.vehicle_seated &&
      !m_input.cinematic() && finite(m_camera.position);

  for (auto& e : m_scene.entities()) {
    // Reset before invisible/missing-mesh early returns. Shadow passes neither
    // consume nor change primary-view state, including these resets.
    if (!shadow_pass && (!e.first_person_clearance_enabled || !clearance_view ||
                         !e.visible || !e.mesh))
      e.first_person_clearance_hidden = false;
    if (!e.visible || !e.mesh) {
      continue;
    }

    const Mat4 model = e.transform.matrix();
    const GeometryBounds primary = m_visibility_bounds.world_bounds(*e.mesh, model);
    if (!shadow_pass && e.first_person_clearance_enabled && clearance_view) {
      const auto& box = primary.aabb;
      // geometry_distance_squared returns zero on invalid input. Fail open
      // explicitly instead of mistaking invalid geometry for a close actor.
      if (!primary.valid || !finite(box.center) || !finite(box.half_extents) ||
          box.half_extents.x < 0.f || box.half_extents.y < 0.f || box.half_extents.z < 0.f) {
        e.first_person_clearance_hidden = false;
      } else {
        const double distance_squared = geometry_distance_squared(primary, m_camera.position);
        if (distance_squared <= .20 * .20) e.first_person_clearance_hidden = true;
        else if (distance_squared >= .40 * .40) e.first_person_clearance_hidden = false;
      }
    }
    GeometryBounds lod;
    if (e.lod_mesh) lod = m_visibility_bounds.world_bounds(*e.lod_mesh, model);
    // A character may need its simpler mesh sooner than world props. Keep this
    // override local to the entity and leave detail-only hiding on the global
    // threshold; all conservative bounds and shadow-caster rules stay shared.
    auto entity_visibility = visibility;
    if (e.lod_mesh && std::isfinite(e.lod_distance) && e.lod_distance > 0.f)
      entity_visibility.lod_mid_distance = e.lod_distance;
    const auto decision = evaluate_world_visibility(
        entity_visibility, primary, e.lod_mesh ? &lod : nullptr, e.detail);
    if (!decision.visible) continue;
    // Keep ordinary bounds-cache/LOD participation while omitting only this
    // primary DrawItem. Reentry must not rebuild an evicted LOD bound.
    if (!shadow_pass && e.first_person_clearance_hidden) continue;

    DrawItem item;
    item.mesh = decision.use_lod ? e.lod_mesh : e.mesh;
    item.model = model;
    item.material = e.material;
    item.tex_key = static_cast<int>(e.material.texture);
    item.metallic = e.material.metallic;
    item.roughness = e.material.roughness;
    item.object_id = static_cast<std::uint64_t>(&e - m_scene.entities().data()) + 1;
    items.push_back(item);
  }
  m_visibility_bounds.end_frame();

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
    m_renderer.draw_mesh(*item.mesh, item.model, item.material, item.object_id);
  }
}

int Application::run() {
  if (!validate_capture_config(m_config)) {
    return 1;
  }
  const std::filesystem::path sequence_directory(m_config.capture_sequence_directory);
  // Snapshot the sequence's bound and destinations: frame callbacks may mutate
  // AppConfig, but must never turn a disk-writing run into an unbounded capture.
  const unsigned sequence_max_frames = m_config.max_frames;
  const std::string sequence_last_capture = m_config.capture_path;
  if (!sequence_directory.empty() &&
      !prepare_sequence_directory(sequence_directory, sequence_last_capture)) {
    return 1;
  }
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

  float render_elapsed = 0.f;
  unsigned rendered_frames = 0;

  m_timer.reset();
  m_fps_log_timer = 0.f;
  while (m_running) {
    InputState input{};
    m_input.poll(input);
    if (on_diagnostic_input) on_diagnostic_input(input, rendered_frames);
    if(m_window.sync_size()) m_renderer.resize(m_window.width(),m_window.height());
    m_last_input = input;
    if (input.quit_requested) {
      m_running = false;
      break;
    }

    const float measured_dt = m_timer.tick();
    const float dt = m_config.fixed_timestep > 0.f ? m_config.fixed_timestep : measured_dt;

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
      const float radius = effective_player_radius();
      if (!m_camera.vehicle_seated) {
        const auto movement = resolve_grounded_player_movement(
            m_prev_cam_pos, m_camera.position, radius, solids);
        if (movement.outcome == GroundedMoveOutcome::CertifiedSweep &&
            movement.sweep.accepted_position) {
          m_camera.position = *movement.sweep.accepted_position;
          if (movement.sweep.blocked_x) m_camera.velocity.x = 0.f;
          if (movement.sweep.blocked_z) m_camera.velocity.z = 0.f;
        } else if (movement.outcome == GroundedMoveOutcome::LegacyRecoveryUncertified &&
                   movement.legacy_position) {
          // Preserve the old endpoint/Y/velocity behavior only for a proven
          // embedded start. This result is not a certified swept placement.
          const Vec3 pre_resolve = m_camera.position;
          m_camera.position = *movement.legacy_position;
          m_camera.position.y = 1.7f;
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
        } else {
          // No placement/depenetration authority here. Retain the existing XZ
          // pose, but never represent an embedded/invalid start as certified.
          m_camera.position.x = m_prev_cam_pos.x;
          m_camera.position.z = m_prev_cam_pos.z;
          m_camera.velocity.x = m_camera.velocity.z = 0.f;
        }
        // Preserve the existing post-collision eye clamp, including crouch.
        m_camera.position.y = 1.7f;
      } else {
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
        draw_scene(true);
      }
      m_renderer.end_shadow_pass();
    }

    m_renderer.begin_frame(m_config.clear_color);
    if (!m_config.freeze_render_time) render_elapsed += dt;
    m_renderer.set_time(render_elapsed);
    const float aspect = static_cast<float>(m_window.width()) /
                         static_cast<float>((std::max)(1, m_window.height()));
    m_renderer.set_camera_position(m_camera.position);
    m_renderer.set_view_proj(m_camera.view_matrix(),
                             m_camera.projection_matrix(aspect));

    if (on_render) {
      on_render();
    } else {
      draw_scene(false);
    }

    if (on_hud && m_config.show_hud) {
      on_hud();
    }

    ++rendered_frames;
    const unsigned frame_limit = sequence_directory.empty() ? m_config.max_frames : sequence_max_frames;
    const bool last_bounded_frame = frame_limit && rendered_frames == frame_limit;
    const std::string& last_capture = sequence_directory.empty() ? m_config.capture_path : sequence_last_capture;
    if (!sequence_directory.empty() || (last_bounded_frame && !last_capture.empty())) {
      std::vector<std::uint8_t> rgb;
      int width{}, height{};
      if (!read_rgb_capture(m_renderer, rgb, width, height) ||
          (!sequence_directory.empty() &&
           !write_rgb_capture(sequence_frame_path(sequence_directory, rendered_frames, sequence_max_frames),
                              rgb, width, height)) ||
          (last_bounded_frame && !last_capture.empty() &&
           !write_rgb_capture(last_capture, rgb, width, height))) {
        m_running = false;
        return 1;
      }
    }
    if (last_bounded_frame) {
      m_running = false;
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
  const auto rendering=m_renderer.statistics();
  if(rendering.hardware_ray_tracing || rendering.software_ray_tracing)
    Log::info("Rendering validation: frames="+std::to_string(rendering.frame_index)+" provider="+rendering.upscaler+
              " triangles="+std::to_string(rendering.triangle_count)+" cpu_ms="+std::to_string(rendering.cpu_frame_ms)+
              " accumulated="+std::to_string(rendering.accumulated_frames)+" errors="+std::to_string(rendering.validation_errors));
  return m_renderer.statistics().validation_errors ? 1 : 0;
}

}  // namespace fury
