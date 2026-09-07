#include "fury/pursuit.hpp"

#include <algorithm>
#include <cmath>

namespace fury {
namespace {

float dist_xz(const Vec3& a, const Vec3& b) {
  const float dx = a.x - b.x;
  const float dz = a.z - b.z;
  return std::sqrt(dx * dx + dz * dz);
}

void step_toward(PatrolCar& car, const Vec3& target, float speed, float dt) {
  const float d = dist_xz(car.position, target);
  if (d < 0.35f) {
    return;
  }
  const float dx = target.x - car.position.x;
  const float dz = target.z - car.position.z;
  const float inv = 1.f / d;
  const float step = speed * dt;
  car.position.x += dx * inv * step;
  car.position.z += dz * inv * step;
  car.position.y = 0.9f;
  car.yaw = std::atan2(dx, dz);
}

Vec3 spawn_point_for(int slot, const Vec3& player) {
  // Approach from street edges around Harbor Metro.
  if (slot == 0) {
    return {player.x - 28.f, 0.9f, player.z + 22.f};
  }
  return {player.x + 26.f, 0.9f, player.z - 20.f};
}

}  // namespace

void PursuitSystem::configure(std::vector<PatrolCar> cars) {
  m_cars = std::move(cars);
  for (std::size_t i = 0; i < m_cars.size(); ++i) {
    m_cars[i].spawn_slot = static_cast<int>(i);
    m_cars[i].active = false;
  }
}

int PursuitSystem::active_count() const {
  int n = 0;
  for (const auto& c : m_cars) {
    if (c.active) {
      ++n;
    }
  }
  return n;
}

void PursuitSystem::activate_car(PatrolCar& car, const Vec3& player_pos, int slot) {
  car.active = true;
  car.spawn_slot = slot;
  car.position = spawn_point_for(slot, player_pos);
  car.contact_cooldown = 0.4f;
  car.yaw = std::atan2(player_pos.x - car.position.x, player_pos.z - car.position.z);
}

void PursuitSystem::deactivate_car(PatrolCar& car) {
  car.active = false;
  car.contact_cooldown = 0.f;
  // Park far off-map so unused meshes stay out of the frame.
  car.position = {0.f, -40.f, 0.f};
}

float PursuitSystem::update(float dt, const Vec3& player_pos, float heat_norm,
                            bool alarm_active, bool in_van, bool in_safehouse) {
  float heat_add = 0.f;
  const bool want_pursuit =
      !in_safehouse && (alarm_active || heat_norm >= spawn_heat);

  if (in_safehouse) {
    for (auto& car : m_cars) {
      if (car.active) {
        deactivate_car(car);
      }
    }
    m_spawn_timer = 0.f;
    m_logged_spawn = false;
    return 0.f;
  }

  // Spawn up to max_cars when heat/alarm warrants it.
  if (want_pursuit) {
    m_spawn_timer += dt;
    const int desired =
        (heat_norm >= 0.78f || alarm_active) ? max_cars : 1;
    int active = active_count();
    if (active < desired && active < static_cast<int>(m_cars.size()) &&
        m_spawn_timer >= 0.65f) {
      for (auto& car : m_cars) {
        if (!car.active) {
          activate_car(car, player_pos, car.spawn_slot);
          m_spawn_timer = 0.f;
          ++active;
          break;
        }
      }
    }
  } else if (heat_norm <= despawn_heat && !alarm_active) {
    // Cool-off: drop cars gradually when heat falls.
    m_spawn_timer = 0.f;
    for (auto& car : m_cars) {
      if (car.active) {
        deactivate_car(car);
        break;
      }
    }
  }

  const float lose_r = in_van ? van_lose_distance : lose_distance;
  for (auto& car : m_cars) {
    if (!car.active) {
      continue;
    }
    car.contact_cooldown = (std::max)(0.f, car.contact_cooldown - dt);

    const float d = dist_xz(car.position, player_pos);
    if (d >= lose_r) {
      deactivate_car(car);
      continue;
    }

    // Van: slightly slower chase so getaways work.
    float spd = car.speed;
    if (in_van) {
      spd *= 0.82f;
    }
    step_toward(car, player_pos, spd, dt);

    if (d <= contact_radius && car.contact_cooldown <= 0.f) {
      heat_add += contact_heat;
      car.contact_cooldown = contact_cooldown;
    }
  }

  (void)m_logged_spawn;
  return heat_add;
}

}  // namespace fury
