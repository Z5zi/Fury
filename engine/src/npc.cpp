#include "fury/npc.hpp"

#include <cmath>

namespace fury {
namespace {

float dist_xz(const Vec3& a, const Vec3& b) {
  const float dx = a.x - b.x;
  const float dz = a.z - b.z;
  return std::sqrt(dx * dx + dz * dz);
}

void step_toward(NpcAgent& npc, const Vec3& target, float speed, float dt) {
  const float d = dist_xz(npc.position, target);
  if (d < 0.2f) {
    return;
  }
  const float dx = target.x - npc.position.x;
  const float dz = target.z - npc.position.z;
  const float inv = 1.f / d;
  const float step = speed * dt;
  npc.position.x += dx * inv * step;
  npc.position.z += dz * inv * step;
  npc.position.y = npc.height * 0.5f;
  npc.yaw = std::atan2(dx, dz);
  npc.anim_phase += speed * dt * 3.2f;
}

}  // namespace

NpcAgent& NpcSystem::add(NpcAgent agent) {
  m_agents.push_back(std::move(agent));
  return m_agents.back();
}

void NpcSystem::apply_schedules(bool day_segment) {
  for (NpcAgent& npc : m_agents) {
    if (npc.base_waypoints.empty() && !npc.waypoints.empty()) {
      npc.base_waypoints = npc.waypoints;
      npc.base_speed = npc.speed;
      if (std::fabs(npc.home.x) < 1e-4f && std::fabs(npc.home.z) < 1e-4f) {
        Vec3 c{0.f, 0.f, 0.f};
        for (const Vec3& w : npc.base_waypoints) {
          c.x += w.x;
          c.z += w.z;
        }
        const float inv = 1.f / static_cast<float>(npc.base_waypoints.size());
        npc.home = {c.x * inv, 0.f, c.z * inv};
      }
    }
    switch (npc.schedule) {
      case NpcSchedule::DayOnly:
        npc.on_duty = day_segment;
        if (!npc.on_duty) {
          npc.chasing = false;
        }
        break;
      case NpcSchedule::NightTighten: {
        npc.on_duty = true;
        if (npc.base_waypoints.empty()) {
          break;
        }
        if (!day_segment) {
          // Compress patrol toward home + pace up (tighter night watch).
          constexpr float kScale = 0.52f;
          npc.waypoints.clear();
          npc.waypoints.reserve(npc.base_waypoints.size());
          for (const Vec3& w : npc.base_waypoints) {
            npc.waypoints.push_back(
                {npc.home.x + (w.x - npc.home.x) * kScale, w.y,
                 npc.home.z + (w.z - npc.home.z) * kScale});
          }
          npc.speed = npc.base_speed * 1.45f;
          if (npc.waypoint_index < 0 ||
              npc.waypoint_index >= static_cast<int>(npc.waypoints.size())) {
            npc.waypoint_index = 0;
          }
        } else {
          npc.waypoints = npc.base_waypoints;
          npc.speed = npc.base_speed;
        }
        break;
      }
      case NpcSchedule::Always:
      default:
        npc.on_duty = true;
        break;
    }
  }
}

void NpcSystem::update(float dt, const Vec3& focus, float max_update_dist) {
  const float max2 = max_update_dist > 0.f ? max_update_dist * max_update_dist : 0.f;
  for (NpcAgent& npc : m_agents) {
    if (!npc.on_duty) {
      continue;
    }
    if (npc.chasing &&
        (npc.kind == NpcKind::Guard || npc.kind == NpcKind::Enforcer)) {
      step_toward(npc, npc.chase_target, npc.chase_speed, dt);
      continue;
    }
    if (max2 > 0.f) {
      const float dx = npc.position.x - focus.x;
      const float dz = npc.position.z - focus.z;
      if (dx * dx + dz * dz > max2) {
        continue;
      }
    }
    if (npc.waypoints.empty()) {
      continue;
    }
    if (npc.waypoint_index < 0 ||
        npc.waypoint_index >= static_cast<int>(npc.waypoints.size())) {
      npc.waypoint_index = 0;
    }
    const Vec3& target = npc.waypoints[static_cast<std::size_t>(npc.waypoint_index)];
    const float d = dist_xz(npc.position, target);
    if (d < 0.35f) {
      npc.waypoint_index =
          (npc.waypoint_index + 1) % static_cast<int>(npc.waypoints.size());
      continue;
    }
    step_toward(npc, target, npc.speed, dt);
  }
}

}  // namespace fury
