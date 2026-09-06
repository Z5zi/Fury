#include "fury/input.hpp"

#include <SDL.h>

namespace fury {

void Input::set_mouse_captured(bool captured) {
  m_mouse_captured = captured;
  SDL_SetRelativeMouseMode(captured ? SDL_TRUE : SDL_FALSE);
  if (captured) {
    SDL_ShowCursor(SDL_DISABLE);
  } else {
    SDL_ShowCursor(SDL_ENABLE);
  }
}

bool Input::poll(InputState& out) {
  out.escape_pressed = false;
  out.interact_pressed = false;
  out.mouse_dx = 0.f;
  out.mouse_dy = 0.f;
  out.mouse_captured = m_mouse_captured;

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      out.quit_requested = true;
    } else if (event.type == SDL_KEYDOWN) {
      if (event.key.keysym.sym == SDLK_ESCAPE) {
        if (m_mouse_captured) {
          set_mouse_captured(false);
        } else {
          out.escape_pressed = true;
          out.quit_requested = true;
        }
      } else if (event.key.keysym.sym == SDLK_e && event.key.repeat == 0) {
        out.interact_pressed = true;
      }
    } else if (event.type == SDL_MOUSEBUTTONDOWN) {
      if (event.button.button == SDL_BUTTON_LEFT && !m_mouse_captured) {
        set_mouse_captured(true);
      }
    } else if (event.type == SDL_MOUSEMOTION && m_mouse_captured) {
      out.mouse_dx += static_cast<float>(event.motion.xrel);
      out.mouse_dy += static_cast<float>(event.motion.yrel);
    }
  }

  const Uint8* keys = SDL_GetKeyboardState(nullptr);
  out.key_w = keys[SDL_SCANCODE_W];
  out.key_a = keys[SDL_SCANCODE_A];
  out.key_s = keys[SDL_SCANCODE_S];
  out.key_d = keys[SDL_SCANCODE_D];
  out.key_space = keys[SDL_SCANCODE_SPACE];
  out.key_ctrl = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL];
  out.key_shift = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];

  const bool f_down = keys[SDL_SCANCODE_F] != 0;
  out.key_f = f_down && !m_f_was_down;
  m_f_was_down = f_down;

  return !out.quit_requested;
}

}  // namespace fury
