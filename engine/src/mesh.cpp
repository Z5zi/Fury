#include "fury/mesh.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>

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

float clampf(float v, float lo, float hi) {
  return (std::max)(lo, (std::min)(hi, v));
}

Vec3 clamp_color(const Vec3& c) {
  return {clampf(c.x, 0.f, 1.f), clampf(c.y, 0.f, 1.f), clampf(c.z, 0.f, 1.f)};
}

/// Limb box from joint `from` to tip `to` (pitch in YZ; X kept as offset).
void append_limb_ik(Mesh& mesh, const Vec3& from, const Vec3& to,
                    float thickness, const Vec3& color) {
  const float dx = to.x - from.x;
  const float dy = to.y - from.y;
  const float dz = to.z - from.z;
  const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (len < 1e-4f) {
    return;
  }
  // Bottom of pitched box at local (0,-len,0) maps with pitch=atan2(-dz,-dy).
  const float pitch = std::atan2(-dz, -dy);
  append_box_pitched(mesh, from, {dx * 0.5f, -len * 0.5f, 0.f},
                     {thickness, len, thickness}, color, pitch);
}

void build_humanoid_into(Mesh& mesh, float height, const Vec3& color,
                         float limb_phase, float breathe_phase,
                         float move_weight) {
  mesh.vertices.clear();
  mesh.indices.clear();
  mesh.mark_dirty();

  const float h = (std::max)(height, 1.2f);
  const float mw = clampf(move_weight, 0.f, 1.f);
  const float swing = std::sin(limb_phase) * 0.55f * mw;
  const float walk_bob = std::sin(limb_phase * 2.f) * (h * 0.012f) * mw;
  const float breathe = std::sin(breathe_phase) * (h * 0.008f);
  const float bob = walk_bob + breathe;

  // Clothing / skin / hair variation from the single tint color.
  const Vec3 shirt = clamp_color(color);
  const Vec3 pants = clamp_color(
      Vec3{color.x * 0.42f, color.y * 0.46f, color.z * 0.58f + 0.05f});
  const Vec3 skin = clamp_color(Vec3{color.x * 0.28f + 0.70f,
                                     color.y * 0.22f + 0.52f,
                                     color.z * 0.18f + 0.40f});
  const float lum = color.x * 0.3f + color.y * 0.5f + color.z * 0.2f;
  const Vec3 hair = clamp_color(
      Vec3{0.10f + lum * 0.22f, 0.07f + lum * 0.14f, 0.05f + lum * 0.10f});
  const Vec3 shoes = Vec3{0.11f, 0.10f, 0.09f};
  const Vec3 sleeve = clamp_color(shirt * 0.90f);

  const float torso_h = h * 0.30f;
  const float torso_w = h * 0.22f;
  const float torso_d = h * 0.14f;
  const float head_s = h * 0.13f;
  const float hair_h = h * 0.045f;
  const float arm_len = h * 0.22f;
  const float arm_w = h * 0.065f;
  const float hand_s = h * 0.055f;
  const float thigh_len = h * 0.28f;
  const float leg_w = h * 0.085f;
  const float foot_len = h * 0.10f;
  const float foot_h = h * 0.045f;
  const float foot_w = h * 0.08f;

  // Feet on y=0; mesh centered so entity.y ≈ height*0.5.
  const float y0 = -h * 0.5f;
  const float hip_y = y0 + thigh_len + foot_h * 0.85f;
  const float shoulder_y = hip_y + torso_h * 0.88f;
  const float torso_cy = hip_y + torso_h * 0.5f + bob;
  const float head_cy = shoulder_y + head_s * 0.70f + bob;
  const float hair_cy = head_cy + head_s * 0.42f + hair_h * 0.35f;

  // Torso (shirt) + head (skin) + hair cube
  append_box_at(mesh, {0.f, torso_cy, 0.f}, {torso_w, torso_h, torso_d}, shirt);
  append_box_at(mesh, {0.f, head_cy, 0.f}, {head_s, head_s, head_s}, skin);
  append_box_at(mesh, {0.f, hair_cy, 0.f},
                {head_s * 1.05f, hair_h, head_s * 1.08f}, hair);

  // Arms + hands (opposite swing) — pivot at shoulders
  const float arm_local_y = -arm_len * 0.45f;
  const Vec3 sh_l{-torso_w * 0.55f, shoulder_y + bob, 0.f};
  const Vec3 sh_r{torso_w * 0.55f, shoulder_y + bob, 0.f};
  append_box_pitched(mesh, sh_l, {0.f, arm_local_y, 0.f},
                     {arm_w, arm_len, arm_w}, sleeve, -swing);
  append_box_pitched(mesh, sh_r, {0.f, arm_local_y, 0.f},
                     {arm_w, arm_len, arm_w}, sleeve, swing);
  // Hands at wrist ends (same pitch as arms)
  {
    const float cp = std::cos(-swing);
    const float sp = std::sin(-swing);
    const float wrist_y = arm_local_y - arm_len * 0.42f;
    const float hy = wrist_y * cp;
    const float hz = wrist_y * sp;
    append_box_at(mesh, {sh_l.x, sh_l.y + hy, sh_l.z + hz},
                  {hand_s, hand_s * 0.7f, hand_s}, skin);
  }
  {
    const float cp = std::cos(swing);
    const float sp = std::sin(swing);
    const float wrist_y = arm_local_y - arm_len * 0.42f;
    const float hy = wrist_y * cp;
    const float hz = wrist_y * sp;
    append_box_at(mesh, {sh_r.x, sh_r.y + hy, sh_r.z + hz},
                  {hand_s, hand_s * 0.7f, hand_s}, skin);
  }

  // Legs — IK-ish foot plant: phase-synced foot targets reduce stance slide.
  const float stride = h * 0.11f * mw;
  const float step_up = h * 0.038f * mw;
  const float hip_x = leg_w * 0.75f;

  auto foot_target = [&](float phase_off, float x_off) {
    const float ph = limb_phase + phase_off;
    const float s = std::sin(ph);
    // Swing half: arc forward/back; stance half: damp Z travel (plant).
    const float z = (s >= 0.f) ? (s * stride) : (s * stride * 0.18f);
    const float y = y0 + step_up * (std::max)(0.f, s);
    return Vec3{x_off, y + foot_h * 0.5f, z};
  };

  const Vec3 hip_l{-hip_x, hip_y + bob, 0.f};
  const Vec3 hip_r{hip_x, hip_y + bob, 0.f};
  const Vec3 foot_l = foot_target(0.f, -hip_x);
  const Vec3 foot_r = foot_target(3.14159265f, hip_x);

  append_limb_ik(mesh, hip_l, {foot_l.x, foot_l.y + foot_h * 0.35f, foot_l.z},
                 leg_w, pants);
  append_limb_ik(mesh, hip_r, {foot_r.x, foot_r.y + foot_h * 0.35f, foot_r.z},
                 leg_w, pants);

  // Feet cubes (mostly horizontal) at planted / swing targets
  append_box_at(mesh, foot_l, {foot_w, foot_h, foot_len}, shoes);
  append_box_at(mesh, foot_r, {foot_w, foot_h, foot_len}, shoes);
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

Mesh make_uv_billboard(float width, float height, int segs_x, int segs_y,
                       const Vec3& color) {
  Mesh mesh;
  const int sx = (std::max)(1, segs_x);
  const int sy = (std::max)(1, segs_y);
  const float hw = width * 0.5f;
  const float hh = height * 0.5f;
  mesh.vertices.reserve(static_cast<std::size_t>((sx + 1) * (sy + 1)));
  mesh.indices.reserve(static_cast<std::size_t>(sx * sy * 6));
  const Vec3 n{0.f, 0.f, 1.f};
  for (int iy = 0; iy <= sy; ++iy) {
    const float v = static_cast<float>(iy) / static_cast<float>(sy);
    const float y = -hh + height * (1.f - v);  // v=0 top of atlas
    for (int ix = 0; ix <= sx; ++ix) {
      const float u = static_cast<float>(ix) / static_cast<float>(sx);
      const float x = -hw + width * u;
      mesh.vertices.push_back({{x, y, 0.f}, n, color, {u, v}});
    }
  }
  for (int iy = 0; iy < sy; ++iy) {
    for (int ix = 0; ix < sx; ++ix) {
      const std::uint32_t i0 = static_cast<std::uint32_t>(iy * (sx + 1) + ix);
      const std::uint32_t i1 = i0 + 1;
      const std::uint32_t i2 = i0 + static_cast<std::uint32_t>(sx + 1);
      const std::uint32_t i3 = i2 + 1;
      mesh.indices.push_back(i0);
      mesh.indices.push_back(i2);
      mesh.indices.push_back(i1);
      mesh.indices.push_back(i1);
      mesh.indices.push_back(i2);
      mesh.indices.push_back(i3);
    }
  }
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

Mesh make_humanoid(float height, const Vec3& color, float limb_phase,
                   float breathe_phase, float move_weight) {
  Mesh mesh;
  build_humanoid_into(mesh, height, color, limb_phase, breathe_phase,
                      move_weight);
  mesh.gpu_dirty = false;  // fresh mesh; upload will pick it up
  return mesh;
}

void pose_humanoid(Mesh& mesh, float height, const Vec3& color,
                   float limb_phase, float breathe_phase, float move_weight) {
  build_humanoid_into(mesh, height, color, limb_phase, breathe_phase,
                      move_weight);
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


namespace {

std::string dirname_of(const std::string& path) {
  const auto pos = path.find_last_of("/\\");
  if (pos == std::string::npos) return {};
  return path.substr(0, pos + 1);
}

std::string to_lower_copy(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return s;
}

TextureSlot texture_slot_from_mtl_name(const std::string& name) {
  const std::string n = to_lower_copy(name);
  auto has = [&](const char* tok) { return n.find(tok) != std::string::npos; };
  if (has("glass") || has("windshield") || has("window") || has("lens") ||
      has("transp")) {
    return TextureSlot::Glass;
  }
  if (has("rubber") || has("tire") || has("tyre")) {
    return TextureSlot::Rubber;
  }
  if (has("asphalt") || has("road") || has("tarmac")) {
    return TextureSlot::Asphalt;
  }
  if (has("brick") || has("masonry")) {
    return TextureSlot::Brick;
  }
  if (has("concrete") || has("stone") || has("curb") || has("sidewalk") ||
      has("pavement") || has("plaster") || has("stucco")) {
    return TextureSlot::Concrete;
  }
  if (has("wood") || has("crate") || has("bark")) {
    return TextureSlot::Wood;
  }
  if (has("barrel")) {
    return TextureSlot::BarrelMetal;
  }
  if (has("chrome") || has("metal") || has("steel") || has("paint") ||
      has("body") || has("livery") || has("alloy") || has("grille") ||
      has("mirror") || has("trim") || has("bumper") || has("metallic") ||
      has("clearcoat") || has("coat") || has("white") || has("hmpd")) {
    return TextureSlot::Metal;
  }
  if (has("skin") || has("skinwarm") || has("shirt") || has("pants") || has("hair") ||
      has("haircard") || has("shoes") || has("shoesole") || has("shoelace") || has("fabric") ||
      has("cloth") || has("jacket") || has("belt") || has("buckle") ||
      has("inner") || has("lip") || has("iris") || has("eyewhite") ||
      has("pupil") || has("cornea") || has("brow") || has("nail") || has("nostril") ||
      has("seam") || has("phone") || has("phonescreen")) {
    return TextureSlot::None;  // vertex/MTL albedo carries clothing color
  }
  return TextureSlot::None;
}

Material material_from_mtl(const std::string& name, const Vec3& kd, const Vec3& ks,
                           const Vec3& ke, float ns, float d, int illum) {
  Material m;
  m.albedo = kd;
  // Ns is Phong exponent — map to roughness roughly.
  const float ns_cl = std::clamp(ns, 1.f, 1000.f);
  m.roughness = std::clamp(1.f - std::log2(ns_cl + 1.f) / 10.f, 0.04f, 0.98f);
  const float spec = (ks.x + ks.y + ks.z) / 3.f;
  const std::string n = to_lower_copy(name);
  const bool name_metal =
      n.find("metal") != std::string::npos || n.find("chrome") != std::string::npos ||
      n.find("steel") != std::string::npos || n.find("alloy") != std::string::npos ||
      n.find("metallic") != std::string::npos || n.find("paint") != std::string::npos ||
      n.find("body") != std::string::npos || n.find("livery") != std::string::npos;
  m.metallic = name_metal ? std::clamp(0.35f + spec * 0.6f, 0.f, 1.f)
                          : ((illum >= 3) ? std::clamp(spec, 0.f, 0.85f) : 0.f);
  // Cycle-5: high-Ns + metal → visible clearcoat response on soft path
  if (m.metallic > 0.45f && ns_cl > 200.f) {
    m.clearcoat = std::max(m.clearcoat, 0.75f);
    m.roughness = std::min(m.roughness, 0.22f);
  }
  if (n.find("rubber") != std::string::npos || n.find("tire") != std::string::npos) {
    m.metallic = 0.f;
    m.roughness = std::max(m.roughness, 0.85f);
  }
  if (n.find("glass") != std::string::npos || n.find("windshield") != std::string::npos ||
      n.find("window") != std::string::npos || n.find("lens") != std::string::npos) {
    m.metallic = std::min(m.metallic, 0.15f);
    m.roughness = std::min(m.roughness, 0.18f);
    m.transmission = 0.65f;
    m.opacity = std::clamp(d, 0.15f, 1.f);
    m.alpha_blend = m.opacity < 0.99f;
  }
  // Cycle-5: clamp emissive — high Ke DRL/lamps caused white sparkle in stills
  const float emit = (ke.x + ke.y + ke.z) / 3.f;
  m.emissive = std::clamp(emit, 0.f, 2.8f);
  if (emit > 0.01f) {
    const float scale = (emit > 1e-4f) ? (m.emissive / emit) : 1.f;
    m.emissive_color = {ke.x * scale, ke.y * scale, ke.z * scale};
  }
  m.texture = texture_slot_from_mtl_name(name);
  // Clearcoat paint: tighten roughness + explicit clearcoat lobe for soft path.
  if (n.find("paint") != std::string::npos || n.find("clearcoat") != std::string::npos ||
      n.find("livery") != std::string::npos || n.find("body") != std::string::npos ||
      n.find("blue_metallic") != std::string::npos || n.find("white") != std::string::npos ||
      n.find("hmpdv") != std::string::npos) {
    m.roughness = std::min(m.roughness, 0.18f);
    m.metallic = std::max(m.metallic, 0.72f);
    m.clearcoat = std::max(m.clearcoat, 0.95f);
    if (m.texture == TextureSlot::None) m.texture = TextureSlot::Metal;
  }
  if (n.find("chrome") != std::string::npos || n.find("mirror") != std::string::npos ||
      n.find("alloy") != std::string::npos) {
    m.metallic = std::max(m.metallic, 0.92f);
    m.roughness = std::min(m.roughness, 0.18f);
    m.clearcoat = std::max(m.clearcoat, 0.35f);
  }
  if (n.find("skin") != std::string::npos || n.find("lip") != std::string::npos ||
      n.find("nail") != std::string::npos || n.find("nostril") != std::string::npos) {
    m.roughness = std::clamp(m.roughness, 0.30f, 0.58f);
    m.metallic = 0.f;
    // Cycle-8: stronger SSS-ish warmth + facial oil sheen (soft face readability)
    if (n.find("skinwarm") != std::string::npos) {
      m.albedo = {std::min(1.f, m.albedo.x * 1.14f), m.albedo.y * 0.92f, m.albedo.z * 0.86f};
      m.clearcoat = std::max(m.clearcoat, 0.26f);
      m.roughness = std::clamp(m.roughness, 0.34f, 0.48f);
    } else if (n.find("skin") != std::string::npos) {
      m.albedo = {m.albedo.x * 1.06f, m.albedo.y * 0.97f, m.albedo.z * 0.93f};
      m.clearcoat = std::max(m.clearcoat, 0.24f);
      m.roughness = std::clamp(m.roughness, 0.34f, 0.48f);
    }
    if (n.find("lip") != std::string::npos) {
      m.clearcoat = std::max(m.clearcoat, 0.55f);
      m.roughness = std::min(m.roughness, 0.28f);
      m.albedo = {std::min(1.f, m.albedo.x * 1.08f), m.albedo.y * 0.92f, m.albedo.z * 0.92f};
    }
    if (n.find("nail") != std::string::npos) {
      m.clearcoat = std::max(m.clearcoat, 0.55f);
      m.roughness = std::min(m.roughness, 0.22f);
    }
  }
  if (n.find("eyewhite") != std::string::npos || n.find("iris") != std::string::npos ||
      n.find("cornea") != std::string::npos) {
    m.roughness = std::min(m.roughness, 0.14f);
    m.metallic = 0.02f;
    m.clearcoat = std::max(m.clearcoat, 0.70f);
    if (n.find("eyewhite") != std::string::npos) {
      m.albedo = {std::min(1.f, m.albedo.x * 1.08f), std::min(1.f, m.albedo.y * 1.08f),
                  std::min(1.f, m.albedo.z * 1.08f)};
    }
    if (n.find("iris") != std::string::npos) {
      m.clearcoat = std::max(m.clearcoat, 0.62f);
      m.roughness = std::min(m.roughness, 0.16f);
    }
    if (n.find("cornea") != std::string::npos) {
      m.roughness = std::min(m.roughness, 0.03f);
      m.clearcoat = std::max(m.clearcoat, 0.99f);
      m.transmission = std::max(m.transmission, 0.22f);
    }
  }
  if (n.find("haircard") != std::string::npos) {
    m.roughness = std::clamp(m.roughness, 0.22f, 0.45f);
    m.clearcoat = std::max(m.clearcoat, 0.35f);
    m.metallic = std::min(m.metallic, 0.08f);
  }
  if (n.find("seam") != std::string::npos) {
    m.roughness = std::max(m.roughness, 0.70f);
    m.metallic = 0.f;
  }
  if (n.find("shirt") != std::string::npos || n.find("pants") != std::string::npos ||
      n.find("fabric") != std::string::npos || n.find("cloth") != std::string::npos ||
      n.find("inner") != std::string::npos) {
    m.roughness = std::max(m.roughness, 0.72f);
    m.metallic = 0.f;
  }
  if (n.find("jacket") != std::string::npos || n.find("coat") != std::string::npos) {
    m.roughness = std::clamp(m.roughness, 0.45f, 0.70f);
    m.metallic = std::min(m.metallic, 0.06f);
  }
  if (n.find("belt") != std::string::npos) {
    m.roughness = std::min(m.roughness, 0.45f);
    m.metallic = std::max(m.metallic, 0.12f);
  }
  if (n.find("hair") != std::string::npos) {
    m.roughness = std::clamp(m.roughness, 0.32f, 0.58f);
    m.metallic = std::min(m.metallic, 0.10f);
    m.clearcoat = std::max(m.clearcoat, 0.15f);
  }
  if (n.find("shoes") != std::string::npos || n.find("boot") != std::string::npos) {
    m.roughness = std::min(m.roughness, 0.42f);
    m.metallic = std::max(m.metallic, 0.12f);
    m.clearcoat = std::max(m.clearcoat, 0.22f);  // leather sheen
  }
  if (n.find("shoesole") != std::string::npos || n.find("rubber") != std::string::npos) {
    m.roughness = std::max(m.roughness, 0.88f);
    m.metallic = 0.f;
    m.clearcoat = 0.f;
    if (m.texture == TextureSlot::None) m.texture = TextureSlot::Rubber;
  }
  if (n.find("shoelace") != std::string::npos) {
    m.roughness = std::max(m.roughness, 0.75f);
    m.metallic = 0.f;
  }
  if (n.find("buckle") != std::string::npos) {
    m.metallic = std::max(m.metallic, 0.85f);
    m.roughness = std::min(m.roughness, 0.28f);
    m.clearcoat = std::max(m.clearcoat, 0.35f);
    if (m.texture == TextureSlot::None) m.texture = TextureSlot::Metal;
  }
  if (n.find("phone") != std::string::npos) {
    m.metallic = std::max(m.metallic, 0.70f);
    m.roughness = std::min(m.roughness, 0.35f);
    m.clearcoat = std::max(m.clearcoat, 0.40f);
    if (n.find("screen") != std::string::npos) {
      m.emissive = std::max(m.emissive, 0.85f);
      m.metallic = 0.1f;
      m.roughness = 0.25f;
    }
  }
  if (n.find("stone") != std::string::npos || n.find("marble") != std::string::npos ||
      n.find("tile") != std::string::npos) {
    m.clearcoat = std::max(m.clearcoat, 0.25f);
    m.roughness = std::min(m.roughness, 0.45f);
  }
  if (n.find("lamp") != std::string::npos || n.find("bulb") != std::string::npos ||
      n.find("emissive") != std::string::npos || n.find("drl") != std::string::npos ||
      n.find("hlbulb") != std::string::npos || n.find("amber") != std::string::npos) {
    if (m.emissive < 0.15f) m.emissive = std::max(m.emissive, 0.55f);
  }
  // Cycle-5: hide debug/wire/rain-streak layers that read as sparkle in soft stills
  if (n.find("wire") != std::string::npos || n.find("rainstreak") != std::string::npos ||
      n.find("rain_streak") != std::string::npos || n.find("debug") != std::string::npos) {
    m.opacity = 0.0f;
    m.alpha_blend = true;
    m.emissive = 0.f;
    m.albedo = {0.02f, 0.02f, 0.02f};
  }
  if (n.find("fade") != std::string::npos) {
    m.opacity = std::min(m.opacity, 0.15f);
    m.alpha_blend = true;
    m.emissive = 0.f;
  }
  return m;
}

struct MtlRecord {
  Vec3 kd{0.8f, 0.8f, 0.8f};
  Vec3 ks{0.2f, 0.2f, 0.2f};
  Vec3 ke{0.f, 0.f, 0.f};
  float ns{20.f};
  float d{1.f};
  int illum{2};
};

bool parse_mtl_file(const std::string& path, std::unordered_map<std::string, MtlRecord>& out) {
  std::ifstream in(path);
  if (!in) return false;
  std::string line;
  std::string cur;
  MtlRecord rec;
  auto flush = [&]() {
    if (!cur.empty()) out[cur] = rec;
  };
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    if (line.rfind("newmtl ", 0) == 0) {
      flush();
      cur = line.substr(7);
      while (!cur.empty() && (cur.back() == ' ' || cur.back() == '\t')) cur.pop_back();
      rec = MtlRecord{};
      continue;
    }
    if (cur.empty()) continue;
    float a = 0.f, b = 0.f, c = 0.f;
    if (std::sscanf(line.c_str(), "Kd %f %f %f", &a, &b, &c) == 3) {
      rec.kd = {a, b, c};
    } else if (std::sscanf(line.c_str(), "Ks %f %f %f", &a, &b, &c) == 3) {
      rec.ks = {a, b, c};
    } else if (std::sscanf(line.c_str(), "Ke %f %f %f", &a, &b, &c) == 3) {
      rec.ke = {a, b, c};
    } else if (std::sscanf(line.c_str(), "Ns %f", &a) == 1) {
      rec.ns = a;
    } else if (std::sscanf(line.c_str(), "d %f", &a) == 1) {
      rec.d = a;
    } else if (std::sscanf(line.c_str(), "Tr %f", &a) == 1) {
      rec.d = 1.f - a;
    } else if (std::sscanf(line.c_str(), "illum %f", &a) == 1) {
      rec.illum = static_cast<int>(a);
    }
  }
  flush();
  return !out.empty();
}

}  // namespace

bool load_obj_mtl(const std::string& path, std::vector<ObjPart>& out,
                  const Vec3& default_color) {
  out.clear();
  std::ifstream in(path);
  if (!in) return false;

  std::vector<Vec3> positions;
  std::vector<Vec3> normals;
  std::vector<Vec2> uvs;
  positions.reserve(256);
  normals.reserve(256);
  uvs.reserve(256);

  std::unordered_map<std::string, MtlRecord> mtl_db;
  std::string mtllib_name;
  std::string cur_mtl = "__default__";

  struct Accum {
    Mesh mesh;
    Material material;
  };
  std::unordered_map<std::string, Accum> parts;
  auto& def = parts[cur_mtl];
  def.material.albedo = default_color;

  auto ensure_part = [&](const std::string& name) -> Accum& {
    auto it = parts.find(name);
    if (it != parts.end()) return it->second;
    Accum a;
    auto mit = mtl_db.find(name);
    if (mit != mtl_db.end()) {
      a.material = material_from_mtl(name, mit->second.kd, mit->second.ks,
                                     mit->second.ke, mit->second.ns,
                                     mit->second.d, mit->second.illum);
    } else {
      a.material.albedo = default_color;
      a.material.texture = texture_slot_from_mtl_name(name);
    }
    return parts.emplace(name, std::move(a)).first->second;
  };

  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;

    if (line.rfind("mtllib ", 0) == 0) {
      mtllib_name = line.substr(7);
      while (!mtllib_name.empty() &&
             (mtllib_name.front() == ' ' || mtllib_name.front() == '\t')) {
        mtllib_name.erase(mtllib_name.begin());
      }
      while (!mtllib_name.empty() &&
             (mtllib_name.back() == ' ' || mtllib_name.back() == '\t')) {
        mtllib_name.pop_back();
      }
      const std::string mtl_path = dirname_of(path) + mtllib_name;
      parse_mtl_file(mtl_path, mtl_db);
      continue;
    }
    if (line.rfind("usemtl ", 0) == 0) {
      cur_mtl = line.substr(7);
      while (!cur_mtl.empty() &&
             (cur_mtl.back() == ' ' || cur_mtl.back() == '\t')) {
        cur_mtl.pop_back();
      }
      if (cur_mtl.empty()) cur_mtl = "__default__";
      ensure_part(cur_mtl);
      continue;
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
      struct Corner {
        int vi{0};
        int ti{0};
        int ni{0};
      };
      std::vector<Corner> corners;
      corners.reserve(8);
      const char* p = line.c_str() + 2;
      while (*p) {
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0') break;
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
        while (*p && *p != ' ' && *p != '\t') ++p;
      }
      if (corners.size() < 3) continue;

      auto resolve_pos = [&](int idx) -> const Vec3* {
        if (idx < 0) idx = static_cast<int>(positions.size()) + idx + 1;
        if (idx < 1 || idx > static_cast<int>(positions.size())) return nullptr;
        return &positions[static_cast<std::size_t>(idx - 1)];
      };
      auto resolve_uv = [&](int idx) -> Vec2 {
        if (idx == 0 || uvs.empty()) return {0.f, 0.f};
        if (idx < 0) idx = static_cast<int>(uvs.size()) + idx + 1;
        if (idx < 1 || idx > static_cast<int>(uvs.size())) return {0.f, 0.f};
        return uvs[static_cast<std::size_t>(idx - 1)];
      };
      auto resolve_n = [&](int idx) -> Vec3 {
        if (idx == 0 || normals.empty()) return {0.f, 1.f, 0.f};
        if (idx < 0) idx = static_cast<int>(normals.size()) + idx + 1;
        if (idx < 1 || idx > static_cast<int>(normals.size())) return {0.f, 1.f, 0.f};
        return normals[static_cast<std::size_t>(idx - 1)];
      };

      Accum& part = ensure_part(cur_mtl);
      const Vec3 vert_col = part.material.albedo;
      for (std::size_t i = 1; i + 1 < corners.size(); ++i) {
        const Corner& c0 = corners[0];
        const Corner& c1 = corners[i];
        const Corner& c2 = corners[i + 1];
        const Vec3* p0 = resolve_pos(c0.vi);
        const Vec3* p1 = resolve_pos(c1.vi);
        const Vec3* p2 = resolve_pos(c2.vi);
        if (!p0 || !p1 || !p2) continue;
        Vec3 n0 = resolve_n(c0.ni);
        Vec3 n1 = resolve_n(c1.ni);
        Vec3 n2 = resolve_n(c2.ni);
        if (c0.ni == 0 && c1.ni == 0 && c2.ni == 0) {
          const Vec3 e1{p1->x - p0->x, p1->y - p0->y, p1->z - p0->z};
          const Vec3 e2{p2->x - p0->x, p2->y - p0->y, p2->z - p0->z};
          Vec3 fn{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z,
                  e1.x * e2.y - e1.y * e2.x};
          const float len = std::sqrt(fn.x * fn.x + fn.y * fn.y + fn.z * fn.z);
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
            static_cast<std::uint32_t>(part.mesh.vertices.size());
        part.mesh.vertices.push_back({*p0, n0, vert_col, resolve_uv(c0.ti)});
        part.mesh.vertices.push_back({*p1, n1, vert_col, resolve_uv(c1.ti)});
        part.mesh.vertices.push_back({*p2, n2, vert_col, resolve_uv(c2.ti)});
        part.mesh.indices.push_back(base + 0);
        part.mesh.indices.push_back(base + 1);
        part.mesh.indices.push_back(base + 2);
      }
    }
  }

  out.reserve(parts.size());
  for (auto& kv : parts) {
    if (kv.second.mesh.vertices.empty() || kv.second.mesh.indices.empty()) {
      continue;
    }
    ObjPart op;
    op.name = kv.first;
    op.mesh = std::move(kv.second.mesh);
    op.material = kv.second.material;
    // Vertex colors already carry Kd — keep material.albedo near white so soft
    // path doesn't double-tint, except for emissive/glass emphasis.
    if (op.material.emissive < 0.01f) {
      op.material.albedo = {1.f, 1.f, 1.f};
    }
    out.push_back(std::move(op));
  }
  return !out.empty();
}

bool load_obj_mtl_asset(const char* filename, std::vector<ObjPart>& out,
                        const Vec3& default_color) {
  out.clear();
  if (!filename || !filename[0]) return false;
  static const char* kPrefixes[] = {
      "assets/meshes/",
      "../assets/meshes/",
      "../../assets/meshes/",
      "../../../assets/meshes/",
      "./",
  };
  for (const char* prefix : kPrefixes) {
    const std::string path = std::string(prefix) + filename;
    if (load_obj_mtl(path, out, default_color)) {
      return true;
    }
  }
  return false;
}


}  // namespace fury
