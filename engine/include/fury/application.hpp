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
  Color clear_color{25, 28, 40, 255};
  bool log_fps{true};
  float fps_log_interval{1.0f};
  bool prefer_opengl{true};
  bool capture_mouse{true};
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
};

}  // namespace fury
