#include "fury/math.hpp"

#include <algorithm>

namespace fury {

bool inverse(const Mat4& m, Mat4& out) {
  double a[4][8]{};
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      if (!std::isfinite(m.at(c, r))) return false;
      a[r][c] = m.at(c, r);
    }
    a[r][r + 4] = 1;
  }
  for (int c = 0; c < 4; ++c) {
    int pivot = c;
    for (int r = c + 1; r < 4; ++r)
      if (std::fabs(a[r][c]) > std::fabs(a[pivot][c])) pivot = r;
    if (std::fabs(a[pivot][c]) < 1e-12) return false;
    for (int k = 0; k < 8; ++k) {
      const double v = a[c][k]; a[c][k] = a[pivot][k]; a[pivot][k] = v;
    }
    const double d = a[c][c];
    for (int k = 0; k < 8; ++k) a[c][k] /= d;
    for (int r = 0; r < 4; ++r) if (r != c) {
      const double f = a[r][c];
      for (int k = 0; k < 8; ++k) a[r][k] -= f * a[c][k];
    }
  }
  Mat4 result;
  for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) {
    result.at(c, r) = static_cast<float>(a[r][c + 4]);
    if (!std::isfinite(result.at(c, r))) return false;
  }
  out = result;
  return true;
}

Mat4 transpose(const Mat4& m) {
  Mat4 result;
  for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
    result.at(c, r) = m.at(r, c);
  return result;
}
namespace {

[[maybe_unused]] float dot_cpp(const float a[3], const float b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

}  // namespace

float length(const Vec3& v) { return std::sqrt(dot(v, v)); }

Vec3 normalize(const Vec3& v) {
  const float len = length(v);
  if (len < 1e-8f) {
    return {0.f, 0.f, 0.f};
  }
  return v * (1.f / len);
}

Vec3 cross(const Vec3& a, const Vec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

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

Mat4 Mat4::identity() {
  Mat4 r{};
  r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f;
  return r;
}

Mat4 operator*(const Mat4& a, const Mat4& b) {
  Mat4 out{};
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      float sum = 0.f;
      for (int k = 0; k < 4; ++k) {
        sum += a.at(k, row) * b.at(col, k);
      }
      out.at(col, row) = sum;
    }
  }
  return out;
}

Vec4 mul(const Mat4& m, const Vec4& v) {
  return {m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12] * v.w,
          m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13] * v.w,
          m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14] * v.w,
          m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15] * v.w};
}

Vec3 transform_point(const Mat4& m, const Vec3& p) {
  const Vec4 r = mul(m, Vec4{p, 1.f});
  if (std::fabs(r.w) > 1e-8f) {
    return {r.x / r.w, r.y / r.w, r.z / r.w};
  }
  return {r.x, r.y, r.z};
}

Vec3 transform_direction(const Mat4& m, const Vec3& d) {
  const Vec4 r = mul(m, Vec4{d, 0.f});
  return {r.x, r.y, r.z};
}

Mat4 translate(const Vec3& t) {
  Mat4 r = Mat4::identity();
  r.m[12] = t.x;
  r.m[13] = t.y;
  r.m[14] = t.z;
  return r;
}

Mat4 scale(const Vec3& s) {
  Mat4 r{};
  r.m[0] = s.x;
  r.m[5] = s.y;
  r.m[10] = s.z;
  r.m[15] = 1.f;
  return r;
}

Mat4 rotate_x(float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  Mat4 r = Mat4::identity();
  r.m[5] = c;
  r.m[6] = s;
  r.m[9] = -s;
  r.m[10] = c;
  return r;
}

Mat4 rotate_y(float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  Mat4 r = Mat4::identity();
  r.m[0] = c;
  r.m[2] = -s;
  r.m[8] = s;
  r.m[10] = c;
  return r;
}

Mat4 rotate_z(float radians) {
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  Mat4 r = Mat4::identity();
  r.m[0] = c;
  r.m[1] = s;
  r.m[4] = -s;
  r.m[5] = c;
  return r;
}

Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up) {
  const Vec3 f = normalize(target - eye);
  const Vec3 s = normalize(cross(f, up));
  const Vec3 u = cross(s, f);

  Mat4 r = Mat4::identity();
  r.m[0] = s.x;
  r.m[4] = s.y;
  r.m[8] = s.z;
  r.m[1] = u.x;
  r.m[5] = u.y;
  r.m[9] = u.z;
  r.m[2] = -f.x;
  r.m[6] = -f.y;
  r.m[10] = -f.z;
  r.m[12] = -dot(s, eye);
  r.m[13] = -dot(u, eye);
  r.m[14] = dot(f, eye);
  return r;
}

Mat4 perspective(float fov_y_radians, float aspect, float z_near, float z_far) {
  Mat4 r{};
  const float f = 1.f / std::tan(fov_y_radians * 0.5f);
  r.m[0] = f / aspect;
  r.m[5] = f;
  r.m[10] = (z_far + z_near) / (z_near - z_far);
  r.m[11] = -1.f;
  r.m[14] = (2.f * z_far * z_near) / (z_near - z_far);
  return r;
}


Mat4 orthographic(float left, float right, float bottom, float top, float z_near,
                  float z_far) {
  Mat4 out{};
  const float rl = right - left;
  const float tb = top - bottom;
  const float fn = z_far - z_near;
  out.m[0] = 2.f / rl;
  out.m[5] = 2.f / tb;
  out.m[10] = -2.f / fn;
  out.m[12] = -(right + left) / rl;
  out.m[13] = -(top + bottom) / tb;
  out.m[14] = -(z_far + z_near) / fn;
  out.m[15] = 1.f;
  return out;
}

}  // namespace fury
