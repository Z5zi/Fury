#pragma once

#include "fury/math.hpp"

#include <string>

namespace fury {

enum class HeistPhase {
  Idle,
  Approaching,
  Looting,
  Escaping,
  Complete,
  Failed,
};

/// Tiny single-player heist state machine for the Vaultline vertical slice.
class HeistController {
 public:
  float interact_radius{3.5f};
  float loot_duration{8.f};
  float escape_radius{4.f};
  float fail_timeout{45.f};

  Vec3 vault_position{0.f, 0.f, 0.f};
  Vec3 escape_position{20.f, 0.f, 20.f};

  void reset();
  void update(const Vec3& player_pos, bool interact_pressed, float dt);

  HeistPhase phase() const { return m_phase; }
  float loot_remaining() const { return m_loot_remaining; }
  float time_in_phase() const { return m_time_in_phase; }
  const char* phase_name() const;
  std::string status_line() const;

 private:
  HeistPhase m_phase{HeistPhase::Idle};
  float m_loot_remaining{0.f};
  float m_time_in_phase{0.f};
};

}  // namespace fury
