#pragma once

#include "fury/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <string>
#include <vector>
#include <utility>

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

/// Grounded wandering agent following authored street waypoints.
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
  /// Legacy walk phase (radians); advanced only by actual XZ travel.
  float anim_phase{0.f};
  /// Idle breathe phase (radians); always advances while on duty.
  float breathe_phase{0.f};
  /// Smoothed [0,1] walk weight for idle↔walk blend / foot plant.
  float move_weight{0.f};
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

  /// Presentation signals from the last simulated update, not requested speed.
  Vec3 velocity{};                 // net XZ displacement / simulated seconds
  float actual_speed{0.f};         // path distance / simulated seconds (m/s)
  float turn_rate{0.f};            // signed heading change / simulated seconds
  double travel_distance{0.0};     // cumulative actual XZ path distance (meters)
  /// Integration state; renderers should use the signals above.
  float locomotion_speed{0.f};
  float yaw_speed{0.f};

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
  /// Nonpositive/nonfinite dt is a no-op. Hitches simulate at most 0.25s in
  /// <=1/60s steps, never a catch-up teleport along the patrol route.
  /// Chasing Guard/Enforcer arrival uses chase_target_radius + agent.radius.
  /// Pass the actual target's configured radius (zero for a point target).
  /// Invalid radii idle chasing agents. Arrival never pushes apart an existing
  /// overlap; it only bounds a threat's own approach from outside the buffer.
  void update(float dt, float chase_target_radius,
              const Vec3& focus = Vec3{}, float max_update_dist = 0.f);
  /// Capture base routes once; hide DayOnly at night; tighten NightTighten patrols.
  void apply_schedules(bool day_segment);

 private:
  std::vector<NpcAgent> m_agents;
};

// Shared integration for NPC patrol/chase and crew follow. Translation always
// stays on the straight line to the authored/current target; turning affects
// pace, never bends the route through adjacent walls or door footprints.
namespace locomotion_detail {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTau = 2.f * kPi;
constexpr float kMaxUpdate = .25f;
constexpr float kMaxStep = 1.f / 60.f;
constexpr float kArrivalEpsilon = 1.e-3f;

inline bool valid_dt(float dt) { return std::isfinite(dt) && dt > 0.f; }
inline float update_time(float dt) { return (std::min)(dt, kMaxUpdate); }
inline float approach(float value, float goal, float limit) {
  return value + std::clamp(goal - value, -limit, limit);
}
inline float distance_xz(const Vec3& a, const Vec3& b) {
  return std::hypot(b.x - a.x, b.z - a.z);
}
inline float wrap_angle(float angle) { return std::remainder(angle, kTau); }
inline float advance_phase(float phase, float delta) {
  return std::fmod(phase + delta, kTau);
}

template <typename Agent>
void clear_motion(Agent& agent) {
  agent.velocity = {};
  agent.actual_speed = 0.f;
  agent.turn_rate = 0.f;
  agent.locomotion_speed = 0.f;
  agent.yaw_speed = 0.f;
}

template <typename Agent>
void idle(Agent& agent, float dt) {
  clear_motion(agent);
  agent.move_weight = approach(agent.move_weight, 0.f, dt * 4.f);
}

template <typename Agent>
void ground_and_breathe(Agent& agent, float dt) {
  agent.position.y = agent.height * .5f;
  agent.breathe_phase = advance_phase(agent.breathe_phase, dt * 2.2f);
}

/// Returns the actual signed yaw change; callers accumulate it across substeps.
/// Arrival braking uses remaining distance, and all translation is clamped so
/// a moved chase/follow target can never cause an overshoot, even mid-brake.
template <typename Agent>
float step_toward(Agent& agent, const Vec3& target, float cruise_speed,
                  float stop_radius, float dt, bool quick = false) {
  const float distance = distance_xz(agent.position, target);
  if (!std::isfinite(distance) || !std::isfinite(cruise_speed) ||
      cruise_speed <= 0.f || distance <= stop_radius + kArrivalEpsilon) {
    idle(agent, dt);
    return 0.f;
  }
  const float dx = (target.x - agent.position.x) / distance;
  const float dz = (target.z - agent.position.z) / distance;
  const float heading_error = wrap_angle(std::atan2(dx, dz) - agent.yaw);
  const float turn_limit = quick ? 5.5f : 4.2f;
  const float requested_turn = std::clamp(heading_error * 8.f, -turn_limit, turn_limit);
  agent.yaw_speed = approach(agent.yaw_speed, requested_turn, dt * 18.f);
  float yaw_delta = agent.yaw_speed * dt;
  if (yaw_delta * heading_error >= 0.f && std::fabs(yaw_delta) > std::fabs(heading_error)) {
    yaw_delta = heading_error;
    agent.yaw_speed = 0.f;
  }
  agent.yaw = wrap_angle(agent.yaw + yaw_delta);

  const float acceleration = quick ? 8.f : 4.f;
  const float braking = quick ? 12.f : 7.f;
  const float remaining = distance - stop_radius;
  // Slowing for a sharp corner removes sideways skating without diverting the
  // translation off the original route. Braking starts early enough to settle.
  const float alignment = (std::max)(0.f, std::cos(heading_error - yaw_delta));
  const float arrival_speed = (std::min)(std::sqrt(2.f * braking * remaining) * .9f,
                                        remaining * 5.f);
  const float desired_speed = (std::min)(cruise_speed * alignment, arrival_speed);
  const float old_speed = agent.locomotion_speed;
  agent.locomotion_speed = approach(old_speed, desired_speed,
      dt * (desired_speed > old_speed ? acceleration : braking));
  const float requested_distance = (old_speed + agent.locomotion_speed) * .5f * dt;
  const float step = (std::min)(remaining, requested_distance);
  const Vec3 before = agent.position;
  agent.position.x += dx * step;
  agent.position.z += dz * step;
  // Measure the representable displacement, not an intended step lost to float
  // precision. Idle/blocked movement cannot keep the walk cycle spinning.
  const float traveled = distance_xz(before, agent.position);
  agent.travel_distance += static_cast<double>(traveled);
  agent.anim_phase = advance_phase(agent.anim_phase, traveled * 3.2f);
  if (step >= remaining) agent.locomotion_speed = 0.f;
  const float walk_weight = static_cast<float>(std::clamp(
      static_cast<double>(traveled) / (static_cast<double>(dt) * cruise_speed), 0.0, 1.0));
  agent.move_weight = approach(agent.move_weight, walk_weight, dt * 4.f);
  return yaw_delta;
}

template <typename Agent>
void finish_update(Agent& agent, const Vec3& start, double distance_before,
                   float yaw_change, float dt) {
  agent.velocity = {(agent.position.x - start.x) / dt, 0.f,
                    (agent.position.z - start.z) / dt};
  agent.actual_speed = static_cast<float>((agent.travel_distance - distance_before) / dt);
  agent.turn_rate = yaw_change / dt;
}
}  // namespace locomotion_detail

}  // namespace fury
