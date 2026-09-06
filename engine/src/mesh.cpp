#include "fury/mesh.hpp"

#include <algorithm>

namespace fury {
namespace {

void push_quad(Mesh& mesh, const Vec3& a, const Vec3& b, const Vec3& c,
               const Vec3& d, const Vec3& normal, const Vec3& color,
               float uv_scale = 1.f) {
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  mesh.vertices.push_back({a, normal, color, {0.f, 0.f}});
  mesh.vertices.push_back({b, normal, color, {uv_scale, 0.f}});
  mesh.vertices.push_back({c, normal, color, {uv_scale, uv_scale}});
  mesh.vertices.push_back({d, normal, color, {0.f, uv_scale}});
  mesh.indices.push_back(base + 0);
  mesh.indices.push_back(base + 1);
  mesh.indices.push_back(base + 2);
  mesh.indices.push_back(base + 0);
  mesh.indices.push_back(base + 2);
  mesh.indices.push_back(base + 3);
}

}  // namespace

Mesh make_box(const Vec3& size, const Vec3& color) {
  return make_colored_box(size, color, color);
}

Mesh make_colored_box(const Vec3& size, const Vec3& color_top,
                      const Vec3& color_side) {
  Mesh mesh;
  const float hx = size.x * 0.5f;
  const float hy = size.y * 0.5f;
  const float hz = size.z * 0.5f;

  const Vec3 p000{-hx, -hy, -hz};
  const Vec3 p001{-hx, -hy, hz};
  const Vec3 p010{-hx, hy, -hz};
  const Vec3 p011{-hx, hy, hz};
  const Vec3 p100{hx, -hy, -hz};
  const Vec3 p101{hx, -hy, hz};
  const Vec3 p110{hx, hy, -hz};
  const Vec3 p111{hx, hy, hz};

  // +Z / -Z / +X / -X sides
  push_quad(mesh, p001, p101, p111, p011, {0.f, 0.f, 1.f}, color_side);
  push_quad(mesh, p100, p000, p010, p110, {0.f, 0.f, -1.f}, color_side);
  push_quad(mesh, p101, p100, p110, p111, {1.f, 0.f, 0.f}, color_side);
  push_quad(mesh, p000, p001, p011, p010, {-1.f, 0.f, 0.f}, color_side);
  // +Y top / -Y bottom
  push_quad(mesh, p011, p111, p110, p010, {0.f, 1.f, 0.f}, color_top);
  push_quad(mesh, p000, p100, p101, p001, {0.f, -1.f, 0.f}, color_side * 0.7f);
  return mesh;
}

Mesh make_plane(float width, float depth, const Vec3& color, float uv_scale) {
  Mesh mesh;
  const float hx = width * 0.5f;
  const float hz = depth * 0.5f;
  push_quad(mesh, {-hx, 0.f, -hz}, {hx, 0.f, -hz}, {hx, 0.f, hz},
            {-hx, 0.f, hz}, {0.f, 1.f, 0.f}, color, uv_scale);
  return mesh;
}

Mesh make_capsule(float radius, float height, const Vec3& color) {
  // Approximate capsule as body box + slightly wider head cube (AABB agents).
  Mesh body = make_box({radius * 2.f, std::max(height - radius * 1.2f, radius),
                        radius * 2.f},
                       color);
  Mesh head = make_box({radius * 2.15f, radius * 1.1f, radius * 2.15f},
                       color * 1.08f);
  const float body_hy = std::max(height - radius * 1.2f, radius) * 0.5f;
  const float head_y = body_hy + radius * 0.35f;
  for (Vertex& v : head.vertices) {
    v.position.y += head_y;
  }
  Mesh out = body;
  const std::uint32_t base = static_cast<std::uint32_t>(out.vertices.size());
  out.vertices.insert(out.vertices.end(), head.vertices.begin(),
                      head.vertices.end());
  for (std::uint32_t idx : head.indices) {
    out.indices.push_back(base + idx);
  }
  return out;
}

}  // namespace fury
