#include "fury/window.hpp"

#include "fury/log.hpp"

#include <cstdint>

#include <SDL.h>

namespace fury {

Window::~Window() { destroy(); }

bool Window::create(const WindowDesc& desc) {
  destroy();

  if (desc.opengl) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    // 5.5.0 — quality-based MSAA (0/2/4). Must be set before CreateWindow.
    int samples = desc.msaa_samples;
    if (samples < 0) samples = 0;
    if (samples > 4) samples = 4;
    if (samples == 1) samples = 2;
    if (samples == 3) samples = 4;
    if (samples > 0) {
      SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
      SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, samples);
    } else {
      SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
      SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    }
  }

  std::uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
  if (desc.opengl) {
    flags |= SDL_WINDOW_OPENGL;
  }

  int used_msaa = 0;
  if (desc.opengl) {
    int samples = desc.msaa_samples;
    if (samples < 0) samples = 0;
    if (samples > 4) samples = 4;
    if (samples == 1) samples = 2;
    if (samples == 3) samples = 4;
    used_msaa = samples;
  }

  m_window = SDL_CreateWindow(desc.title.c_str(), SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, desc.width, desc.height,
                              flags);
  // xvfb / some GLX setups lack multisample visuals — retry without MSAA.
  if (!m_window && desc.opengl && used_msaa > 0) {
    Log::warn(std::string("SDL_CreateWindow MSAA×") + std::to_string(used_msaa) +
              " failed (" + SDL_GetError() + "); retrying without MSAA");
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    used_msaa = 0;
    m_window = SDL_CreateWindow(desc.title.c_str(), SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, desc.width, desc.height,
                                flags);
  }
  if (!m_window) {
    Log::error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    return false;
  }
  m_width = desc.width;
  m_height = desc.height;
  m_opengl = desc.opengl;
  {
    std::string msg = "Window created: " + desc.title + " (" +
                      std::to_string(desc.width) + "x" +
                      std::to_string(desc.height) + ")";
    if (m_opengl) {
      msg += " [OpenGL]";
      if (used_msaa > 0) {
        msg += " MSAA×" + std::to_string(used_msaa);
      }
    }
    Log::info(msg);
  }
  return true;
}

void Window::destroy() {
  if (m_window) {
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
  }
  m_opengl = false;
}

}  // namespace fury
