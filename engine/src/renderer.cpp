#include "fury/renderer.hpp"

#include "fury/log.hpp"

#include <SDL.h>

namespace fury {

Renderer::~Renderer() { destroy(); }

bool Renderer::create(SDL_Window* window) {
  destroy();
  if (!window) {
    Log::error("Renderer::create: null window");
    return false;
  }
  m_renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!m_renderer) {
    // Fallback without vsync / acceleration hints
    m_renderer = SDL_CreateRenderer(window, -1, 0);
  }
  if (!m_renderer) {
    Log::error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
    return false;
  }
  Log::info("SDL renderer ready");
  return true;
}

void Renderer::destroy() {
  if (m_renderer) {
    SDL_DestroyRenderer(m_renderer);
    m_renderer = nullptr;
  }
}

void Renderer::clear(const Color& color) {
  if (!m_renderer) {
    return;
  }
  SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
  SDL_RenderClear(m_renderer);
}

void Renderer::present() {
  if (m_renderer) {
    SDL_RenderPresent(m_renderer);
  }
}

}  // namespace fury
