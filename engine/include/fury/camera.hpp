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
  /// When seated in a vehicle stub: ground-plane WASD at vehicle_speed, no fly.
  bool vehicle_seated{false};

  Vec3 forward() const;
  Vec3 right() const;
  Vec3 up() const;

  Mat4 view_matrix() const;
  Mat4 projection_matrix(float aspect) const;

  void update(const InputState& input, float dt);
};

}  // namespace fury
