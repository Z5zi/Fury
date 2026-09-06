#include "fury/window.hpp"

#include "fury/log.hpp"

#include <SDL.h>

namespace fury {

Window::~Window() { destroy(); }

bool Window::create(const WindowDesc& desc) {
  destroy();
  m_window = SDL_CreateWindow(
      desc.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      desc.width, desc.height, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!m_window) {
    Log::error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    return false;
  }
  m_width = desc.width;
  m_height = desc.height;
  Log::info("Window created: " + desc.title + " (" +
            std::to_string(desc.width) + "x" + std::to_string(desc.height) +
            ")");
  return true;
}

void Window::destroy() {
  if (m_window) {
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
  }
}

}  // namespace fury
