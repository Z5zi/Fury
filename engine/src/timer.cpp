#include "fury/timer.hpp"

#include <SDL.h>

namespace fury {

Timer::Timer() { reset(); }

void Timer::reset() {
  m_start = SDL_GetTicks64() / 1000.0;
  m_last = m_start;
  m_delta = 0.f;
  m_fps = 0.f;
  m_fps_accum = 0.f;
  m_fps_frames = 0;
}

float Timer::tick() {
  const double now = SDL_GetTicks64() / 1000.0;
  m_delta = static_cast<float>(now - m_last);
  m_last = now;

  m_fps_accum += m_delta;
  ++m_fps_frames;
  if (m_fps_accum >= 0.5f) {
    m_fps = static_cast<float>(m_fps_frames) / m_fps_accum;
    m_fps_accum = 0.f;
    m_fps_frames = 0;
  }
  return m_delta;
}

float Timer::elapsed_seconds() const {
  return static_cast<float>((SDL_GetTicks64() / 1000.0) - m_start);
}

}  // namespace fury
