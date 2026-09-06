#pragma once

#include <cstdint>

struct SDL_Renderer;
struct SDL_Window;

namespace fury {

struct Color {
  std::uint8_t r{30};
  std::uint8_t g{30};
  std::uint8_t b{40};
  std::uint8_t a{255};
};

class Renderer {
 public:
  Renderer() = default;
  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  bool create(SDL_Window* window);
  void destroy();

  void clear(const Color& color);
  void present();

  SDL_Renderer* handle() const { return m_renderer; }
  bool valid() const { return m_renderer != nullptr; }

 private:
  SDL_Renderer* m_renderer{nullptr};
};

}  // namespace fury
