#include "fury/heist.hpp"

#include <cmath>
#include <sstream>

namespace fury {
namespace {

float dist_xz(const Vec3& a, const Vec3& b) {
  const float dx = a.x - b.x;
  const float dz = a.z - b.z;
  return std::sqrt(dx * dx + dz * dz);
}

}  // namespace

void HeistController::reset() {
  m_phase = HeistPhase::Idle;
  m_loot_remaining = 0.f;
  m_time_in_phase = 0.f;
}

const char* HeistController::phase_name() const {
  switch (m_phase) {
    case HeistPhase::Idle: return "Idle";
    case HeistPhase::Approaching: return "NearVault";
    case HeistPhase::Looting: return "Looting";
    case HeistPhase::Escaping: return "Escaping";
    case HeistPhase::Complete: return "Complete";
    case HeistPhase::Failed: return "Failed";
  }
  return "?";
}

std::string HeistController::status_line() const {
  std::ostringstream oss;
  oss << "Heist[" << phase_name() << "]";
  if (m_phase == HeistPhase::Looting) {
    oss << " loot=" << m_loot_remaining << "s";
  } else if (m_phase == HeistPhase::Escaping) {
    oss << " reach escape zone";
  } else if (m_phase == HeistPhase::Approaching) {
    oss << " press E to crack vault";
  } else if (m_phase == HeistPhase::Idle) {
    oss << " find Meridian Mutual vault";
  }
  return oss.str();
}

void HeistController::update(const Vec3& player_pos, bool interact_pressed,
                             float dt) {
  m_time_in_phase += dt;
  const float d_vault = dist_xz(player_pos, vault_position);
  const float d_escape = dist_xz(player_pos, escape_position);

  switch (m_phase) {
    case HeistPhase::Idle:
      if (d_vault <= interact_radius) {
        m_phase = HeistPhase::Approaching;
        m_time_in_phase = 0.f;
      }
      break;
    case HeistPhase::Approaching:
      if (d_vault > interact_radius) {
        m_phase = HeistPhase::Idle;
        m_time_in_phase = 0.f;
      } else if (interact_pressed) {
        m_phase = HeistPhase::Looting;
        m_loot_remaining = loot_duration;
        m_time_in_phase = 0.f;
      }
      break;
    case HeistPhase::Looting:
      m_loot_remaining -= dt;
      if (m_loot_remaining <= 0.f) {
        m_phase = HeistPhase::Escaping;
        m_time_in_phase = 0.f;
      } else if (m_time_in_phase > fail_timeout) {
        m_phase = HeistPhase::Failed;
        m_time_in_phase = 0.f;
      }
      break;
    case HeistPhase::Escaping:
      if (d_escape <= escape_radius) {
        m_phase = HeistPhase::Complete;
        m_time_in_phase = 0.f;
      } else if (m_time_in_phase > fail_timeout) {
        m_phase = HeistPhase::Failed;
        m_time_in_phase = 0.f;
      }
      break;
    case HeistPhase::Complete:
    case HeistPhase::Failed:
      if (interact_pressed) {
        reset();
      }
      break;
  }
}

}  // namespace fury
