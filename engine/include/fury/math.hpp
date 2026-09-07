#pragma once

#include "fury/platform.hpp"

#include <cmath>
#include <cstring>

namespace fury {

struct Vec2 {
  float x{0.f};
  float y{0.f};
};

struct Vec3 {
  float x{0.f};
  float y{0.f};
  float z{0.f};

  Vec3() = default;
  Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

  Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
  Vec3 operator-() const { return {-x, -y, -z}; }
  Vec3& operator+=(const Vec3& o) {
    x += o.x;
    y += o.y;
    z += o.z;
    return *this;
  }
  Vec3& operator-=(const Vec3& o) {
    x -= o.x;
    y -= o.y;
    z -= o.z;
    return *this;
  }
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

struct Vec4 {
  float x{0.f};
  float y{0.f};
  float z{0.f};
  float w{0.f};

  Vec4() = default;
  Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
  explicit Vec4(const Vec3& v, float w_ = 1.f) : x(v.x), y(v.y), z(v.z), w(w_) {}
};

/// Column-major 4x4 matrix (OpenGL convention).
struct Mat4 {
  float m[16]{};

  static Mat4 identity();
  float& at(int col, int row) { return m[col * 4 + row]; }
  float at(int col, int row) const { return m[col * 4 + row]; }
};

float length(const Vec3& v);
Vec3 normalize(const Vec3& v);
Vec3 cross(const Vec3& a, const Vec3& b);

/// Dot product of two 3D float vectors.
/// Uses an x86_64 NASM kernel when FURY_HAS_ASM is enabled; otherwise C++.
float dot(const Vec3& a, const Vec3& b);
float dot(const float a[3], const float b[3]);

bool math_uses_asm();

Mat4 operator*(const Mat4& a, const Mat4& b);
/// Returns false for singular/non-finite matrices, leaving out unchanged.
bool inverse(const Mat4& m, Mat4& out);
Mat4 transpose(const Mat4& m);
Vec4 mul(const Mat4& m, const Vec4& v);
Vec3 transform_point(const Mat4& m, const Vec3& p);
Vec3 transform_direction(const Mat4& m, const Vec3& d);

Mat4 translate(const Vec3& t);
Mat4 scale(const Vec3& s);
Mat4 rotate_x(float radians);
Mat4 rotate_y(float radians);
Mat4 rotate_z(float radians);
Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up);
Mat4 perspective(float fov_y_radians, float aspect, float z_near, float z_far);
Mat4 orthographic(float left, float right, float bottom, float top,
                  float z_near, float z_far);

inline float radians(float degrees) { return degrees * 0.017453292519943295f; }
inline float degrees(float radians) { return radians * 57.29577951308232f; }

}  // namespace fury

#if FURY_HAS_ASM
extern "C" float fury_dot3_asm(const float* a, const float* b);
#endif
