#include "fury/mesh.hpp"

#include <algorithm>
#include <cmath>

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

void append_box_at(Mesh& mesh, const Vec3& center, const Vec3& size,
                   const Vec3& color) {
  Mesh part = make_colored_box(size, color, color);
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  for (Vertex& v : part.vertices) {
    v.position.x += center.x;
    v.position.y += center.y;
    v.position.z += center.z;
  }
  mesh.vertices.insert(mesh.vertices.end(), part.vertices.begin(),
                       part.vertices.end());
  for (std::uint32_t idx : part.indices) {
    mesh.indices.push_back(base + idx);
  }
}

/// Axis-aligned box with optional pitch (X) rotation about a world pivot —
/// used for limb swing stubs (no skeletal format).
void append_box_pitched(Mesh& mesh, const Vec3& pivot, const Vec3& local_center,
                        const Vec3& size, const Vec3& color, float pitch) {
  Mesh part = make_colored_box(size, color, color);
  const float cp = std::cos(pitch);
  const float sp = std::sin(pitch);
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  for (Vertex& v : part.vertices) {
    // Local offset from limb pivot (shoulder/hip)
    float lx = v.position.x + local_center.x;
    float ly = v.position.y + local_center.y;
    float lz = v.position.z + local_center.z;
    // Pitch around local X (swing forward/back in YZ)
    const float ry = ly * cp - lz * sp;
    const float rz = ly * sp + lz * cp;
    v.position.x = pivot.x + lx;
    v.position.y = pivot.y + ry;
    v.position.z = pivot.z + rz;
    // Rotate normal similarly
    const float ny = v.normal.y * cp - v.normal.z * sp;
    const float nz = v.normal.y * sp + v.normal.z * cp;
    v.normal.y = ny;
    v.normal.z = nz;
  }
  mesh.vertices.insert(mesh.vertices.end(), part.vertices.begin(),
                       part.vertices.end());
  for (std::uint32_t idx : part.indices) {
    mesh.indices.push_back(base + idx);
  }
}

void build_humanoid_into(Mesh& mesh, float height, const Vec3& color,
                         float limb_phase) {
  mesh.vertices.clear();
  mesh.indices.clear();
  mesh.gpu_dirty = true;

  const float h = (std::max)(height, 1.2f);
  const float swing = std::sin(limb_phase) * 0.55f;  // radians-ish amplitude
  const float bob = std::sin(limb_phase * 2.f) * (h * 0.012f);

  const float torso_h = h * 0.32f;
  const float torso_w = h * 0.22f;
  const float torso_d = h * 0.14f;
  const float head_s = h * 0.14f;
  const float arm_len = h * 0.28f;
  const float arm_w = h * 0.07f;
  const float leg_len = h * 0.36f;
  const float leg_w = h * 0.09f;

  // Feet on y=0; entity position is typically mid-height (~0.9).
  // Mesh is centered so entity.y ≈ height*0.5 matches capsule convention.
  const float y0 = -h * 0.5f;

  const float hip_y = y0 + leg_len;
  const float shoulder_y = hip_y + torso_h * 0.85f;
  const float torso_cy = hip_y + torso_h * 0.5f + bob;
  const float head_cy = shoulder_y + head_s * 0.65f + bob;

  // Torso + head
  append_box_at(mesh, {0.f, torso_cy, 0.f}, {torso_w, torso_h, torso_d}, color);
  append_box_at(mesh, {0.f, head_cy, 0.f}, {head_s, head_s, head_s},
                color * 1.08f);

  // Arms (opposite swing) — pivot at shoulders
  const float arm_local_y = -arm_len * 0.45f;
  append_box_pitched(mesh, {-torso_w * 0.55f, shoulder_y + bob, 0.f},
                     {0.f, arm_local_y, 0.f}, {arm_w, arm_len, arm_w},
                     color * 0.92f, -swing);
  append_box_pitched(mesh, {torso_w * 0.55f, shoulder_y + bob, 0.f},
                     {0.f, arm_local_y, 0.f}, {arm_w, arm_len, arm_w},
                     color * 0.92f, swing);

  // Legs (opposite to arms) — pivot at hips
  const float leg_local_y = -leg_len * 0.5f;
  append_box_pitched(mesh, {-leg_w * 0.7f, hip_y + bob, 0.f},
                     {0.f, leg_local_y, 0.f}, {leg_w, leg_len, leg_w},
                     color * 0.78f, swing);
  append_box_pitched(mesh, {leg_w * 0.7f, hip_y + bob, 0.f},
                     {0.f, leg_local_y, 0.f}, {leg_w, leg_len, leg_w},
                     color * 0.78f, -swing);
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
  Mesh body = make_box({radius * 2.f, (std::max)(height - radius * 1.2f, radius),
                        radius * 2.f},
                       color);
  Mesh head = make_box({radius * 2.15f, radius * 1.1f, radius * 2.15f},
                       color * 1.08f);
  const float body_hy = (std::max)(height - radius * 1.2f, radius) * 0.5f;
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

Mesh make_humanoid(float height, const Vec3& color, float limb_phase) {
  Mesh mesh;
  build_humanoid_into(mesh, height, color, limb_phase);
  mesh.gpu_dirty = false;  // fresh mesh; upload will pick it up
  return mesh;
}

void pose_humanoid(Mesh& mesh, float height, const Vec3& color,
                   float limb_phase) {
  build_humanoid_into(mesh, height, color, limb_phase);
}

}  // namespace fury
