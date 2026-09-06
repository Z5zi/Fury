#include "fury/math.hpp"

namespace fury {
namespace {

float dot_cpp(const float a[3], const float b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

}  // namespace

float dot(const float a[3], const float b[3]) {
#if FURY_HAS_ASM
  return fury_dot3_asm(a, b);
#else
  return dot_cpp(a, b);
#endif
}

float dot(const Vec3& a, const Vec3& b) {
  const float aa[3] = {a.x, a.y, a.z};
  const float bb[3] = {b.x, b.y, b.z};
  return dot(aa, bb);
}

bool math_uses_asm() {
#if FURY_HAS_ASM
  return true;
#else
  return false;
#endif
}

// Ensure the C++ implementation remains linked for tools/tests even when ASM is on.
float fury_dot3_cpp_fallback(const float a[3], const float b[3]) {
  return dot_cpp(a, b);
}

}  // namespace fury
