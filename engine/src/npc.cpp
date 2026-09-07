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

void NpcSystem::update(float dt, const Vec3& focus, float max_update_dist) {
  const float max2 = max_update_dist > 0.f ? max_update_dist * max_update_dist : 0.f;
  for (NpcAgent& npc : m_agents) {
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
