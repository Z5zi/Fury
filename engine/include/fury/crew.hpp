#pragma once

#include "fury/math.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace fury {

/// AI heist crew stub — follows the player with lateral offsets during a job.
struct CrewMember {
  std::string name;
  std::string entity_name;
  Vec3 position{0.f, 0.9f, 0.f};
  float yaw{0.f};
  /// Lateral/back offset in player-local XZ (right, back).
  Vec3 follow_offset{-1.6f, 0.f, -1.2f};
  float follow_speed{6.5f};
  bool active{true};
};

class CrewSystem {
 public:
  static constexpr std::size_t kMaxCrew = 2;

  std::vector<CrewMember>& members() { return m_members; }
  const std::vector<CrewMember>& members() const { return m_members; }

  CrewMember& add(CrewMember m) {
    if (m_members.size() >= kMaxCrew) {
      m_members.back() = std::move(m);
      return m_members.back();
    }
    m_members.push_back(std::move(m));
    return m_members.back();
  }

  /// Follow player when `following` (typically during breach/loot/escape).
  void update(float dt, const Vec3& player_pos, float player_yaw, bool following) {
    const float cy = std::cos(player_yaw);
    const float sy = std::sin(player_yaw);
    for (auto& c : m_members) {
      if (!c.active) {
        continue;
      }
      if (!following) {
        continue;
      }
      // offset: +x = right of facing, +z = behind
      const float ox = c.follow_offset.x;
      const float oz = c.follow_offset.z;
      const Vec3 target{player_pos.x + cy * ox - sy * oz, 0.9f,
                        player_pos.z + sy * ox + cy * oz};
      const Vec3 delta = target - c.position;
      const float dist = std::sqrt(delta.x * delta.x + delta.z * delta.z);
      if (dist > 1e-3f) {
        const float step = std::min(dist, c.follow_speed * dt);
        c.position.x += (delta.x / dist) * step;
        c.position.z += (delta.z / dist) * step;
        c.position.y = 0.9f;
        c.yaw = std::atan2(delta.x, delta.z);
      }
    }
  }

  /// How many active crew are within `radius` of `pos` (XZ).
  int nearby_count(const Vec3& pos, float radius) const {
    int n = 0;
    const float r2 = radius * radius;
    for (const auto& c : m_members) {
      if (!c.active) {
        continue;
      }
      const float dx = c.position.x - pos.x;
      const float dz = c.position.z - pos.z;
      if (dx * dx + dz * dz <= r2) {
        ++n;
      }
    }
    return n;
  }

  /// Loot timer multiplier: ~1.0 alone, up to ~1.35 with 2 nearby.
  float loot_speed_boost(const Vec3& player_pos, float radius = 5.f) const {
    const int n = nearby_count(player_pos, radius);
    return 1.f + 0.175f * static_cast<float>(std::min(n, 2));
  }

 private:
  std::vector<CrewMember> m_members;
};

}  // namespace fury
