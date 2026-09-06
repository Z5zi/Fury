#pragma once

#include "fury/math.hpp"

namespace fury {

struct Transform {
  Vec3 position{0.f, 0.f, 0.f};
  Vec3 rotation_euler{0.f, 0.f, 0.f};  // radians: pitch(x), yaw(y), roll(z)
  Vec3 scale{1.f, 1.f, 1.f};

  Mat4 matrix() const;
};

}  // namespace fury
