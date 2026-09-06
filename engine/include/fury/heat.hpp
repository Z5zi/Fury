#pragma once

#include "fury/heist.hpp"
#include "fury/math.hpp"

namespace fury {

/// Wanted / heat meter for Vaultline. Rises near guards during Breach/Looting,
/// decays when hidden or after extract; max heat can fail the job.
class HeatMeter {
 public:
  float value{0.f};  // 0..1
  // 1.0.0 balance: slower rise / faster decay so Meridian is reliably completable
  float rise_rate{0.16f};
  float decay_rate{0.14f};
  float escape_decay_rate{0.09f};
  float guard_radius{8.5f};
  float max_fail_threshold{1.f};

  void reset() { value = 0.f; }

  /// Update heat. Returns true if heat hit max and the heist should fail.
  bool update(float dt, HeistPhase phase, const Vec3& player_pos,
              const Vec3& guard_pos, bool player_hidden);

  float normalized() const { return value; }
  bool is_max() const { return value >= max_fail_threshold - 1e-4f; }
};

}  // namespace fury
