#pragma once

#include "fury/input.hpp"
#include "fury/math.hpp"

namespace fury {

class Camera {
 public:
  Vec3 position{0.f, 2.f, 8.f};
  float yaw{-1.5707963f};  // look down -Z by default? actually -90 deg looks -Z from +X... 
  float pitch{0.f};
  float move_speed{8.f};
  float mouse_sensitivity{0.0022f};
  float fov_y_degrees{60.f};
  float near_plane{0.1f};
  float far_plane{500.f};
  bool fly_mode{true};

  Vec3 forward() const;
  Vec3 right() const;
  Vec3 up() const;

  Mat4 view_matrix() const;
  Mat4 projection_matrix(float aspect) const;

  void update(const InputState& input, float dt);
};

}  // namespace fury
