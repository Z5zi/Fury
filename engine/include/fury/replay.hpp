#pragma once

#include "fury/camera.hpp"
#include "fury/math.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fury {

/// One sample of player camera transform for the replay ring buffer.
struct ReplaySample {
  Vec3 position{0.f, 1.7f, 0.f};
  float yaw{0.f};
  float pitch{0.f};
};

/// Ring buffer of the last N seconds of player transform (Vaultline 2.7.0).
/// F10 enters scrub: A/D rewind/advance camera along the path; ghost trail optional.
struct ReplayBuffer {
  static constexpr float kDurationSeconds = 8.f;
  static constexpr float kSampleHz = 30.f;
  static constexpr std::size_t kCapacity =
      static_cast<std::size_t>(kDurationSeconds * kSampleHz) + 1;

  std::vector<ReplaySample> samples;
  std::size_t head{0};   // next write index
  std::size_t count{0};  // filled slots
  float accum{0.f};

  bool scrubbing{false};
  float scrub_u{1.f};  // 0 = oldest, 1 = newest
  Vec3 saved_position{0.f, 1.7f, 12.f};
  float saved_yaw{-1.5707963f};
  float saved_pitch{-0.08f};
  bool saved_fly{false};
  bool saved_third{false};
  bool saved_vehicle{false};

  ReplayBuffer() { samples.resize(kCapacity); }

  void clear() {
    head = 0;
    count = 0;
    accum = 0.f;
    scrub_u = 1.f;
  }

  void push(const Camera& cam, float dt) {
    if (scrubbing) {
      return;
    }
    accum += dt;
    const float interval = 1.f / kSampleHz;
    while (accum >= interval) {
      accum -= interval;
      ReplaySample s;
      s.position = cam.position;
      s.yaw = cam.yaw;
      s.pitch = cam.pitch;
      samples[head] = s;
      head = (head + 1) % kCapacity;
      if (count < kCapacity) {
        ++count;
      }
    }
  }

  /// Chronological index 0 = oldest, count-1 = newest.
  ReplaySample at_chrono(std::size_t chrono_i) const {
    if (count == 0) {
      return {};
    }
    const std::size_t i = chrono_i % count;
    const std::size_t oldest = (head + kCapacity - count) % kCapacity;
    return samples[(oldest + i) % kCapacity];
  }

  ReplaySample sample_at_u(float u) const {
    if (count == 0) {
      return {};
    }
    if (count == 1) {
      return at_chrono(0);
    }
    u = std::clamp(u, 0.f, 1.f);
    const float f = u * static_cast<float>(count - 1);
    const std::size_t i0 = static_cast<std::size_t>(f);
    const std::size_t i1 = (std::min)(i0 + 1, count - 1);
    const float t = f - static_cast<float>(i0);
    const ReplaySample a = at_chrono(i0);
    const ReplaySample b = at_chrono(i1);
    ReplaySample out;
    out.position = {
        a.position.x + (b.position.x - a.position.x) * t,
        a.position.y + (b.position.y - a.position.y) * t,
        a.position.z + (b.position.z - a.position.z) * t,
    };
    out.yaw = a.yaw + (b.yaw - a.yaw) * t;
    out.pitch = a.pitch + (b.pitch - a.pitch) * t;
    return out;
  }

  void apply_to_camera(Camera& cam) const {
    const ReplaySample s = sample_at_u(scrub_u);
    cam.position = s.position;
    cam.yaw = s.yaw;
    cam.pitch = s.pitch;
    cam.velocity = {};
    cam.fly_mode = true;
    cam.vehicle_seated = false;
    cam.third_person = false;
    cam.snap_look();
  }

  void begin_scrub(Camera& cam) {
    if (scrubbing || count == 0) {
      return;
    }
    saved_position = cam.position;
    saved_yaw = cam.yaw;
    saved_pitch = cam.pitch;
    saved_fly = cam.fly_mode;
    saved_third = cam.third_person;
    saved_vehicle = cam.vehicle_seated;
    scrub_u = 1.f;
    scrubbing = true;
    apply_to_camera(cam);
  }

  void end_scrub(Camera& cam) {
    if (!scrubbing) {
      return;
    }
    cam.position = saved_position;
    cam.yaw = saved_yaw;
    cam.pitch = saved_pitch;
    cam.fly_mode = saved_fly;
    cam.third_person = saved_third;
    cam.vehicle_seated = saved_vehicle;
    cam.velocity = {};
    cam.snap_look();
    scrubbing = false;
  }

  void toggle_scrub(Camera& cam) {
    if (scrubbing) {
      end_scrub(cam);
    } else {
      begin_scrub(cam);
    }
  }

  /// Scrub with signed rate (A = negative / older, D = positive / newer).
  void scrub(float du) {
    if (!scrubbing || count == 0) {
      return;
    }
    scrub_u = std::clamp(scrub_u + du, 0.f, 1.f);
  }

  /// Ghost trail sample count for drawing (every stride-th chrono sample).
  std::size_t ghost_stride() const {
    if (count <= 24) {
      return 1;
    }
    return (std::max)(std::size_t{1}, count / 24);
  }
};

}  // namespace fury
