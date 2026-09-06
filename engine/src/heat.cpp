#include "fury/heat.hpp"

#include <algorithm>
#include <cmath>

namespace fury {
namespace {

float dist_xz(const Vec3& a, const Vec3& b) {
  const float dx = a.x - b.x;
  const float dz = a.z - b.z;
  return std::sqrt(dx * dx + dz * dz);
}

}  // namespace

bool HeatMeter::update(float dt, HeistPhase phase, const Vec3& player_pos,
                       const Vec3& guard_pos, bool player_hidden) {
  const float d_guard = dist_xz(player_pos, guard_pos);
  const bool near_guard = d_guard <= guard_radius;
  const bool risky = phase == HeistPhase::Breach || phase == HeistPhase::Looting;

  if (risky && near_guard && !player_hidden) {
    value += rise_rate * dt;
  } else if (phase == HeistPhase::Escape) {
    // Still warm during escape; decays slowly unless near guard.
    if (near_guard && !player_hidden) {
      value += rise_rate * 0.55f * dt;
    } else {
      value -= escape_decay_rate * dt;
    }
  } else if (phase == HeistPhase::Success || phase == HeistPhase::Failed ||
             phase == HeistPhase::Idle || player_hidden) {
    value -= decay_rate * (player_hidden ? 1.6f : 1.f) * dt;
  } else if (!near_guard) {
    value -= decay_rate * 0.5f * dt;
  }

  value = std::clamp(value, 0.f, 1.f);

  if (risky && is_max()) {
    return true;
  }
  // Escape with max heat: harder — still signal fail if stuck at max too long
  // handled by caller via shortened timeout; here only fail on risky max.
  return false;
}

}  // namespace fury
