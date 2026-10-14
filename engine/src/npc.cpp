#include "fury/npc.hpp"

#include <algorithm>
#include <cmath>

namespace fury {
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
          locomotion_detail::clear_motion(npc);
          npc.move_weight = 0.f;
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
        } else {
          npc.waypoints = npc.base_waypoints;
          npc.speed = npc.base_speed;
        }
        if (npc.waypoint_index < 0 ||
            npc.waypoint_index >= static_cast<int>(npc.waypoints.size())) {
          npc.waypoint_index = 0;
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

void NpcSystem::update(float dt, float chase_target_radius,
                       const Vec3& focus, float max_update_dist) {
  using namespace locomotion_detail;
  if (!valid_dt(dt)) return;
  dt = update_time(dt);
  const int steps = static_cast<int>(std::ceil(dt / kMaxStep));
  const float step_dt = dt / static_cast<float>(steps);
  const float max2 = max_update_dist > 0.f ? max_update_dist * max_update_dist : 0.f;
  for (NpcAgent& npc : m_agents) {
    if (!npc.on_duty) continue;
    ground_and_breathe(npc, dt);
    const bool chase = npc.chasing &&
        (npc.kind == NpcKind::Guard || npc.kind == NpcKind::Enforcer);
    const float chase_stop = chase_target_radius + npc.radius;
    if (chase && (!std::isfinite(chase_stop) ||
                  chase_target_radius < 0.f || npc.radius < 0.f)) {
      idle(npc, dt);
      continue;
    }
    if (!chase && max2 > 0.f) {
      const float dx = npc.position.x - focus.x;
      const float dz = npc.position.z - focus.z;
      if (dx * dx + dz * dz > max2) {
        idle(npc, dt);
        continue;
      }
    }
    if (!chase && npc.waypoints.empty()) {
      idle(npc, dt);
      continue;
    }
    const Vec3 start = npc.position;
    const double distance_before = npc.travel_distance;
    float yaw_change = 0.f;
    for (int step = 0; step < steps; ++step) {
      if (chase) {
        yaw_change += step_toward(npc, npc.chase_target, npc.chase_speed,
                                 chase_stop, step_dt, true);
        continue;
      }
      if (npc.waypoint_index < 0 ||
          npc.waypoint_index >= static_cast<int>(npc.waypoints.size())) {
        npc.waypoint_index = 0;
      }
      // Consume already reached/duplicate points in the same substep. The
      // bound also handles a route made entirely of coincident points.
      std::size_t reached = 0;
      while (reached < npc.waypoints.size() &&
             distance_xz(npc.position, npc.waypoints[static_cast<std::size_t>(npc.waypoint_index)])
                 <= kArrivalEpsilon) {
        npc.waypoint_index = (npc.waypoint_index + 1) % static_cast<int>(npc.waypoints.size());
        ++reached;
      }
      if (reached == npc.waypoints.size()) {
        idle(npc, step_dt);
        continue;
      }
      const Vec3& target = npc.waypoints[static_cast<std::size_t>(npc.waypoint_index)];
      yaw_change += step_toward(npc, target, npc.speed, 0.f, step_dt);
    }
    finish_update(npc, start, distance_before, yaw_change, dt);
  }
}

}  // namespace fury
