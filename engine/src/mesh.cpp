#include "fury/mesh.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

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

bool load_obj(const std::string& path, Mesh& out, const Vec3& default_color) {
  out = Mesh{};
  std::ifstream in(path);
  if (!in) {
    return false;
  }

  std::vector<Vec3> positions;
  std::vector<Vec3> normals;
  std::vector<Vec2> uvs;
  positions.reserve(64);
  normals.reserve(64);
  uvs.reserve(64);

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    // Strip CR for Windows-authored files
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    if (line.size() >= 2 && line[0] == 'v' && line[1] == ' ') {
      float x = 0.f, y = 0.f, z = 0.f;
      if (std::sscanf(line.c_str() + 2, "%f %f %f", &x, &y, &z) >= 3) {
        positions.push_back({x, y, z});
      }
      continue;
    }
    if (line.size() >= 3 && line[0] == 'v' && line[1] == 'n' && line[2] == ' ') {
      float x = 0.f, y = 0.f, z = 0.f;
      if (std::sscanf(line.c_str() + 3, "%f %f %f", &x, &y, &z) >= 3) {
        normals.push_back({x, y, z});
      }
      continue;
    }
    if (line.size() >= 3 && line[0] == 'v' && line[1] == 't' && line[2] == ' ') {
      float u = 0.f, v = 0.f;
      if (std::sscanf(line.c_str() + 3, "%f %f", &u, &v) >= 2) {
        uvs.push_back({u, v});
      }
      continue;
    }
    if (line.size() >= 2 && line[0] == 'f' && line[1] == ' ') {
      // Parse face corners: v, v/vt, v//vn, v/vt/vn (1-based; negative = relative)
      struct Corner {
        int vi{0};
        int ti{0};
        int ni{0};
      };
      std::vector<Corner> corners;
      corners.reserve(8);

      const char* p = line.c_str() + 2;
      while (*p) {
        while (*p == ' ' || *p == '\t') {
          ++p;
        }
        if (*p == '\0') {
          break;
        }
        Corner c;
        int consumed = 0;
        if (std::sscanf(p, "%d/%d/%d%n", &c.vi, &c.ti, &c.ni, &consumed) == 3) {
          corners.push_back(c);
          p += consumed;
          continue;
        }
        if (std::sscanf(p, "%d//%d%n", &c.vi, &c.ni, &consumed) == 2) {
          corners.push_back(c);
          p += consumed;
          continue;
        }
        if (std::sscanf(p, "%d/%d%n", &c.vi, &c.ti, &consumed) == 2) {
          corners.push_back(c);
          p += consumed;
          continue;
        }
        if (std::sscanf(p, "%d%n", &c.vi, &consumed) == 1) {
          corners.push_back(c);
          p += consumed;
          continue;
        }
        // Skip unknown token
        while (*p && *p != ' ' && *p != '\t') {
          ++p;
        }
      }

      if (corners.size() < 3) {
        continue;
      }

      auto resolve_pos = [&](int idx) -> const Vec3* {
        if (idx < 0) {
          idx = static_cast<int>(positions.size()) + idx + 1;
        }
        if (idx < 1 || idx > static_cast<int>(positions.size())) {
          return nullptr;
        }
        return &positions[static_cast<std::size_t>(idx - 1)];
      };
      auto resolve_uv = [&](int idx) -> Vec2 {
        if (idx == 0 || uvs.empty()) {
          return {0.f, 0.f};
        }
        if (idx < 0) {
          idx = static_cast<int>(uvs.size()) + idx + 1;
        }
        if (idx < 1 || idx > static_cast<int>(uvs.size())) {
          return {0.f, 0.f};
        }
        return uvs[static_cast<std::size_t>(idx - 1)];
      };
      auto resolve_n = [&](int idx) -> Vec3 {
        if (idx == 0 || normals.empty()) {
          return {0.f, 1.f, 0.f};
        }
        if (idx < 0) {
          idx = static_cast<int>(normals.size()) + idx + 1;
        }
        if (idx < 1 || idx > static_cast<int>(normals.size())) {
          return {0.f, 1.f, 0.f};
        }
        return normals[static_cast<std::size_t>(idx - 1)];
      };

      // Fan triangulation about corner 0
      for (std::size_t i = 1; i + 1 < corners.size(); ++i) {
        const Corner& c0 = corners[0];
        const Corner& c1 = corners[i];
        const Corner& c2 = corners[i + 1];
        const Vec3* p0 = resolve_pos(c0.vi);
        const Vec3* p1 = resolve_pos(c1.vi);
        const Vec3* p2 = resolve_pos(c2.vi);
        if (!p0 || !p1 || !p2) {
          continue;
        }

        Vec3 n0 = resolve_n(c0.ni);
        Vec3 n1 = resolve_n(c1.ni);
        Vec3 n2 = resolve_n(c2.ni);
        // If no normals were referenced, compute a flat face normal
        if (c0.ni == 0 && c1.ni == 0 && c2.ni == 0) {
          const Vec3 e1{p1->x - p0->x, p1->y - p0->y, p1->z - p0->z};
          const Vec3 e2{p2->x - p0->x, p2->y - p0->y, p2->z - p0->z};
          Vec3 fn{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z,
                  e1.x * e2.y - e1.y * e2.x};
          const float len =
              std::sqrt(fn.x * fn.x + fn.y * fn.y + fn.z * fn.z);
          if (len > 1e-8f) {
            fn.x /= len;
            fn.y /= len;
            fn.z /= len;
          } else {
            fn = {0.f, 1.f, 0.f};
          }
          n0 = n1 = n2 = fn;
        }

        const std::uint32_t base =
            static_cast<std::uint32_t>(out.vertices.size());
        out.vertices.push_back(
            {*p0, n0, default_color, resolve_uv(c0.ti)});
        out.vertices.push_back(
            {*p1, n1, default_color, resolve_uv(c1.ti)});
        out.vertices.push_back(
            {*p2, n2, default_color, resolve_uv(c2.ti)});
        out.indices.push_back(base + 0);
        out.indices.push_back(base + 1);
        out.indices.push_back(base + 2);
      }
    }
  }

  if (out.vertices.empty() || out.indices.empty()) {
    out = Mesh{};
    return false;
  }
  return true;
}

bool load_obj_asset(const char* filename, Mesh& out, const Vec3& default_color) {
  if (!filename || !filename[0]) {
    out = Mesh{};
    return false;
  }
  // Common layouts: run from repo root, build/, or build/apps/vaultline/
  static const char* kPrefixes[] = {
      "assets/meshes/",
      "../assets/meshes/",
      "../../assets/meshes/",
      "../../../assets/meshes/",
      "./",
  };
  for (const char* prefix : kPrefixes) {
    const std::string path = std::string(prefix) + filename;
    if (load_obj(path, out, default_color)) {
      return true;
    }
  }
  out = Mesh{};
  return false;
}

}  // namespace fury
