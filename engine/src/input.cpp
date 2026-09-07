#include "fury/input.hpp"

#include "fury/log.hpp"

#include <SDL.h>

#include <algorithm>
#include <string>
#include <cmath>

namespace fury {
namespace {

constexpr float kStickDeadzone = 0.18f;
constexpr float kTriggerThreshold = 0.35f;
// Right-stick → mouse-delta scale per frame (pre mouse_sensitivity).
constexpr float kLookStickScale = 14.f;

float axis_norm(Sint16 raw) {
  const float v = static_cast<float>(raw) / 32767.f;
  return std::clamp(v, -1.f, 1.f);
}

float apply_deadzone(float v, float dz) {
  const float a = std::fabs(v);
  if (a < dz) return 0.f;
  const float sign = (v < 0.f) ? -1.f : 1.f;
  return sign * ((a - dz) / (1.f - dz));
}

}  // namespace

Input::Input() = default;

Input::~Input() { close_controller(); }

void Input::open_controller(int joystick_index) {
  if (m_controller) return;
  if (!SDL_IsGameController(joystick_index)) return;
  SDL_GameController* pad = SDL_GameControllerOpen(joystick_index);
  if (!pad) {
    Log::warn(std::string("GameControllerOpen failed: ") + SDL_GetError());
    return;
  }
  m_controller = pad;
  SDL_Joystick* js = SDL_GameControllerGetJoystick(pad);
  m_controller_instance_id = js ? SDL_JoystickInstanceID(js) : -1;
  const char* name = SDL_GameControllerName(pad);
  Log::info(std::string("Gamepad connected: ") + (name ? name : "(unknown)"));
}

void Input::close_controller() {
  if (!m_controller) return;
  SDL_GameControllerClose(static_cast<SDL_GameController*>(m_controller));
  m_controller = nullptr;
  m_controller_instance_id = -1;
  m_pad_a_was = m_pad_y_was = m_pad_start_was = false;
  Log::info("Gamepad disconnected");
}

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

void Input::set_escape_modal(bool active) { m_escape_modal = active; }

void Input::poll_gamepad(InputState& out) {
  out.gamepad_connected = (m_controller != nullptr);
  out.gamepad_y_pressed = false;
  out.gamepad_start_pressed = false;
  out.gamepad_boost = false;
  out.move_forward = 0.f;
  out.move_strafe = 0.f;

  if (!m_controller) {
    m_pad_a_was = m_pad_y_was = m_pad_start_was = false;
    return;
  }

  auto* pad = static_cast<SDL_GameController*>(m_controller);

  // Always track Start / Y edges so modals (settings / map) can close.
  const bool y_down =
      SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y) != 0;
  const bool start_down =
      SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START) != 0;
  if (y_down && !m_pad_y_was) out.gamepad_y_pressed = true;
  m_pad_y_was = y_down;
  if (start_down && !m_pad_start_was) out.gamepad_start_pressed = true;
  m_pad_start_was = start_down;

  // Soft locks: no move / look / face gameplay buttons.
  if (m_text_entry || m_cinematic) {
    const bool a_down =
        SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) != 0;
    m_pad_a_was = a_down;
    return;
  }

  const float lx =
      apply_deadzone(axis_norm(SDL_GameControllerGetAxis(
                         pad, SDL_CONTROLLER_AXIS_LEFTX)),
                     kStickDeadzone);
  const float ly =
      apply_deadzone(axis_norm(SDL_GameControllerGetAxis(
                         pad, SDL_CONTROLLER_AXIS_LEFTY)),
                     kStickDeadzone);
  // SDL Y+: down; game forward is −Y on stick.
  out.move_strafe = lx;
  out.move_forward = -ly;

  const float rx =
      apply_deadzone(axis_norm(SDL_GameControllerGetAxis(
                         pad, SDL_CONTROLLER_AXIS_RIGHTX)),
                     kStickDeadzone);
  const float ry =
      apply_deadzone(axis_norm(SDL_GameControllerGetAxis(
                         pad, SDL_CONTROLLER_AXIS_RIGHTY)),
                     kStickDeadzone);
  out.mouse_dx += rx * kLookStickScale;
  out.mouse_dy += ry * kLookStickScale;

  const float lt =
      axis_norm(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT));
  const float rt = axis_norm(
      SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT));
  if (lt > kTriggerThreshold || rt > kTriggerThreshold) {
    out.gamepad_boost = true;
    out.key_shift = true;
  }

  // Face buttons (Xbox layout via SDL GameController):
  // A interact, B crouch, X sprint (Y/Start already edged above).
  const bool a_down =
      SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) != 0;
  const bool b_down =
      SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B) != 0;
  const bool x_down =
      SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X) != 0;

  if (a_down && !m_pad_a_was) {
    out.interact_pressed = true;
  }
  m_pad_a_was = a_down;

  if (b_down) out.key_ctrl = true;
  if (x_down) out.key_shift = true;
}

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
  out.move_forward = 0.f;
  out.move_strafe = 0.f;
  out.gamepad_y_pressed = false;
  out.gamepad_start_pressed = false;
  out.gamepad_boost = false;
  out.gamepad_connected = (m_controller != nullptr);

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      out.quit_requested = true;
    } else if (event.type == SDL_TEXTINPUT && m_text_entry) {
      out.text_chars += event.text.text;
    } else if (event.type == SDL_CONTROLLERDEVICEADDED) {
      open_controller(event.cdevice.which);
    } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
      if (event.cdevice.which == m_controller_instance_id) {
        close_controller();
      }
    } else if (event.type == SDL_KEYDOWN) {
      if (event.key.keysym.sym == SDLK_ESCAPE) {
        if (m_text_entry) {
          // Cancel text entry — do not quit.
          out.escape_pressed = true;
        } else if (m_cinematic || m_escape_modal) {
          // Cutscene / photo / replay — do not release mouse or quit.
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
      // Cinematic / UI modals (help, map, lobby): keep cursor free for clicks.
      if (event.button.button == SDL_BUTTON_LEFT && !m_mouse_captured &&
          !m_text_entry && !m_cinematic) {
        set_mouse_captured(true);
      }
    } else if (event.type == SDL_MOUSEMOTION && m_mouse_captured &&
               !m_text_entry) {
      out.mouse_dx += static_cast<float>(event.motion.xrel);
      out.mouse_dy += static_cast<float>(event.motion.yrel);
    }
  }

  // Hot-plug: open first available pad if none yet.
  if (!m_controller) {
    const int n = SDL_NumJoysticks();
    for (int i = 0; i < n; ++i) {
      if (SDL_IsGameController(i)) {
        open_controller(i);
        break;
      }
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
    out.move_forward = 0.f;
    out.move_strafe = 0.f;
    m_f_was_down = keys[SDL_SCANCODE_F] != 0;
    m_v_was_down = keys[SDL_SCANCODE_V] != 0;
    m_y_was_down = keys[SDL_SCANCODE_Y] != 0;
    m_k_was_down = keys[SDL_SCANCODE_K] != 0;
    m_enter_was_down = keys[SDL_SCANCODE_RETURN] != 0 ||
                       keys[SDL_SCANCODE_KP_ENTER] != 0;
    // Start / Y still edged for closing settings / map-board cycle.
    poll_gamepad(out);
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

    poll_gamepad(out);
  }

  return !out.quit_requested;
}

}  // namespace fury
