#pragma once

#include <cstdint>

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

  float mouse_dx{0.f};
  float mouse_dy{0.f};
};

class Input {
 public:
  /// Poll SDL events + keyboard/mouse state. Returns false if quit requested.
  bool poll(InputState& out);
  void set_mouse_captured(bool captured);
  bool mouse_captured() const { return m_mouse_captured; }

 private:
  bool m_mouse_captured{false};
  bool m_interact_was_down{false};
  bool m_f_was_down{false};
};

}  // namespace fury
