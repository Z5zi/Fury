#pragma once

#include "fury/platform.hpp"

namespace fury {

struct Vec3 {
  float x{0.f};
  float y{0.f};
  float z{0.f};
};

/// Dot product of two 3D float vectors.
/// Uses an x86_64 NASM kernel when FURY_HAS_ASM is enabled; otherwise C++.
float dot(const Vec3& a, const Vec3& b);
float dot(const float a[3], const float b[3]);

/// True when the assembly kernel is linked and will be used.
bool math_uses_asm();

}  // namespace fury

#if FURY_HAS_ASM
extern "C" float fury_dot3_asm(const float* a, const float* b);
#endif
