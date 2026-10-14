#pragma once

#include "fury/npc.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace fury {

/// AI heist crew stub — follows the player with lateral offsets during a job.
struct CrewMember {
  std::string name;
  /// Player-facing label for nameplates / Q dialogue.
  std::string display_name;
  std::string entity_name;
  Vec3 position{0.f, 0.85f, 0.f};
  float yaw{0.f};
  /// Player-local offset: +x right, +z forward (negative z trails behind).
  Vec3 follow_offset{-1.6f, 0.f, -1.2f};
  float follow_speed{6.5f};
  float height{1.7f};
  /// Procedural walk limb phase (radians).
  float anim_phase{0.f};
  /// Idle breathe phase (radians).
  float breathe_phase{0.f};
  /// Smoothed [0,1] walk weight for idle↔walk blend / foot plant.
  float move_weight{0.f};
  bool active{true};

  /// Same measured motion contract as NpcAgent (meters, seconds, radians).
  Vec3 velocity{};
  float actual_speed{0.f};
  float turn_rate{0.f};
  double travel_distance{0.0};
  float locomotion_speed{0.f};
  float yaw_speed{0.f};

  const char* label() const {
    if (!display_name.empty()) return display_name.c_str();
    return name.c_str();
  }
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
  /// player_yaw follows Camera: zero faces +X, positive yaw turns toward +Z.
  /// Invalid dt is a no-op; valid hitches share the NPC 0.25s simulation cap.
  void update(float dt, const Vec3& player_pos, float player_yaw, bool following) {
    using namespace locomotion_detail;
    if (!valid_dt(dt)) return;
    dt = update_time(dt);
    const int steps = static_cast<int>(std::ceil(dt / kMaxStep));
    const float step_dt = dt / static_cast<float>(steps);
    const float cy = std::cos(player_yaw);
    const float sy = std::sin(player_yaw);
    for (auto& c : m_members) {
      if (!c.active) {
        clear_motion(c);
        c.move_weight = 0.f;
        continue;
      }
      ground_and_breathe(c, dt);
      if (!following) {
        idle(c, dt);
        continue;
      }
      // Camera's planar basis is forward=(cos,sin), right=(-sin,cos).
      // Keep the authored offsets, including their negative trailing z.
      const Vec3 target{player_pos.x - sy * c.follow_offset.x + cy * c.follow_offset.z,
                        c.height * .5f,
                        player_pos.z + cy * c.follow_offset.x + sy * c.follow_offset.z};
      const Vec3 start = c.position;
      const double distance_before = c.travel_distance;
      float yaw_change = 0.f;
      for (int step = 0; step < steps; ++step) {
        yaw_change += step_toward(c, target, c.follow_speed, 0.f, step_dt, true);
      }
      finish_update(c, start, distance_before, yaw_change, dt);
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
    return 1.f + 0.175f * static_cast<float>((std::min)(n, 2));
  }

 private:
  std::vector<CrewMember> m_members;
};

}  // namespace fury
