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
  m_breach_remaining = 0.f;
  m_time_in_phase = 0.f;
}

const char* HeistController::phase_name() const {
  switch (m_phase) {
    case HeistPhase::Idle: return "Idle";
    case HeistPhase::Approach: return "Approach";
    case HeistPhase::Breach: return "Breach";
    case HeistPhase::Looting: return "Looting";
    case HeistPhase::Escape: return "Escape";
    case HeistPhase::Success: return "Success";
    case HeistPhase::Failed: return "Failed";
  }
  return "?";
}

std::string HeistController::status_line() const {
  std::ostringstream oss;
  oss << "Heist[" << phase_name() << "]";
  switch (m_phase) {
    case HeistPhase::Idle:
      oss << " enter Meridian Mutual / find the vault";
      break;
    case HeistPhase::Approach:
      oss << " press E to breach vault";
      break;
    case HeistPhase::Breach:
      oss << " cracking... " << m_breach_remaining << "s";
      break;
    case HeistPhase::Looting:
      oss << " loot=" << m_loot_remaining << "s";
      break;
    case HeistPhase::Escape:
      oss << " reach extraction pad";
      break;
    case HeistPhase::Success:
      oss << " job complete — E to reset";
      break;
    case HeistPhase::Failed:
      oss << " busted — E to reset";
      break;
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
      if (d_vault <= approach_radius) {
        m_phase = HeistPhase::Approach;
        m_time_in_phase = 0.f;
      }
      break;

    case HeistPhase::Approach:
      if (d_vault > approach_radius) {
        m_phase = HeistPhase::Idle;
        m_time_in_phase = 0.f;
      } else if (interact_pressed && d_vault <= interact_radius) {
        m_phase = HeistPhase::Breach;
        m_breach_remaining = breach_duration;
        m_time_in_phase = 0.f;
      }
      break;

    case HeistPhase::Breach:
      if (d_vault > approach_radius * 1.35f) {
        m_phase = HeistPhase::Failed;
        m_time_in_phase = 0.f;
        break;
      }
      m_breach_remaining -= dt;
      if (m_breach_remaining <= 0.f) {
        m_phase = HeistPhase::Looting;
        m_loot_remaining = loot_duration;
        m_time_in_phase = 0.f;
      }
      break;

    case HeistPhase::Looting:
      m_loot_remaining -= dt;
      if (m_loot_remaining <= 0.f) {
        m_phase = HeistPhase::Escape;
        m_time_in_phase = 0.f;
      } else if (m_time_in_phase > loot_fail_timeout) {
        m_phase = HeistPhase::Failed;
        m_time_in_phase = 0.f;
      }
      break;

    case HeistPhase::Escape:
      if (d_escape <= escape_radius) {
        m_phase = HeistPhase::Success;
        m_time_in_phase = 0.f;
      } else if (m_time_in_phase > escape_timeout) {
        m_phase = HeistPhase::Failed;
        m_time_in_phase = 0.f;
      }
      break;

    case HeistPhase::Success:
    case HeistPhase::Failed:
      if (interact_pressed) {
        reset();
      }
      break;
  }
}

}  // namespace fury
