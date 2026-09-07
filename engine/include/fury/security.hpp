#pragma once

#include "fury/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace fury {

/// Visibility / detection meter (Vaultline 3.5.0). Rises near guards and
/// active security camera cones; decays when clear. Separate from heat.
struct VisibilityMeter {
  float value{0.f};  // 0..1
  float rise_rate{0.55f};
  float decay_rate{0.38f};

  void reset() { value = 0.f; }
  float normalized() const { return value; }
};

struct SecurityCamera {
  Vec3 position{0.f, 3.f, 0.f};
  float yaw{0.f};               // facing on XZ (same convention as Camera)
  float cone_half_rad{0.55f};   // ~31 deg half-angle
  float range{13.f};
  int site_id{0};               // 0=bank, 1=jewelry, 2=depot
  std::string entity_name;
  std::string lens_name;
};

struct BreakerBox {
  Vec3 position{0.f, 1.1f, 0.f};
  float interact_radius{2.9f};
  int site_id{0};
  std::string entity_name;
  bool tripped{false};
};

/// Site cameras + breaker boxes. Cameras raise heat when the player is in the
/// view cone while standing (not crouching); breakers disable a whole site.
class SecurityNet {
 public:
  float camera_heat_rate{0.22f};
  float guard_vis_radius{9.f};

  void clear() {
    cameras_.clear();
    breakers_.clear();
    for (int i = 0; i < 3; ++i) site_disabled_[i] = false;
  }

  void add_camera(SecurityCamera cam) { cameras_.push_back(std::move(cam)); }
  void add_breaker(BreakerBox box) { breakers_.push_back(std::move(box)); }

  const std::vector<SecurityCamera>& cameras() const { return cameras_; }
  const std::vector<BreakerBox>& breakers() const { return breakers_; }
  bool site_disabled(int id) const {
    return id >= 0 && id < 3 && site_disabled_[id];
  }

  static bool point_in_cone(const SecurityCamera& cam, const Vec3& player) {
    const float dx = player.x - cam.position.x;
    const float dz = player.z - cam.position.z;
    const float dist = std::sqrt(dx * dx + dz * dz);
    if (dist > cam.range || dist < 0.05f) {
      return false;
    }
    const float fx = std::cos(cam.yaw);
    const float fz = std::sin(cam.yaw);
    const float ndx = dx / dist;
    const float ndz = dz / dist;
    const float d = fx * ndx + fz * ndz;
    return d >= std::cos(cam.cone_half_rad);
  }

  int nearest_breaker_index(const Vec3& player) const {
    int best = -1;
    float best_d = 1e9f;
    for (int i = 0; i < static_cast<int>(breakers_.size()); ++i) {
      const auto& b = breakers_[static_cast<std::size_t>(i)];
      if (b.tripped || site_disabled_[std::clamp(b.site_id, 0, 2)]) {
        continue;
      }
      const float dx = player.x - b.position.x;
      const float dz = player.z - b.position.z;
      const float d = std::sqrt(dx * dx + dz * dz);
      if (d <= b.interact_radius && d < best_d) {
        best_d = d;
        best = i;
      }
    }
    return best;
  }

  bool near_live_breaker(const Vec3& player) const {
    return nearest_breaker_index(player) >= 0;
  }

  /// Trip breaker under player if in range. Returns site_id or -1.
  int try_trip_breaker(const Vec3& player) {
    const int i = nearest_breaker_index(player);
    if (i < 0) {
      return -1;
    }
    auto& b = breakers_[static_cast<std::size_t>(i)];
    b.tripped = true;
    const int sid = std::clamp(b.site_id, 0, 2);
    site_disabled_[sid] = true;
    return sid;
  }

  /// Update visibility + return heat added this frame from cameras.
  /// `near_guard` / `guard_dist` feed the visibility meter.
  float update(float dt, const Vec3& player, bool crouching, bool hidden,
               bool near_guard, float /*guard_dist*/, VisibilityMeter& vis) {
    bool spotted = false;
    bool in_any_cone = false;
    float heat_add = 0.f;

    if (!hidden) {
      for (const auto& cam : cameras_) {
        const int sid = std::clamp(cam.site_id, 0, 2);
        if (site_disabled_[sid]) {
          continue;
        }
        if (!point_in_cone(cam, player)) {
          continue;
        }
        in_any_cone = true;
        spotted = true;
        // Heat only when standing (not crouching) in the cone.
        if (!crouching) {
          heat_add += camera_heat_rate * dt;
        }
      }
      if (near_guard) {
        spotted = true;
      }
    }

    if (hidden) {
      vis.value -= vis.decay_rate * 1.5f * dt;
    } else if (spotted) {
      float mul = 1.f;
      if (crouching) {
        mul = 0.42f;  // crouch lowers visibility rise
      }
      if (in_any_cone && !crouching) {
        mul *= 1.25f;
      }
      vis.value += vis.rise_rate * mul * dt;
    } else {
      vis.value -= vis.decay_rate * dt;
    }
    vis.value = std::clamp(vis.value, 0.f, 1.f);
    return heat_add;
  }

  static const char* site_name(int id) {
    switch (id) {
      case 0:
        return "Meridian Mutual";
      case 1:
        return "Crown & Cutler";
      case 2:
        return "Harbor Depot";
      default:
        return "site";
    }
  }

 private:
  std::vector<SecurityCamera> cameras_;
  std::vector<BreakerBox> breakers_;
  bool site_disabled_[3]{false, false, false};
};

}  // namespace fury
