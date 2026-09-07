#pragma once

#include "fury/math.hpp"

#include <string>
#include <vector>

namespace fury {

/// Patrol car agent used by Vaultline heat pursuits (box-mesh driven).
struct PatrolCar {
  std::string entity_name;
  Vec3 position{0.f, 0.9f, 0.f};
  float yaw{0.f};
  float speed{11.f};
  bool active{false};
  float contact_cooldown{0.f};
  /// Spawn index 0..1 for staggered approach from different edges.
  int spawn_slot{0};
};

/// Spawns 1–2 patrol cars when heat/alarm is high; they chase the player.
/// Contact bumps heat; lose by distance, getaway van, or safehouse.
class PursuitSystem {
 public:
  float spawn_heat{0.55f};
  float despawn_heat{0.28f};
  float lose_distance{52.f};
  float van_lose_distance{38.f};
  float contact_radius{3.8f};
  float contact_heat{0.07f};
  float contact_cooldown{1.1f};
  int max_cars{2};

  std::vector<PatrolCar>& cars() { return m_cars; }
  const std::vector<PatrolCar>& cars() const { return m_cars; }

  int active_count() const;
  bool any_active() const { return active_count() > 0; }

  /// Prepare car slots (call once after creating scene entities).
  void configure(std::vector<PatrolCar> cars);

  /// Update pursuits. Returns heat to add from bumper contact this frame.
  /// `in_van` / `in_safehouse` help the player shake the chase.
  float update(float dt, const Vec3& player_pos, float heat_norm, bool alarm_active,
               bool in_van, bool in_safehouse);

 private:
  std::vector<PatrolCar> m_cars;
  float m_spawn_timer{0.f};
  bool m_logged_spawn{false};

  void activate_car(PatrolCar& car, const Vec3& player_pos, int slot);
  void deactivate_car(PatrolCar& car);
};

}  // namespace fury
