#pragma once

#include "fury/math.hpp"

#include <string>

namespace fury {

enum class HeistPhase {
  Idle,
  Approach,
  Breach,
  Looting,
  Escape,
  Success,
  Failed,
};

/// Heist state machine for the Vaultline vertical slice.
/// Flow: approach vault → breach → loot timer → escape pad → success/fail.
class HeistController {
 public:
  float approach_radius{5.f};
  float interact_radius{3.5f};
  float breach_duration{2.5f};
  float loot_duration{8.f};
  float escape_radius{4.5f};
  float escape_timeout{50.f};
  float loot_fail_timeout{40.f};

  Vec3 vault_position{0.f, 0.f, 0.f};
  Vec3 escape_position{20.f, 0.f, 20.f};

  void reset();
  void update(const Vec3& player_pos, bool interact_pressed, float dt);

  HeistPhase phase() const { return m_phase; }
  float loot_remaining() const { return m_loot_remaining; }
  float breach_remaining() const { return m_breach_remaining; }
  float time_in_phase() const { return m_time_in_phase; }
  const char* phase_name() const;
  std::string status_line() const;

 private:
  HeistPhase m_phase{HeistPhase::Idle};
  float m_loot_remaining{0.f};
  float m_breach_remaining{0.f};
  float m_time_in_phase{0.f};
};

}  // namespace fury
