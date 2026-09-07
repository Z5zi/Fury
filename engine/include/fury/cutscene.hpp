#pragma once

#include "fury/camera.hpp"
#include "fury/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace fury {

/// Simple scripted camera fly-through (keyframe lerp). Used for Vaultline
/// intro over Harbor Metro — original, no third-party IP.
struct CutsceneKeyframe {
  Vec3 position{0.f, 2.f, 8.f};
  float yaw{-1.5707963f};
  float pitch{-0.12f};
};

struct CutsceneStub {
  bool active{false};
  bool finished{false};  // true once played/skipped this launch
  float t{0.f};          // seconds elapsed
  float duration{4.f};

  static constexpr std::size_t kFrameCount = 4;
  CutsceneKeyframe frames[kFrameCount] = {
      // Rise over waterfront / loft approach
      {{18.f, 42.f, 62.f}, -2.6f, -0.55f},
      // Sweep Ridge Pier / east bridge
      {{48.f, 28.f, 28.f}, -2.1f, -0.35f},
      // Fly over Meridian Mutual plaza
      {{-8.f, 22.f, 6.f}, -0.9f, -0.42f},
      // Settle near spawn
      {{0.f, 1.7f, 12.f}, -1.5707963f, -0.08f},
  };

  void begin() {
    active = true;
    finished = false;
    t = 0.f;
  }

  void skip() {
    active = false;
    finished = true;
    t = duration;
  }

  /// Advance cutscene. Returns true while still playing.
  bool update(float dt, Camera& cam) {
    if (!active) {
      return false;
    }
    t += dt;
    const float u = (duration > 1e-4f) ? std::clamp(t / duration, 0.f, 1.f) : 1.f;
    apply(u, cam);
    if (u >= 1.f) {
      active = false;
      finished = true;
      return false;
    }
    return true;
  }

  void apply(float u, Camera& cam) const {
    const float scaled = u * static_cast<float>(kFrameCount - 1);
    const int i0 = static_cast<int>(scaled);
    const int i1 = (std::min)(i0 + 1, static_cast<int>(kFrameCount) - 1);
    const float f = scaled - static_cast<float>(i0);
    const CutsceneKeyframe& a = frames[static_cast<std::size_t>(i0)];
    const CutsceneKeyframe& b = frames[static_cast<std::size_t>(i1)];
    // Smoothstep for gentler fly
    const float s = f * f * (3.f - 2.f * f);
    cam.position = {
        a.position.x + (b.position.x - a.position.x) * s,
        a.position.y + (b.position.y - a.position.y) * s,
        a.position.z + (b.position.z - a.position.z) * s,
    };
    cam.yaw = a.yaw + (b.yaw - a.yaw) * s;
    cam.pitch = a.pitch + (b.pitch - a.pitch) * s;
    cam.velocity = {};
    cam.fly_mode = true;  // avoid ground collision mid-flight
    cam.snap_look();
  }
};

}  // namespace fury
