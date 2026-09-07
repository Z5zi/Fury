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

void Input::set_text_entry(bool active) {
  if (active == m_text_entry) return;
  m_text_entry = active;
  if (active) {
    if (m_mouse_captured) {
      set_mouse_captured(false);
    }
    SDL_StartTextInput();
  } else {
    SDL_StopTextInput();
  }
}

void Input::set_cinematic(bool active) { m_cinematic = active; }

bool Input::poll(InputState& out) {
  out.escape_pressed = false;
  out.interact_pressed = false;
  out.key_enter = false;
  out.key_y = false;
  out.key_k = false;
  out.key_backspace = false;
  out.text_chars.clear();
  out.mouse_dx = 0.f;
  out.mouse_dy = 0.f;
  out.mouse_captured = m_mouse_captured;

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      out.quit_requested = true;
    } else if (event.type == SDL_TEXTINPUT && m_text_entry) {
      out.text_chars += event.text.text;
    } else if (event.type == SDL_KEYDOWN) {
      if (event.key.keysym.sym == SDLK_ESCAPE) {
        if (m_text_entry) {
          // Cancel text entry — do not quit.
          out.escape_pressed = true;
        } else if (m_cinematic) {
          // Cutscene skip — do not release mouse or quit.
          out.escape_pressed = true;
        } else if (m_mouse_captured) {
          set_mouse_captured(false);
        } else {
          out.escape_pressed = true;
          out.quit_requested = true;
        }
      } else if (event.key.keysym.sym == SDLK_e && event.key.repeat == 0 &&
                 !m_text_entry) {
        out.interact_pressed = true;
      } else if (event.key.keysym.sym == SDLK_RETURN &&
                 event.key.repeat == 0) {
        out.key_enter = true;
      } else if (event.key.keysym.sym == SDLK_BACKSPACE &&
                 event.key.repeat == 0 && m_text_entry) {
        out.key_backspace = true;
      }
    } else if (event.type == SDL_MOUSEBUTTONDOWN) {
      if (event.button.button == SDL_BUTTON_LEFT && !m_mouse_captured &&
          !m_text_entry) {
        set_mouse_captured(true);
      }
    } else if (event.type == SDL_MOUSEMOTION && m_mouse_captured &&
               !m_text_entry) {
      out.mouse_dx += static_cast<float>(event.motion.xrel);
      out.mouse_dy += static_cast<float>(event.motion.yrel);
    }
  }

  const Uint8* keys = SDL_GetKeyboardState(nullptr);
  if (m_text_entry || m_cinematic) {
    out.key_w = out.key_a = out.key_s = out.key_d = false;
    out.key_space = out.key_ctrl = out.key_shift = false;
    out.key_f = false;
    out.key_v = false;
    out.mouse_dx = 0.f;
    out.mouse_dy = 0.f;
    m_f_was_down = keys[SDL_SCANCODE_F] != 0;
    m_v_was_down = keys[SDL_SCANCODE_V] != 0;
    m_y_was_down = keys[SDL_SCANCODE_Y] != 0;
    m_k_was_down = keys[SDL_SCANCODE_K] != 0;
    m_enter_was_down = keys[SDL_SCANCODE_RETURN] != 0 ||
                       keys[SDL_SCANCODE_KP_ENTER] != 0;
  } else {
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

    const bool v_down = keys[SDL_SCANCODE_V] != 0;
    out.key_v = v_down && !m_v_was_down;
    m_v_was_down = v_down;

    const bool y_down = keys[SDL_SCANCODE_Y] != 0;
    // Prefer KEYDOWN Return for enter; also edge from scancode if missed.
    if (!out.key_enter) {
      const bool enter_down = keys[SDL_SCANCODE_RETURN] != 0 ||
                              keys[SDL_SCANCODE_KP_ENTER] != 0;
      out.key_enter = enter_down && !m_enter_was_down;
      m_enter_was_down = enter_down;
    } else {
      m_enter_was_down = true;
    }
    out.key_y = y_down && !m_y_was_down;
    m_y_was_down = y_down;

    const bool k_down = keys[SDL_SCANCODE_K] != 0;
    out.key_k = k_down && !m_k_was_down;
    m_k_was_down = k_down;
  }

  return !out.quit_requested;
}

}  // namespace fury
