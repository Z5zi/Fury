#pragma once

#include <string>

struct SDL_Window;

namespace fury {

struct WindowDesc {
  std::string title{"Fury"};
  int width{1280};
  int height{720};
  bool opengl{false};
  bool resizable{true};
  /// Requested MSAA samples for OpenGL pixel format (0/2/4). Soft path ignores.
  int msaa_samples{0};
};

class Window {
 public:
  Window() = default;
  ~Window();

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  bool create(const WindowDesc& desc);
  void destroy();
  /// Refresh dimensions after SDL resize events; true only when changed.
  bool sync_size();

  SDL_Window* handle() const { return m_window; }
  int width() const { return m_width; }
  int height() const { return m_height; }
  bool valid() const { return m_window != nullptr; }
  bool opengl() const { return m_opengl; }

 private:
  SDL_Window* m_window{nullptr};
  int m_width{0};
  int m_height{0};
  bool m_opengl{false};
};

}  // namespace fury
