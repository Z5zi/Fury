#include "fury/traffic.hpp"

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

void TrafficSystem::configure(std::vector<TrafficCar> cars) {
  m_cars = std::move(cars);
  for (auto& car : m_cars) {
    if (car.waypoints.empty()) {
      car.active = false;
      continue;
    }
    car.waypoint_index = 0;
    car.position = car.waypoints[0];
    car.position.y = 0.85f;
    car.current_speed = car.cruise_speed * 0.6f;
    if (car.waypoints.size() > 1) {
      const Vec3& n = car.waypoints[1];
      car.yaw = std::atan2(n.x - car.position.x, n.z - car.position.z);
    }
  }
}

void TrafficSystem::update(float dt, const Vec3& player_pos) {
  if (dt <= 0.f) {
    return;
  }
  for (auto& car : m_cars) {
    if (!car.active || car.waypoints.empty()) {
      continue;
    }
    const int n = static_cast<int>(car.waypoints.size());
    if (car.waypoint_index < 0 || car.waypoint_index >= n) {
      car.waypoint_index = 0;
    }
    const Vec3& target = car.waypoints[static_cast<std::size_t>(car.waypoint_index)];
    const float d_wp = dist_xz(car.position, target);
    if (d_wp < 1.2f) {
      car.waypoint_index = (car.waypoint_index + 1) % n;
      continue;
    }

    const float dx = target.x - car.position.x;
    const float dz = target.z - car.position.z;
    const float inv = 1.f / d_wp;
    const float dir_x = dx * inv;
    const float dir_z = dz * inv;
    car.yaw = std::atan2(dir_x, dir_z);

    // Simple player avoid: if close and roughly ahead / overlapping, brake.
    float desired = car.cruise_speed;
    const float d_player = dist_xz(car.position, player_pos);
    if (d_player < stop_radius) {
      desired = 0.f;
    } else if (d_player < avoid_radius) {
      const float to_px = player_pos.x - car.position.x;
      const float to_pz = player_pos.z - car.position.z;
      const float ahead = to_px * dir_x + to_pz * dir_z;
      if (ahead > -1.5f) {
        const float t =
            (d_player - stop_radius) / (avoid_radius - stop_radius + 1e-3f);
        desired = car.cruise_speed * std::clamp(t, 0.f, 1.f) * 0.45f;
      }
    }

    if (car.current_speed < desired) {
      car.current_speed =
          (std::min)(desired, car.current_speed + accel * dt);
    } else {
      car.current_speed =
          (std::max)(desired, car.current_speed - brake * dt);
    }

    const float step = car.current_speed * dt;
    car.position.x += dir_x * step;
    car.position.z += dir_z * step;
    car.position.y = 0.85f;
  }
}

}  // namespace fury
