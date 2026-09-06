#pragma once

namespace fury {

struct InputState {
  bool quit_requested{false};
  bool escape_pressed{false};
};

class Input {
 public:
  /// Poll SDL events into state. Returns false if the OS asked to quit.
  bool poll(InputState& out);
};

}  // namespace fury
