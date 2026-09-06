#include "fury/transform.hpp"

namespace fury {

Mat4 Transform::matrix() const {
  return translate(position) * rotate_y(rotation_euler.y) *
         rotate_x(rotation_euler.x) * rotate_z(rotation_euler.z) *
         fury::scale(this->scale);
}

}  // namespace fury
