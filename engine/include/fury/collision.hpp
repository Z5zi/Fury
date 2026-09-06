#pragma once

#include "fury/math.hpp"

#include <vector>

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

}  // namespace fury
