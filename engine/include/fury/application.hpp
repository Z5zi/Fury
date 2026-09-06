#pragma once

#include <functional>
#include <string>

#include "fury/input.hpp"
#include "fury/renderer.hpp"
#include "fury/timer.hpp"
#include "fury/window.hpp"

namespace fury {

struct AppConfig {
  WindowDesc window;
  Color clear_color{30, 30, 46, 255};
  bool log_fps{true};
  float fps_log_interval{1.0f};
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
  const Timer& timer() const { return m_timer; }

 private:
  AppConfig m_config;
  Window m_window;
  Renderer m_renderer;
  Input m_input;
  Timer m_timer;
  bool m_running{false};
  bool m_initialized{false};
  float m_fps_log_timer{0.f};
};

}  // namespace fury
