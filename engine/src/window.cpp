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
  }

  std::uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
  if (desc.opengl) {
    flags |= SDL_WINDOW_OPENGL;
  }

  m_window = SDL_CreateWindow(desc.title.c_str(), SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, desc.width, desc.height,
                              flags);
  if (!m_window) {
    Log::error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    return false;
  }
  m_width = desc.width;
  m_height = desc.height;
  m_opengl = desc.opengl;
  Log::info("Window created: " + desc.title + " (" +
            std::to_string(desc.width) + "x" + std::to_string(desc.height) +
            ")" + (m_opengl ? " [OpenGL]" : ""));
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
