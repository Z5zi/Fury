#pragma once

#include "fury/math.hpp"

#include <vector>
#include <optional>

namespace fury {

struct Aabb {
  Vec3 center{0.f, 0.f, 0.f};
  Vec3 half_extents{0.5f, 0.5f, 0.5f};

  Vec3 min() const { return center - half_extents; }
  Vec3 max() const { return center + half_extents; }

  static Aabb from_center_size(const Vec3& c, const Vec3& size) {
    return Aabb{c, size * 0.5f};
  }
};

/// Axis-aligned XZ collision for a point with radius (ignores Y for walk).
/// Returns corrected position. Solid AABBs are treated as infinite-height
/// pillars in XZ (Y half-extents still used for vertical gating if needed).
Vec3 resolve_player_collision(const Vec3& desired, float radius,
                              const std::vector<Aabb>& solids,
                              float eye_height = 1.7f);

bool aabb_overlaps_xz(const Aabb& a, const Vec3& point, float radius);

// Bounded flat-XZ movement only. Existing endpoint/placement owners above retain
// their API. Vertical activation is deliberately the legacy desired-eye gate:
// skip only when desired.y > top + .5 or desired.y + .2 < bottom.
// This does not implement a body interval, support height, stance or recovery.
inline constexpr float player_sweep_domain_limit = 1024.f;
inline constexpr unsigned player_sweep_max_contacts = 4;
inline constexpr double player_sweep_contact_skin = .001;
enum class PlayerSweepOutcome {
  Clear, Blocked, ContactLimit, InvalidStart, InvalidInput, NumericalFailure
};
struct PlayerSweepResult {
  PlayerSweepOutcome outcome{PlayerSweepOutcome::InvalidInput};
  // Present only for a certified endpoint; invalid starts are never repaired.
  std::optional<Vec3> accepted_position;
  bool blocked_x{false};
  bool blocked_z{false};
  unsigned contacts{0};
};
// Coordinates, derived box bounds and requested XZ components must be finite
// and within +/-1024m; radius must be finite, positive and in that domain.
// Open XZ interior blocks. Face contact/tangent/away remain legal. Every box is
// validated, including vertically inactive boxes. Hidden solids remain solid.
// Result Y is desired.y. O(4*N), constant workspace, no scene mutation.
PlayerSweepResult sweep_player_collision(const Vec3& start, const Vec3& desired,
                                         float radius,
                                         const std::vector<Aabb>& solids);

// Grounded, non-seated movement dispatch. Certified sweep and inherited legacy
// endpoint handling are deliberately distinct: a clear legacy endpoint does
// not certify the path from an embedded start.
enum class GroundedMoveOutcome { CertifiedSweep, LegacyRecoveryUncertified, Held };
struct GroundedMoveResult {
  GroundedMoveOutcome outcome{GroundedMoveOutcome::Held};
  PlayerSweepResult sweep;
  // Present only for validated InvalidStart legacy handling. Never populate the
  // sweep's accepted_position with these uncertified coordinates.
  std::optional<Vec3> legacy_position;
};
// Same snapshot/desired eye/radius for validation and legacy dispatch. Invalid
// input/geometry, numerical failure, and clear-start contact outcomes never
// trigger legacy handling. Certified partial ContactLimit semantics are kept.
GroundedMoveResult resolve_grounded_player_movement(
    const Vec3& start, const Vec3& desired, float radius,
    const std::vector<Aabb>& solids);

}  // namespace fury
