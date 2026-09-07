#pragma once

#include "fury/math.hpp"

#include <string>
#include <vector>

namespace fury {

/// Civilian traffic car — loops street waypoints (not pursuit).
struct TrafficCar {
  std::string entity_name;
  Vec3 position{0.f, 0.85f, 0.f};
  float yaw{0.f};
  float cruise_speed{7.5f};
  float current_speed{0.f};
  std::vector<Vec3> waypoints;
  int waypoint_index{0};
  bool active{true};
};

/// 4–8 civilian cars looping roads; slow/stop near the player (simple avoid).
class TrafficSystem {
 public:
  float avoid_radius{7.5f};
  float stop_radius{3.6f};
  float accel{9.f};
  float brake{18.f};

  std::vector<TrafficCar>& cars() { return m_cars; }
  const std::vector<TrafficCar>& cars() const { return m_cars; }

  void configure(std::vector<TrafficCar> cars);
  void update(float dt, const Vec3& player_pos);

 private:
  std::vector<TrafficCar> m_cars;
};

}  // namespace fury
