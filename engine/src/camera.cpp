#include "fury/camera.hpp"

#include <algorithm>
#include <cmath>

namespace fury {

Vec3 Camera::forward() const {
  const float cp = std::cos(pitch);
  return normalize(Vec3{std::cos(yaw) * cp, std::sin(pitch), std::sin(yaw) * cp});
}

Vec3 Camera::right() const {
  return normalize(cross(forward(), Vec3{0.f, 1.f, 0.f}));
}

Vec3 Camera::up() const { return normalize(cross(right(), forward())); }

Mat4 Camera::view_matrix() const {
  return look_at(position, position + forward(), Vec3{0.f, 1.f, 0.f});
}

Mat4 Camera::projection_matrix(float aspect) const {
  return perspective(radians(fov_y_degrees), aspect, near_plane, far_plane);
}

void Camera::update(const InputState& input, float dt) {
  if (input.mouse_captured) {
    yaw += input.mouse_dx * mouse_sensitivity;
    pitch -= input.mouse_dy * mouse_sensitivity;
    pitch = std::clamp(pitch, radians(-89.f), radians(89.f));
  }

  if (input.key_f) {
    // edge handled in input as held after toggle in app; here treat as held toggle once
  }

  Vec3 wish{0.f, 0.f, 0.f};
  const Vec3 f = forward();
  const Vec3 r = right();
  if (input.key_w) wish += fly_mode ? f : normalize(Vec3{f.x, 0.f, f.z});
  if (input.key_s) wish -= fly_mode ? f : normalize(Vec3{f.x, 0.f, f.z});
  if (input.key_d) wish += r;
  if (input.key_a) wish -= r;
  if (fly_mode) {
    if (input.key_space) wish += Vec3{0.f, 1.f, 0.f};
    if (input.key_ctrl) wish -= Vec3{0.f, 1.f, 0.f};
  }

  if (length(wish) > 1e-6f) {
    float speed = move_speed;
    if (input.key_shift) speed *= 2.2f;
    position += normalize(wish) * (speed * dt);
  }

  if (!fly_mode) {
    // Keep a standing eye height for walk mode
    position.y = 1.7f;
  }
}

}  // namespace fury
