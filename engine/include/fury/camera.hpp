#pragma once

#include "fury/input.hpp"
#include "fury/math.hpp"

namespace fury {

class Camera {
 public:
  Vec3 position{0.f, 2.f, 8.f};
  float yaw{-1.5707963f};
  float pitch{0.f};
  float move_speed{8.f};
  float vehicle_speed{22.f};
  float mouse_sensitivity{0.0022f};
  float fov_y_degrees{60.f};
  float near_plane{0.1f};
  float far_plane{500.f};
  bool fly_mode{true};
  /// First/third person body view (V). Body mesh shown only when third && !fly.
  bool third_person{false};
  float third_person_distance{4.2f};
  float third_person_height{1.15f};
  /// When seated in a vehicle stub: ground-plane WASD at vehicle_speed, no fly.
  bool vehicle_seated{false};
  /// Hold Ctrl in walk mode: crouch (slower, quieter heat; lower eye).
  bool crouching{false};
  float crouch_speed_mul{0.42f};
  float stand_eye_y{1.7f};
  float crouch_eye_y{1.05f};

  /// Horizontal / wish velocity (walk + drive accel/decel polish).
  Vec3 velocity{0.f, 0.f, 0.f};
  float accel_walk{48.f};
  float accel_drive{34.f};
  float friction_walk{32.f};
  float friction_drive{18.f};
  /// Soft look lag (coyote-ish smoothing) — higher = snappier.
  float look_smooth_rate{16.f};
  float yaw_target{-1.5707963f};
  float pitch_target{0.f};
  /// Brief residual wish after release (coyote coast).
  float coyote_time{0.12f};
  float coyote_timer{0.f};
  Vec3 coyote_wish{0.f, 0.f, 0.f};

  Vec3 forward() const;
  Vec3 right() const;
  Vec3 up() const;

  Mat4 view_matrix() const;
  Mat4 projection_matrix(float aspect) const;

  void update(const InputState& input, float dt);

  /// Sync look targets to current yaw/pitch (call after teleport / seat).
  void snap_look() {
    yaw_target = yaw;
    pitch_target = pitch;
  }
};

}  // namespace fury
