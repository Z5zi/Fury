#include "fury/collision.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace fury {

bool aabb_overlaps_xz(const Aabb& a, const Vec3& point, float radius) {
  const float min_x = a.center.x - a.half_extents.x - radius;
  const float max_x = a.center.x + a.half_extents.x + radius;
  const float min_z = a.center.z - a.half_extents.z - radius;
  const float max_z = a.center.z + a.half_extents.z + radius;
  return point.x >= min_x && point.x <= max_x && point.z >= min_z &&
         point.z <= max_z;
}

Vec3 resolve_player_collision(const Vec3& desired, float radius,
                              const std::vector<Aabb>& solids,
                              float eye_height) {
  Vec3 pos = desired;
  // Keep walk height unless flying (caller may override Y afterward).
  (void)eye_height;

  // Separate X then Z for smoother sliding along walls.
  for (int pass = 0; pass < 2; ++pass) {
    for (const Aabb& box : solids) {
      // Skip if player eyes are clearly above the solid (rooftop walk later).
      const float top = box.center.y + box.half_extents.y;
      const float bottom = box.center.y - box.half_extents.y;
      if (pos.y > top + 0.5f || pos.y + 0.2f < bottom) {
        continue;
      }

      const float min_x = box.center.x - box.half_extents.x - radius;
      const float max_x = box.center.x + box.half_extents.x + radius;
      const float min_z = box.center.z - box.half_extents.z - radius;
      const float max_z = box.center.z + box.half_extents.z + radius;

      if (pos.x <= min_x || pos.x >= max_x || pos.z <= min_z || pos.z >= max_z) {
        continue;
      }

      const float push_left = pos.x - min_x;
      const float push_right = max_x - pos.x;
      const float push_neg_z = pos.z - min_z;
      const float push_pos_z = max_z - pos.z;

      const float min_push =
          std::min({push_left, push_right, push_neg_z, push_pos_z});

      if (pass == 0) {
        // Prefer X resolution first pass
        if (push_left <= push_right && push_left <= min_push + 1e-4f) {
          pos.x = min_x;
        } else if (push_right <= min_push + 1e-4f) {
          pos.x = max_x;
        }
      } else {
        if (push_neg_z <= push_pos_z && push_neg_z <= min_push + 1e-4f) {
          pos.z = min_z;
        } else if (push_pos_z <= min_push + 1e-4f) {
          pos.z = max_z;
        } else if (push_left <= push_right) {
          pos.x = min_x;
        } else {
          pos.x = max_x;
        }
      }
    }
  }

  return pos;
}

namespace {
struct SweepRect { double min_x, max_x, min_z, max_z; };
bool sweep_in_domain(float v) {
  return std::isfinite(v) && std::abs(v) <= player_sweep_domain_limit;
}
bool sweep_in_domain(const Vec3& p) {
  return sweep_in_domain(p.x) && sweep_in_domain(p.y) && sweep_in_domain(p.z);
}
bool sweep_rectangle(const Aabb& box, float radius, SweepRect& r,
                     float& bottom, float& top) {
  if (!sweep_in_domain(box.center) || !sweep_in_domain(box.half_extents) ||
      box.half_extents.x < 0.f || box.half_extents.y < 0.f ||
      box.half_extents.z < 0.f) return false;
  const float min_x = box.center.x - box.half_extents.x - radius;
  const float max_x = box.center.x + box.half_extents.x + radius;
  const float min_z = box.center.z - box.half_extents.z - radius;
  const float max_z = box.center.z + box.half_extents.z + radius;
  bottom = box.center.y - box.half_extents.y;
  top = box.center.y + box.half_extents.y;
  r = {min_x, max_x, min_z, max_z};
  return sweep_in_domain(min_x) && sweep_in_domain(max_x) &&
         sweep_in_domain(min_z) && sweep_in_domain(max_z) &&
         sweep_in_domain(bottom) && sweep_in_domain(top) &&
         min_x < max_x && min_z < max_z;
}
bool sweep_active(float eye_y, float bottom, float top) {
  return !(eye_y > top + 0.5f || eye_y + 0.2f < bottom);
}
bool sweep_inside(const Vec3& p, const SweepRect& r) {
  return p.x > r.min_x && p.x < r.max_x &&
         p.z > r.min_z && p.z < r.max_z;
}
struct SweepHit { bool hit{false}; double time{1.}; bool x{false}, z{false}; };
SweepHit sweep_entry(const Vec3& p, double dx, double dz, const SweepRect& r) {
  double enter = -std::numeric_limits<double>::infinity();
  double leave = std::numeric_limits<double>::infinity();
  double near_time[2] = {enter, enter};
  const double origin[] = {p.x, p.z}, delta[] = {dx, dz};
  const double lo[] = {r.min_x, r.min_z}, hi[] = {r.max_x, r.max_z};
  for (unsigned axis = 0; axis < 2; ++axis) {
    if (delta[axis] == 0.) {
      if (origin[axis] <= lo[axis] || origin[axis] >= hi[axis]) return {};
    } else {
      const double t0 = (lo[axis] - origin[axis]) / delta[axis];
      const double t1 = (hi[axis] - origin[axis]) / delta[axis];
      near_time[axis] = std::min(t0, t1);
      enter = std::max(enter, near_time[axis]);
      leave = std::min(leave, std::max(t0, t1));
    }
  }
  if (std::max(enter, 0.) >= std::min(leave, 1.)) return {};
  return {true, std::max(enter, 0.), near_time[0] == enter,
          near_time[1] == enter};
}
SweepHit first_sweep_hit(const Vec3& p, double dx, double dz, float eye_y,
                        float radius, const std::vector<Aabb>& solids) {
  SweepHit first;
  for (const Aabb& box : solids) {
    SweepRect r{}; float bottom{}, top{};
    sweep_rectangle(box, radius, r, bottom, top); // Validated by API entry.
    if (!sweep_active(eye_y, bottom, top)) continue;
    const SweepHit hit = sweep_entry(p, dx, dz, r);
    if (!hit.hit) continue;
    if (!first.hit || hit.time < first.time) first = hit;
    else if (hit.time == first.time) {
      first.x |= hit.x;
      first.z |= hit.z;
    }
  }
  return first;
}
bool sweep_position_clear(const Vec3& p, float eye_y, float radius,
                          const std::vector<Aabb>& solids) {
  for (const Aabb& box : solids) {
    SweepRect r{}; float bottom{}, top{};
    sweep_rectangle(box, radius, r, bottom, top); // Validated by API entry.
    if (sweep_active(eye_y, bottom, top) && sweep_inside(p, r)) return false;
  }
  return true;
}
float sweep_round_toward_start(double value, float start) {
  float rounded = static_cast<float>(value);
  if ((value > start && rounded > value) ||
      (value < start && rounded < value))
    rounded = std::nextafter(rounded, start);
  return rounded;
}
}  // namespace

PlayerSweepResult sweep_player_collision(const Vec3& start, const Vec3& desired,
                                         float radius,
                                         const std::vector<Aabb>& solids) {
  PlayerSweepResult result;
  if (!sweep_in_domain(start) || !sweep_in_domain(desired) ||
      !sweep_in_domain(radius) || radius <= 0.f) return result;
  double dx = double(desired.x) - start.x, dz = double(desired.z) - start.z;
  if (std::abs(dx) > player_sweep_domain_limit ||
      std::abs(dz) > player_sweep_domain_limit) return result;
  // Validate all geometry before checking overlap or attempting any movement.
  for (const Aabb& box : solids) {
    SweepRect r{}; float bottom{}, top{};
    if (!sweep_rectangle(box, radius, r, bottom, top)) return result;
  }
  if (!sweep_position_clear(start, desired.y, radius, solids)) {
    result.outcome = PlayerSweepOutcome::InvalidStart;
    return result;
  }
  Vec3 position{start.x, desired.y, start.z};
  result.outcome = PlayerSweepOutcome::Clear;
  for (unsigned contact = 0; contact < player_sweep_max_contacts; ++contact) {
    if (dx == 0. && dz == 0.) {
      if (!result.blocked_x) position.x = desired.x;
      if (!result.blocked_z) position.z = desired.z;
      break;
    }
    const SweepHit hit = first_sweep_hit(position, dx, dz, desired.y, radius, solids);
    double fraction = 1.;
    if (hit.hit) {
      const double longest_axis = std::max(std::abs(dx), std::abs(dz));
      fraction = std::max(0., hit.time - player_sweep_contact_skin / longest_axis);
    }
    const Vec3 next = hit.hit ?
      Vec3{sweep_round_toward_start(double(position.x) + dx * fraction, position.x),
           desired.y,
           sweep_round_toward_start(double(position.z) + dz * fraction, position.z)} :
      Vec3{result.blocked_x ? position.x : desired.x, desired.y,
           result.blocked_z ? position.z : desired.z};
    // Validate the rounded endpoint and actual segment, not only ideal TOI.
    if (!sweep_position_clear(next, desired.y, radius, solids) ||
        first_sweep_hit(position, double(next.x) - position.x,
                       double(next.z) - position.z, desired.y, radius, solids).hit) {
      result.outcome = PlayerSweepOutcome::NumericalFailure;
      return result;
    }
    position = next;
    if (!hit.hit) { dx = dz = 0.; break; }
    ++result.contacts;
    result.outcome = PlayerSweepOutcome::Blocked;
    result.blocked_x |= hit.x;
    result.blocked_z |= hit.z;
    dx = result.blocked_x ? 0. : double(desired.x) - position.x;
    dz = result.blocked_z ? 0. : double(desired.z) - position.z;
  }
  if (dx != 0. || dz != 0.) result.outcome = PlayerSweepOutcome::ContactLimit;
  result.accepted_position = position;
  return result;
}

namespace {
GroundedMoveResult finish_grounded_player_movement(
    PlayerSweepResult sweep, const Vec3& desired, float radius,
    const std::vector<Aabb>& solids) {
  GroundedMoveResult result;
  result.sweep = std::move(sweep);
  // This helper is private and receives only this call's validated sweep result.
  // Missing certified coordinates alone are never permission for recovery.
  if (result.sweep.outcome == PlayerSweepOutcome::InvalidStart &&
      !result.sweep.accepted_position) {
    result.outcome = GroundedMoveOutcome::LegacyRecoveryUncertified;
    result.legacy_position = resolve_player_collision(desired, radius, solids, 1.7f);
  } else if ((result.sweep.outcome == PlayerSweepOutcome::Clear ||
              result.sweep.outcome == PlayerSweepOutcome::Blocked ||
              result.sweep.outcome == PlayerSweepOutcome::ContactLimit) &&
             result.sweep.accepted_position) {
    result.outcome = GroundedMoveOutcome::CertifiedSweep;
  }
  return result;
}
}  // namespace

GroundedMoveResult resolve_grounded_player_movement(
    const Vec3& start, const Vec3& desired, float radius,
    const std::vector<Aabb>& solids) {
  return finish_grounded_player_movement(
      sweep_player_collision(start, desired, radius, solids), desired, radius, solids);
}

}  // namespace fury
