#pragma once

#include "fury/camera.hpp"
#include "fury/input.hpp"
#include "fury/renderer.hpp"
#include "fury/scene.hpp"
#include "fury/timer.hpp"
#include "fury/window.hpp"

#include <functional>
#include <string>

namespace fury {

struct AppConfig {
  WindowDesc window;
  Color clear_color{72, 110, 160, 255};  // outdoor sky-ish default
  bool log_fps{true};
  float fps_log_interval{1.0f};
  bool prefer_opengl{true};
  RenderBackendKind preferred_backend{RenderBackendKind::None};
  bool capture_mouse{true};
  bool enable_collision{true};
  float player_radius{0.45f};
  /// Skip drawing entities farther than this (meters). 0 = disabled.
  float cull_distance{90.f};
  /// Detail props skipped (or swapped to lod_mesh) beyond this. <=0 → 0.5 * cull.
  float lod_mid_distance{0.f};
  /// When true, skip draw if entity origin is outside sector_focus (deep indoors).
  bool sector_hide{false};
  Aabb sector_focus{};
};

class Application {
 public:
  explicit Application(AppConfig config = {});
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  bool init();
  /// Run until Esc or window close. Returns exit code (0 = ok).
  int run();
  void request_quit();

  Window& window() { return m_window; }
  Renderer& renderer() { return m_renderer; }
  Input& input() { return m_input; }
  Camera& camera() { return m_camera; }
  Scene& scene() { return m_scene; }
  const Timer& timer() const { return m_timer; }
  const InputState& last_input() const { return m_last_input; }
  AppConfig& config() { return m_config; }

  /// Optional: runs before fly-toggle + camera move. Return true to consume F
  /// (skip default fly toggle) — used by vehicle enter/exit.
  std::function<bool(float dt, const InputState&)> on_pre_update;
  /// Optional hooks (called each frame after input / before present).
  std::function<void(float dt, const InputState&)> on_update;
  std::function<void()> on_render;  // after camera set; draw scene if null
  std::function<void()> on_hud;     // after 3D draw; for logging overlays

 private:
  void draw_scene();

  AppConfig m_config;
  Window m_window;
  Renderer m_renderer;
  Input m_input;
  Camera m_camera;
  Scene m_scene;
  Timer m_timer;
  InputState m_last_input{};
  bool m_running{false};
  bool m_initialized{false};
  float m_fps_log_timer{0.f};
  Vec3 m_prev_cam_pos{};
};

}  // namespace fury
