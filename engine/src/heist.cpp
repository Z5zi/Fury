#include "fury/heist.hpp"

#include <algorithm>
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
  m_loot_elapsed = 0.f;
  m_last_payout = 0;
  m_inventory.clear_carry();
}

float HeistController::loot_progress() const {
  if (loot_duration <= 1e-4f) {
    return 1.f;
  }
  if (m_phase == HeistPhase::Looting) {
    return std::clamp(1.f - m_loot_remaining / loot_duration, 0.f, 1.f);
  }
  if (m_phase == HeistPhase::Escape || m_phase == HeistPhase::Success) {
    return 1.f;
  }
  return 0.f;
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
  oss << "Heist[" << phase_name() << "] cash=$" << m_inventory.cash
      << " score=" << m_score.lifetime_cash;
  switch (m_phase) {
    case HeistPhase::Idle:
      oss << " | find a vault / display case";
      break;
    case HeistPhase::Approach:
      oss << " | press E to breach";
      break;
    case HeistPhase::Breach:
      oss << " | cracking... " << m_breach_remaining << "s";
      break;
    case HeistPhase::Looting:
      oss << " | loot=" << m_loot_remaining << "s bags=" << m_inventory.loot_bags;
      break;
    case HeistPhase::Escape:
      oss << " | reach extraction pad";
      break;
    case HeistPhase::Success:
      oss << " | +" << m_last_payout << " — E to reset";
      break;
    case HeistPhase::Failed:
      oss << " | busted — E to reset";
      break;
  }
  return oss.str();
}

void HeistController::finalize_success() {
  const float speed_ratio =
      (loot_duration > 1e-4f)
          ? std::clamp(1.f - (m_loot_elapsed / (loot_duration + 12.f)), 0.f, 1.f)
          : 0.f;
  m_score.base_payout = base_payout + jewelry_bonus + m_inventory.carry_value();
  m_score.speed_bonus = static_cast<int>(2500.f * speed_ratio);
  m_score.stealth_bonus = 0;
  m_last_payout = m_score.last_total();
  m_inventory.cash += m_last_payout;
  m_score.lifetime_cash += m_last_payout;
  ++m_score.successes;
  m_inventory.clear_carry();
}

void HeistController::finalize_fail() {
  m_last_payout = 0;
  m_score.base_payout = 0;
  m_score.speed_bonus = 0;
  ++m_score.failures;
  m_inventory.clear_carry();
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
        finalize_fail();
        break;
      }
      m_breach_remaining -= dt;
      if (m_breach_remaining <= 0.f) {
        m_phase = HeistPhase::Looting;
        m_loot_remaining = loot_duration;
        m_loot_elapsed = 0.f;
        m_inventory.loot_bags = 1;
        if (jewelry_bonus > 0) {
          m_inventory.jewelry = 2;
        }
        m_time_in_phase = 0.f;
      }
      break;

    case HeistPhase::Looting:
      m_loot_remaining -= dt;
      m_loot_elapsed += dt;
      if (m_loot_remaining <= 0.f) {
        m_phase = HeistPhase::Escape;
        m_loot_remaining = 0.f;
        m_inventory.loot_bags = std::max(m_inventory.loot_bags, 2);
        m_time_in_phase = 0.f;
      } else if (m_time_in_phase > loot_fail_timeout) {
        m_phase = HeistPhase::Failed;
        m_time_in_phase = 0.f;
        finalize_fail();
      }
      break;

    case HeistPhase::Escape:
      if (d_escape <= escape_radius) {
        m_phase = HeistPhase::Success;
        m_time_in_phase = 0.f;
        finalize_success();
      } else if (m_time_in_phase > escape_timeout) {
        m_phase = HeistPhase::Failed;
        m_time_in_phase = 0.f;
        finalize_fail();
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
