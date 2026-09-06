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

  const bool grounded = vehicle_seated || !fly_mode;

  Vec3 wish{0.f, 0.f, 0.f};
  const Vec3 f = forward();
  const Vec3 r = right();
  if (input.key_w) wish += grounded ? normalize(Vec3{f.x, 0.f, f.z}) : f;
  if (input.key_s) wish -= grounded ? normalize(Vec3{f.x, 0.f, f.z}) : f;
  if (input.key_d) wish += r;
  if (input.key_a) wish -= r;
  if (fly_mode && !vehicle_seated) {
    if (input.key_space) wish += Vec3{0.f, 1.f, 0.f};
    if (input.key_ctrl) wish -= Vec3{0.f, 1.f, 0.f};
  }

  if (length(wish) > 1e-6f) {
    float speed = vehicle_seated ? vehicle_speed : move_speed;
    if (input.key_shift && !vehicle_seated) speed *= 2.2f;
    if (input.key_shift && vehicle_seated) speed *= 1.35f;
    position += normalize(wish) * (speed * dt);
  }

  if (vehicle_seated) {
    position.y = 1.55f;
  } else if (!fly_mode) {
    position.y = 1.7f;
  }
}

}  // namespace fury
