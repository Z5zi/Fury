#include "fury/collision.hpp"

#include <algorithm>
#include <cmath>

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

}  // namespace fury
