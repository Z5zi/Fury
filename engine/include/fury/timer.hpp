#pragma once

namespace fury {

class Timer {
 public:
  Timer();

  void reset();
  /// Seconds since last tick (or reset); advances the clock.
  float tick();
  float elapsed_seconds() const;
  float delta_seconds() const { return m_delta; }
  float fps() const { return m_fps; }

 private:
  double m_start{0.0};
  double m_last{0.0};
  float m_delta{0.f};
  float m_fps{0.f};
  float m_fps_accum{0.f};
  int m_fps_frames{0};
};

}  // namespace fury
