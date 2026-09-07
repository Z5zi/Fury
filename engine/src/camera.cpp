#include "fury/camera.hpp"

#include <algorithm>
#include <cmath>

namespace fury {
namespace {

float exp_smooth(float rate, float dt) {
  return 1.f - std::exp(-rate * dt);
}

}  // namespace

Vec3 Camera::forward() const {
  const float cp = std::cos(pitch);
  return normalize(Vec3{std::cos(yaw) * cp, std::sin(pitch), std::sin(yaw) * cp});
}

Vec3 Camera::right() const {
  return normalize(cross(forward(), Vec3{0.f, 1.f, 0.f}));
}

Vec3 Camera::up() const { return normalize(cross(right(), forward())); }

Mat4 Camera::view_matrix() const {
  Vec3 eye = position;
  if (third_person && !fly_mode && !vehicle_seated) {
    const Vec3 f = forward();
    Vec3 flat{f.x, 0.f, f.z};
    const float fl = length(flat);
    if (fl > 1e-5f) {
      flat = flat * (1.f / fl);
    } else {
      flat = {std::cos(yaw), 0.f, std::sin(yaw)};
    }
    eye = position - flat * third_person_distance;
    eye.y += third_person_height;
  }
  return look_at(eye, eye + forward(), Vec3{0.f, 1.f, 0.f});
}

Mat4 Camera::projection_matrix(float aspect) const {
  return perspective(radians(fov_y_degrees), aspect, near_plane, far_plane);
}

void Camera::update(const InputState& input, float dt) {
  // Mouse look when captured; gamepad right-stick always contributes to dx/dy.
  const bool has_look =
      input.mouse_captured ||
      (std::fabs(input.mouse_dx) + std::fabs(input.mouse_dy) > 1e-6f);
  if (has_look) {
    yaw_target += input.mouse_dx * mouse_sensitivity;
    const float ysign = invert_y ? 1.f : -1.f;
    pitch_target += ysign * input.mouse_dy * mouse_sensitivity;
    pitch_target = std::clamp(pitch_target, radians(-89.f), radians(89.f));
  }

  // Coyote-ish look smoothing — soft lag without feeling mushy.
  const float lk = exp_smooth(look_smooth_rate, dt);
  yaw += (yaw_target - yaw) * lk;
  pitch += (pitch_target - pitch) * lk;

  const bool grounded = vehicle_seated || !fly_mode;

  float forward_axis = input.move_forward;
  float strafe_axis = input.move_strafe;
  if (input.key_w) forward_axis += 1.f;
  if (input.key_s) forward_axis -= 1.f;
  if (input.key_d) strafe_axis += 1.f;
  if (input.key_a) strafe_axis -= 1.f;
  forward_axis = std::clamp(forward_axis, -1.f, 1.f);
  strafe_axis = std::clamp(strafe_axis, -1.f, 1.f);

  Vec3 wish{0.f, 0.f, 0.f};
  const Vec3 f = forward();
  const Vec3 r = right();
  if (std::fabs(forward_axis) > 1e-4f) {
    wish += (grounded ? normalize(Vec3{f.x, 0.f, f.z}) : f) * forward_axis;
  }
  if (std::fabs(strafe_axis) > 1e-4f) {
    wish += r * strafe_axis;
  }
  if (fly_mode && !vehicle_seated) {
    if (input.key_space) wish += Vec3{0.f, 1.f, 0.f};
    if (input.key_ctrl) wish -= Vec3{0.f, 1.f, 0.f};
  }

  const float wish_len = length(wish);
  if (wish_len > 1.f) {
    wish = wish * (1.f / wish_len);
    coyote_wish = wish;
    coyote_timer = coyote_time;
  } else if (wish_len > 1e-6f) {
    // Preserve analog magnitude (partial stick).
    coyote_wish = wish * (1.f / wish_len);
    coyote_timer = coyote_time;
  } else if (coyote_timer > 0.f && grounded) {
    coyote_timer = (std::max)(0.f, coyote_timer - dt);
    // Soft residual steer — coyote coast
    wish = coyote_wish * (0.25f * (coyote_timer / (std::max)(coyote_time, 1e-3f)));
  }

  // Crouch: Ctrl / gamepad B in walk mode only (fly still uses Ctrl for descend).
  crouching = !fly_mode && !vehicle_seated && input.key_ctrl;
  float max_speed = vehicle_seated ? vehicle_speed : move_speed;
  if (crouching) {
    max_speed *= crouch_speed_mul;
  } else if (input.key_shift && !vehicle_seated) {
    max_speed *= 2.2f;
  }
  if (input.key_shift && vehicle_seated) max_speed *= 1.35f;

  const float accel = vehicle_seated ? accel_drive : accel_walk;
  const float friction = vehicle_seated ? friction_drive : friction_walk;

  if (length(wish) > 1e-5f) {
    velocity += wish * (accel * dt);
    const float spd = length(velocity);
    // Scale max speed by analog wish length so partial stick is slower.
    const float wish_spd = (std::min)(1.f, length(wish));
    const float cap = max_speed * (std::max)(wish_spd, crouching ? 1.f : 0.35f);
    if (spd > cap) {
      velocity = velocity * (cap / spd);
    }
  } else {
    const float spd = length(velocity);
    if (spd > 1e-5f) {
      const float drop = friction * dt;
      const float ns = (std::max)(0.f, spd - drop);
      velocity = (ns > 1e-5f) ? velocity * (ns / spd) : Vec3{0.f, 0.f, 0.f};
    } else {
      velocity = {0.f, 0.f, 0.f};
    }
  }

  position += velocity * dt;

  if (vehicle_seated) {
    position.y = 1.55f;
    velocity.y = 0.f;
  } else if (!fly_mode) {
    position.y = crouching ? crouch_eye_y : stand_eye_y;
    velocity.y = 0.f;
  }
}

}  // namespace fury
