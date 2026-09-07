#pragma once

#include <cstdint>
#include <string>

namespace fury {

struct InputState {
  bool quit_requested{false};
  bool escape_pressed{false};
  bool interact_pressed{false};  // E / edge-triggered
  bool mouse_captured{false};

  // Held keys
  bool key_w{false};
  bool key_a{false};
  bool key_s{false};
  bool key_d{false};
  bool key_space{false};
  bool key_ctrl{false};
  bool key_shift{false};
  bool key_f{false};  // toggle fly

  // Edge-triggered helpers (chat / ready)
  bool key_enter{false};
  bool key_y{false};
  bool key_k{false};
  bool key_backspace{false};

  /// Characters typed this frame (when text entry active).
  std::string text_chars;

  float mouse_dx{0.f};
  float mouse_dy{0.f};
};

class Input {
 public:
  /// Poll SDL events + keyboard/mouse state. Returns false if quit requested.
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

 private:
  bool m_mouse_captured{false};
  bool m_interact_was_down{false};
  bool m_f_was_down{false};
  bool m_enter_was_down{false};
  bool m_y_was_down{false};
  bool m_k_was_down{false};
  bool m_text_entry{false};
  bool m_cinematic{false};
};

}  // namespace fury
