#pragma once

#include "fury/math.hpp"

#include <cstdint>

#include <string>
#include <vector>

namespace fury {

enum class NpcKind {
  Civilian,
  Guard,
  Fence,
  Enforcer,  // 3.8.0 rare Syndicate boss stub (high-tier heists)
};

/// 4.4.0 day/night presence & patrol behavior.
enum class NpcSchedule : std::uint8_t {
  Always = 0,
  DayOnly = 1,        // denser daytime civilians; fence Cass open hours
  NightTighten = 2,   // guards — tighter / faster patrol at night
};

/// Lightweight AABB wandering agent following street waypoints.
struct NpcAgent {
  /// Short internal id (e.g. CivA) — also used as fallback label.
  std::string name;
  /// Player-facing display name for nameplates / dialogue (e.g. "Mira Vale").
  std::string display_name;
  NpcKind kind{NpcKind::Civilian};
  Vec3 position{0.f, 0.f, 0.f};
  float yaw{0.f};
  float speed{2.2f};
  float chase_speed{3.4f};
  float radius{0.4f};
  float height{1.8f};
  /// Procedural walk limb phase (radians); advanced by move speed.
  float anim_phase{0.f};
  std::vector<Vec3> waypoints;
  int waypoint_index{0};
  /// When true (guards), move toward chase_target instead of waypoints.
  bool chasing{false};
  Vec3 chase_target{0.f, 0.f, 0.f};
  /// Linked scene entity name for rendering.
  std::string entity_name;
  /// Day/night schedule (4.4.0).
  NpcSchedule schedule{NpcSchedule::Always};
  /// Patrol / spawn home (XZ); used to compress night routes.
  Vec3 home{0.f, 0.f, 0.f};
  /// Captured day route + pace (filled on first apply_schedules).
  std::vector<Vec3> base_waypoints;
  float base_speed{0.f};
  /// Runtime: false when DayOnly and night (hidden / skipped).
  bool on_duty{true};

  const char* label() const {
    if (!display_name.empty()) {
      return display_name.c_str();
    }
    return name.c_str();
  }
};

class NpcSystem {
 public:
  std::vector<NpcAgent>& agents() { return m_agents; }
  const std::vector<NpcAgent>& agents() const { return m_agents; }

  NpcAgent& add(NpcAgent agent);
  /// Update agents. When max_update_dist > 0, skip non-chasing NPCs farther
  /// than that XZ distance from focus (perf). Off-duty agents are skipped.
  void update(float dt, const Vec3& focus = Vec3{}, float max_update_dist = 0.f);
  /// Capture base routes once; hide DayOnly at night; tighten NightTighten patrols.
  void apply_schedules(bool day_segment);

 private:
  std::vector<NpcAgent> m_agents;
};

}  // namespace fury
