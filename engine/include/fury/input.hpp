#pragma once

#include <cstdint>
#include <string>

namespace fury {

struct InputState {
  bool quit_requested{false};
  bool escape_pressed{false};
  bool interact_pressed{false};  // E / gamepad A — edge-triggered
  bool mouse_captured{false};

  // Held keys (also OR'd from gamepad where noted)
  bool key_w{false};
  bool key_a{false};
  bool key_s{false};
  bool key_d{false};
  bool key_space{false};
  bool key_ctrl{false};   // crouch (walk) / descend (fly); gamepad B
  bool key_shift{false};  // sprint / boost; gamepad X + triggers
  bool key_f{false};      // toggle fly
  bool key_v{false};      // toggle first/third person

  // Edge-triggered helpers (chat / ready)
  bool key_enter{false};
  bool key_y{false};
  bool key_k{false};
  bool key_backspace{false};

  /// Characters typed this frame (when text entry active).
  std::string text_chars;

  float mouse_dx{0.f};
  float mouse_dy{0.f};

  /// Analog move from left stick (−1..1). Combined with WASD in Camera.
  float move_forward{0.f};  // +forward / −back
  float move_strafe{0.f};   // +right / −left

  /// Gamepad present and opened this session.
  bool gamepad_connected{false};

  /// Edge: Y button — map / mission board toggle (app chooses).
  bool gamepad_y_pressed{false};
  /// Edge: Start — settings toggle.
  bool gamepad_start_pressed{false};
  /// Triggers past threshold (optional boost; also ORs into key_shift).
  bool gamepad_boost{false};
};

class Input {
 public:
  Input();
  ~Input();

  Input(const Input&) = delete;
  Input& operator=(const Input&) = delete;

  /// Poll SDL events + keyboard/mouse/gamepad. Returns false if quit requested.
  bool poll(InputState& out);
  void set_mouse_captured(bool captured);
  bool mouse_captured() const { return m_mouse_captured; }

  /// When true: suppress move/look, collect text, Esc cancels without quitting.
  void set_text_entry(bool active);
  bool text_entry() const { return m_text_entry; }

  /// When true: Esc emits escape_pressed without releasing mouse or quitting
  /// (cutscene skip). Also suppresses WASD/look like a soft lock.
  void set_cinematic(bool active);
  bool cinematic() const { return m_cinematic; }

  /// When true: Esc emits escape_pressed without quit (photo / replay scrub).
  /// Does NOT suppress WASD/look — free camera stays interactive.
  void set_escape_modal(bool active);
  bool escape_modal() const { return m_escape_modal; }

 private:
  void open_controller(int joystick_index);
  void close_controller();
  void poll_gamepad(InputState& out);

  bool m_mouse_captured{false};
  bool m_interact_was_down{false};
  bool m_f_was_down{false};
  bool m_v_was_down{false};
  bool m_enter_was_down{false};
  bool m_y_was_down{false};
  bool m_k_was_down{false};
  bool m_text_entry{false};
  bool m_cinematic{false};
  bool m_escape_modal{false};

  // SDL_GameController* kept as void* to avoid leaking SDL into every TU.
  void* m_controller{nullptr};
  int m_controller_instance_id{-1};
  bool m_pad_a_was{false};
  bool m_pad_y_was{false};
  bool m_pad_start_was{false};
};

}  // namespace fury
