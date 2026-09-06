#include "fury/application.hpp"

#include "fury/log.hpp"
#include "fury/math.hpp"
#include "fury/platform.hpp"

#include <SDL.h>

#include <sstream>

namespace fury {

Application::Application(AppConfig config) : m_config(std::move(config)) {}

Application::~Application() {
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

  Log::info(std::string("Fury 0.1.0 on ") + platform_name());
  Log::info(std::string("Math backend: ") +
            (math_uses_asm() ? "x86_64 NASM (fury_dot3_asm)" : "C++ fallback"));

  // Smoke-test math so asm is exercised when present
  const Vec3 a{1.f, 2.f, 3.f};
  const Vec3 b{4.f, 5.f, 6.f};
  const float d = dot(a, b);
  {
    std::ostringstream oss;
    oss << "dot({1,2,3},{4,5,6}) = " << d << " (expect 32)";
    Log::info(oss.str());
  }

  if (!m_window.create(m_config.window)) {
    return false;
  }
  if (!m_renderer.create(m_window.handle())) {
    return false;
  }

  m_timer.reset();
  m_fps_log_timer = 0.f;
  return true;
}

void Application::request_quit() { m_running = false; }

int Application::run() {
  if (!m_initialized && !init()) {
    return 1;
  }

  m_running = true;
  Log::info("Entering main loop (Esc to quit)");

  while (m_running) {
    InputState input{};
    m_input.poll(input);
    if (input.quit_requested) {
      m_running = false;
      break;
    }

    const float dt = m_timer.tick();
    m_renderer.clear(m_config.clear_color);
    m_renderer.present();

    if (m_config.log_fps) {
      m_fps_log_timer += dt;
      if (m_fps_log_timer >= m_config.fps_log_interval) {
        std::ostringstream oss;
        oss << "FPS: " << m_timer.fps();
        Log::info(oss.str());
        m_fps_log_timer = 0.f;
      }
    }
  }

  Log::info("Shutdown");
  return 0;
}

}  // namespace fury
