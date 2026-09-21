#pragma once
// AAA Meridian Mutual benchmark block — Harbor Metro / HMPD / Meridian Mutual only.
// Cycle-6: PIXEL QUALITY — Blender AAA humans, reflections impossible to miss,
// clean render, expensive night interaction, asphalt physicality at capture distance.
// Harbor Metro / HMPD / Meridian Mutual only — no Rockstar/GTA IP.

#include <fury/fury.hpp>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace aaa_meridian {

using fury::Aabb;
using fury::Entity;
using fury::Material;
using fury::TextureSlot;
using fury::Vec3;

inline bool camera_inside_solid(const Vec3& cam, const std::vector<Aabb>& solids,
                                float margin = 0.15f) {
  for (const Aabb& box : solids) {
    const Vec3 mn = box.min();
    const Vec3 mx = box.max();
    if (cam.x >= mn.x - margin && cam.x <= mx.x + margin &&
        cam.y >= mn.y - margin && cam.y <= mx.y + margin &&
        cam.z >= mn.z - margin && cam.z <= mx.z + margin) {
      return true;
    }
  }
  return false;
}

/// Nudge camera out of solids along +Y then along away-from-center XZ.
inline Vec3 reject_camera_in_mesh(Vec3 cam, const std::vector<Aabb>& solids) {
  for (int iter = 0; iter < 8; ++iter) {
    bool hit = false;
    for (const Aabb& box : solids) {
      const Vec3 mn = box.min();
      const Vec3 mx = box.max();
      if (cam.x < mn.x || cam.x > mx.x || cam.y < mn.y || cam.y > mx.y ||
          cam.z < mn.z || cam.z > mx.z) {
        continue;
      }
      hit = true;
      // Prefer lifting above the solid (street cams), else push to nearest face.
      const float up = (mx.y + 0.35f) - cam.y;
      const float dx0 = cam.x - mn.x;
      const float dx1 = mx.x - cam.x;
      const float dz0 = cam.z - mn.z;
      const float dz1 = mx.z - cam.z;
      const float best = std::min({up, dx0, dx1, dz0, dz1});
      if (best == up) {
        cam.y = mx.y + 0.35f;
      } else if (best == dx0) {
        cam.x = mn.x - 0.35f;
      } else if (best == dx1) {
        cam.x = mx.x + 0.35f;
      } else if (best == dz0) {
        cam.z = mn.z - 0.35f;
      } else {
        cam.z = mx.z + 0.35f;
      }
    }
    if (!hit) break;
  }
  if (cam.y < 0.6f) cam.y = 0.6f;
  return cam;
}

inline Material mat_asphalt() {
  Material m;
  m.albedo = {0.78f, 0.78f, 0.80f};  // multiplied by dark asphalt tex
  m.roughness = 0.55f;               // Cycle-6: wetter base — SSR/env must scream
  m.metallic = 0.08f;
  m.wetness = 0.82f;
  m.clearcoat = 0.35f;
  m.texture = TextureSlot::Asphalt;
  return m;
}
inline Material mat_concrete() {
  Material m;
  m.albedo = {1.02f, 1.0f, 0.96f};
  m.roughness = 0.78f;
  m.metallic = 0.04f;
  m.texture = TextureSlot::Concrete;
  return m;
}
inline Material mat_brick() {
  Material m;
  m.albedo = {1.0f, 0.95f, 0.9f};
  m.roughness = 0.68f;
  m.metallic = 0.03f;
  m.texture = TextureSlot::Brick;
  return m;
}
inline Material mat_glass() {
  Material m;
  m.albedo = {0.42f, 0.62f, 0.92f};
  m.roughness = 0.04f;
  m.metallic = 0.02f;
  m.emissive = 0.10f;
  m.transmission = 0.82f;
  m.opacity = 0.38f;
  m.alpha_blend = true;
  m.clearcoat = 0.95f;
  m.texture = TextureSlot::Glass;
  return m;
}
inline Material mat_painted_metal(const Vec3& rgb) {
  Material m;
  m.albedo = rgb;
  m.roughness = 0.18f;
  m.metallic = 0.88f;
  m.clearcoat = 0.95f;
  m.texture = TextureSlot::Metal;
  return m;
}
inline Material mat_chrome() {
  Material m;
  m.albedo = {0.90f, 0.92f, 0.96f};
  m.roughness = 0.08f;
  m.metallic = 0.98f;
  m.clearcoat = 0.55f;
  m.texture = TextureSlot::Metal;
  return m;
}
inline Material mat_wood() {
  Material m;
  m.albedo = {0.55f, 0.38f, 0.22f};
  m.roughness = 0.72f;
  m.metallic = 0.02f;
  m.texture = TextureSlot::Wood;
  return m;
}
inline Material mat_marble() {
  Material m;
  m.albedo = {0.92f, 0.90f, 0.86f};
  m.roughness = 0.22f;
  m.metallic = 0.06f;
  m.clearcoat = 0.65f;
  m.texture = TextureSlot::Concrete;
  return m;
}
inline Material mat_rubber() {
  Material m;
  m.albedo = {0.95f, 0.95f, 0.95f};
  m.roughness = 0.95f;
  m.metallic = 0.0f;
  m.texture = TextureSlot::Rubber;
  return m;
}

inline void add_contact_blob(fury::Scene& scene, fury::Mesh* plane, const char* name,
                             const Vec3& pos, float sx, float sz) {
  Entity e;
  e.name = name;
  e.tag = "contact_shadow";
  e.mesh = plane;
  e.transform.position = {pos.x, 0.02f, pos.z};
  e.transform.scale = {sx, 1.f, sz};
  e.material.albedo = {0.05f, 0.05f, 0.06f};
  e.material.roughness = 1.f;
  e.material.emissive = 0.f;
  e.detail = true;
  scene.add_entity(std::move(e));
}


/// Spawn every MTL group as its own entity (soft multi-material pipeline).
inline int spawn_obj_mtl(fury::Scene& scene, const char* soft_path,
                         const char* full_path, const char* name_prefix,
                         const fury::Transform& xf, const char* tag = "",
                         bool solid = false,
                         Aabb collider = Aabb{},
                         const Material* fallback_mat = nullptr) {
  std::vector<fury::ObjPart> parts;
  const char* used = nullptr;
  if (fury::load_obj_mtl_asset(soft_path, parts)) {
    used = soft_path;
  } else if (fury::load_obj_mtl_asset(full_path, parts)) {
    used = full_path;
  }
  if (!used || parts.empty()) {
    fury::Log::warn(std::string("AAA multi-mat miss: ") + soft_path);
    return 0;
  }
  fury::Log::info(std::string("AAA multi-mat ") + name_prefix + " from " + used +
                  " parts=" + std::to_string(parts.size()));
  int n = 0;
  for (fury::ObjPart& part : parts) {
    // Cycle-5: drop wire/rain-streak/debug parts that sparkle in soft stills
    {
      std::string pl = part.name;
      for (char& c : pl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if (pl.find("wire") != std::string::npos || pl.find("rainstreak") != std::string::npos ||
          pl.find("rain_streak") != std::string::npos || pl.find("debug") != std::string::npos) {
        continue;
      }
    }
    Entity e;
    e.name = std::string(name_prefix) + "_" + part.name;
    e.tag = tag ? tag : "";
    e.mesh = scene.add_mesh(std::move(part.mesh));
    e.material = part.material;
    if (fallback_mat && e.material.texture == TextureSlot::None &&
        e.material.emissive < 0.01f) {
      // Keep MTL albedo (already baked into verts / material); optionally stamp slot.
      if (fallback_mat->texture != TextureSlot::None) {
        e.material.texture = fallback_mat->texture;
      }
    }
    e.transform = xf;
    if (solid && n == 0) {
      e.solid = true;
      e.collider = collider;
    } else {
      e.detail = true;
    }
    scene.add_entity(std::move(e));
    ++n;
  }
  return n;
}

inline fury::Mesh* load_hm(fury::Scene& scene, const char* soft_path,
                           const char* full_path, fury::Mesh fallback,
                           bool prefer_full = true) {
  fury::Mesh loaded;
  auto try_load = [&](const char* path, const char* label) -> fury::Mesh* {
    if (fury::load_obj_asset(path, loaded)) {
      fury::Log::info(std::string("AAA mesh ") + label + ": " + path +
                      " verts=" + std::to_string(loaded.vertices.size()));
      return scene.add_mesh(std::move(loaded));
    }
    return nullptr;
  };
  fury::Mesh* m = nullptr;
  if (prefer_full) {
    m = try_load(full_path, "full");
    if (!m) m = try_load(soft_path, "softLOD");
  } else {
    m = try_load(soft_path, "softLOD");
    if (!m) m = try_load(full_path, "full");
  }
  if (m) return m;
  fury::Log::warn(std::string("AAA mesh FALLBACK box for ") + full_path);
  return scene.add_mesh(std::move(fallback));
}

/// Top-10 block builder. Call after build_harbor_metro().
inline void build_aaa_meridian_block(fury::Scene& scene) {
  // ---- 2. Kill debug placeholders in Meridian spawn area ----
  for (Entity& e : scene.entities()) {
    const Vec3& p = e.transform.position;
    const bool near_block =
        p.x > -55.f && p.x < 55.f && p.z > -30.f && p.z < 28.f;
    if (!near_block) continue;
    // Hide primitive parked-car proxies & neon slabs that read as debug.
    if (e.name.rfind("ParkedCar", 0) == 0 || e.name.rfind("ParkedCab", 0) == 0 ||
        e.name.rfind("NeonSign", 0) == 0 || e.name == "ExtractionPad" ||
        e.name.rfind("ExtractSign", 0) == 0 || e.name.rfind("WinZ", 0) == 0 ||
        e.name.rfind("WinX", 0) == 0 || e.name.rfind("Billboard", 0) == 0 ||
        e.name.rfind("District", 0) == 0 || e.name.rfind("StreetSign", 0) == 0 ||
        e.name.rfind("HarborWater", 0) == 0) {
      e.visible = false;
      e.solid = false;
      continue;
    }
    // Flatten cyan-looking emissive escape cues near plaza.
    if (e.tag == "escape" && near_block) {
      e.visible = false;
    }
    // Hide overlapping box buildings that the authored kit replaces.
    if (e.name.rfind("Bldg", 0) == 0) {
      const int id = std::atoi(e.name.c_str() + 4);
      if (id <= 8) {
        e.visible = false;
        e.solid = false;
      }
    }
    // Hide procedural Meridian exterior walls/roof — annex shell replaces them.
    if (e.name == "BankWallN" || e.name == "BankWallW" || e.name == "BankWallE" ||
        e.name == "BankWallSL" || e.name == "BankWallSR" || e.name == "BankRoof" ||
        e.name == "StreetGrid" || e.name == "BankPlaza" || e.name == "Sidewalk" ||
        e.name == "SidewalkNS") {
      e.visible = false;
      if (e.name != "StreetGrid" && e.name != "Sidewalk" && e.name != "SidewalkNS")
        e.solid = false;
    }
  }

  // ---- 4. Rebuild street surface: road → curb → sidewalk + lane marks + manhole ----
  // Base fill — kills sky-blue clear showing through as "debug ground"
  auto* base_fill = scene.add_mesh(
      fury::make_plane(140.f, 110.f, Vec3{0.12f, 0.12f, 0.13f}, 28.f));
  {
    Entity e;
    e.name = "AaaBaseFill";
    e.tag = "asphalt";
    e.mesh = base_fill;
    e.transform.position = {5.f, 0.004f, 14.f};
    e.material = mat_asphalt();
    e.material.albedo = {0.42f, 0.42f, 0.44f};
    e.material.wetness = 0.78f;
    scene.add_entity(std::move(e));
  }
  // Extra near-camera ground slabs (kill clear-color holes at capture pitches)
  auto* cam_ground = scene.add_mesh(
      fury::make_plane(60.f, 50.f, Vec3{0.14f, 0.14f, 0.15f}, 16.f));
  for (const Vec3& gp : {Vec3{10.f, 0.006f, 24.f}, Vec3{24.f, 0.006f, 17.5f},
                         Vec3{6.f, 0.006f, 16.f}, Vec3{35.f, 0.006f, 12.5f},
                         Vec3{20.f, 0.006f, 22.f}}) {
    Entity e;
    e.name = "AaaCamGround";
    e.tag = "asphalt";
    e.mesh = cam_ground;
    e.transform.position = gp;
    e.material = mat_asphalt();
    e.material.albedo = {0.48f, 0.48f, 0.50f};
    scene.add_entity(std::move(e));
  }
  // Plaza apron in front of Meridian (BankPlaza was hidden)
  auto* meridian_plaza = scene.add_mesh(
      fury::make_plane(24.f, 10.f, Vec3{0.55f, 0.53f, 0.48f}, 6.f));
  {
    Entity e;
    e.name = "AaaMeridianPlaza";
    e.mesh = meridian_plaza;
    e.transform.position = {35.f, 0.03f, 9.5f};
    e.material = mat_concrete();
    e.material.albedo = {0.68f, 0.66f, 0.62f};
    e.material.roughness = 0.75f;
    scene.add_entity(std::move(e));
  }
  auto* road = scene.add_mesh(
      fury::make_plane(48.f, 22.f, Vec3{0.14f, 0.14f, 0.15f}, 14.f));
  {
    Entity e;
    e.name = "AaaRoad";
    e.tag = "asphalt";
    e.mesh = road;
    e.transform.position = {5.f, 0.01f, 14.f};
    e.material = mat_asphalt();
    scene.add_entity(std::move(e));
  }
  auto* road_ns = scene.add_mesh(
      fury::make_plane(18.f, 40.f, Vec3{0.14f, 0.14f, 0.15f}, 12.f));
  {
    Entity e;
    e.name = "AaaRoadNS";
    e.tag = "asphalt";
    e.mesh = road_ns;
    e.transform.position = {5.f, 0.012f, 10.f};
    e.material = mat_asphalt();
    scene.add_entity(std::move(e));
  }

  auto* curb = scene.add_mesh(
      fury::make_box({48.f, 0.18f, 0.45f}, Vec3{0.55f, 0.54f, 0.50f}));
  Material curb_m = mat_concrete();
  curb_m.albedo = {0.72f, 0.70f, 0.66f};
  curb_m.roughness = 0.7f;
  for (float z : { 8.5f, 19.5f }) {
    Entity e;
    e.name = "AaaCurbEW";
    e.mesh = curb;
    e.transform.position = {0.f, 0.09f, z};
    e.material = curb_m;
    e.solid = true;
    e.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, {48.f, 0.18f, 0.45f});
    scene.add_entity(std::move(e));
  }
  auto* curb_ns = scene.add_mesh(
      fury::make_box({0.45f, 0.18f, 28.f}, Vec3{0.55f, 0.54f, 0.50f}));
  for (float x : { -20.f, 30.f }) {
    Entity e;
    e.name = "AaaCurbNS";
    e.mesh = curb_ns;
    e.transform.position = {x, 0.09f, 14.f};
    e.material = curb_m;
    e.solid = true;
    e.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, {0.45f, 0.18f, 28.f});
    scene.add_entity(std::move(e));
  }

  auto* sidewalk = scene.add_mesh(
      fury::make_plane(46.f, 5.5f, Vec3{0.55f, 0.53f, 0.48f}, 8.f));
  {
    Entity e;
    e.name = "AaaSidewalkN";
    e.mesh = sidewalk;
    e.transform.position = {5.f, 0.06f, 7.2f};
    e.material = mat_concrete();
    scene.add_entity(std::move(e));
  }
  {
    Entity e;
    e.name = "AaaSidewalkS";
    e.mesh = sidewalk;
    e.transform.position = {5.f, 0.06f, 20.8f};
    e.material = mat_concrete();
    scene.add_entity(std::move(e));
  }
  auto* sidewalk_ns = scene.add_mesh(
      fury::make_plane(5.5f, 30.f, Vec3{0.55f, 0.53f, 0.48f}, 8.f));
  for (float x : { -22.f, 32.f }) {
    Entity e;
    e.name = "AaaSidewalkEW";
    e.mesh = sidewalk_ns;
    e.transform.position = {x, 0.065f, 14.f};
    e.material = mat_concrete();
    scene.add_entity(std::move(e));
  }

  // Lane markings
  auto* stripe = scene.add_mesh(
      fury::make_box({1.8f, 0.03f, 0.18f}, Vec3{0.92f, 0.90f, 0.75f}));
  Material stripe_m;
  stripe_m.albedo = {1.15f, 1.1f, 0.85f};
  stripe_m.roughness = 0.55f;
  stripe_m.emissive = 0.02f;
  for (float x = -10.f; x <= 28.f; x += 4.f) {
    Entity e;
    e.name = "AaaLaneMark";
    e.mesh = stripe;
    e.transform.position = {x, 0.025f, 14.f};
    e.material = stripe_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  auto* cross = scene.add_mesh(
      fury::make_box({0.35f, 0.03f, 3.2f}, Vec3{0.92f, 0.90f, 0.75f}));
  for (float x = -4.f; x <= 4.f; x += 0.9f) {
    Entity e;
    e.name = "AaaCrosswalk";
    e.mesh = cross;
    e.transform.position = {x, 0.026f, 10.5f};
    e.material = stripe_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }

  // Manhole decals
  auto* manhole = scene.add_mesh(
      fury::make_box({0.9f, 0.04f, 0.9f}, Vec3{0.25f, 0.26f, 0.28f}));
  Material mh = mat_painted_metal({0.35f, 0.36f, 0.38f});
  mh.roughness = 0.55f;
  for (const Vec3& p : {Vec3{-2.f, 0.03f, 15.5f}, Vec3{12.f, 0.03f, 13.f},
                        Vec3{22.f, 0.03f, 16.f}}) {
    Entity e;
    e.name = "AaaManhole";
    e.mesh = manhole;
    e.transform.position = p;
    e.material = mh;
    e.detail = true;
    scene.add_entity(std::move(e));
  }


  // ---- 5. Street material treatment: cracks, patches, stains, tiles ----
  auto* crack = scene.add_mesh(
      fury::make_box({2.8f, 0.02f, 0.08f}, Vec3{0.08f, 0.08f, 0.09f}));
  Material crack_m = mat_asphalt();
  crack_m.albedo = {0.22f, 0.22f, 0.24f};
  crack_m.roughness = 0.98f;
  for (const Vec3& p : {Vec3{-4.f, 0.03f, 13.2f}, Vec3{8.f, 0.03f, 15.8f},
                        Vec3{16.f, 0.03f, 12.4f}, Vec3{3.f, 0.03f, 16.5f}}) {
    Entity e;
    e.name = "AaaAsphaltCrack";
    e.mesh = crack;
    e.transform.position = p;
    e.transform.rotation_euler = {0.f, p.x * 0.15f, 0.f};
    e.material = crack_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  auto* patch = scene.add_mesh(
      fury::make_box({2.2f, 0.025f, 1.4f}, Vec3{0.18f, 0.17f, 0.16f}));
  Material patch_m = mat_asphalt();
  patch_m.albedo = {0.38f, 0.36f, 0.34f};
  patch_m.roughness = 0.88f;
  patch_m.wetness = 0.25f;
  for (const Vec3& p : {Vec3{0.f, 0.028f, 14.8f}, Vec3{14.f, 0.028f, 13.5f},
                        Vec3{22.f, 0.028f, 15.2f}}) {
    Entity e;
    e.name = "AaaAsphaltPatch";
    e.mesh = patch;
    e.transform.position = p;
    e.material = patch_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  auto* stain = scene.add_mesh(
      fury::make_plane(1.6f, 1.1f, Vec3{0.12f, 0.10f, 0.08f}, 1.f));
  Material stain_m = mat_asphalt();
  stain_m.albedo = {0.28f, 0.22f, 0.14f};
  stain_m.roughness = 0.55f;
  stain_m.wetness = 0.75f;
  for (const Vec3& p : {Vec3{6.f, 0.029f, 14.2f}, Vec3{19.f, 0.029f, 15.6f}}) {
    Entity e;
    e.name = "AaaOilStain";
    e.mesh = stain;
    e.transform.position = p;
    e.material = stain_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  // Sidewalk tile variation (slight albedo jitter via separate plates)
  auto* tile = scene.add_mesh(
      fury::make_plane(1.15f, 1.15f, Vec3{0.62f, 0.60f, 0.55f}, 1.f));
  for (int i = 0; i < 10; ++i) {
    Entity e;
    e.name = "AaaSidewalkTile";
    e.mesh = tile;
    e.transform.position = {8.f + static_cast<float>(i) * 1.25f, 0.072f, 7.2f};
    e.material = mat_concrete();
    const float j = 0.92f + 0.03f * static_cast<float>((i * 3) % 5);
    e.material.albedo = {j, j * 0.98f, j * 0.94f};
    e.material.roughness = 0.72f + 0.04f * static_cast<float>(i % 3);
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  // Curb wear / dirt edge
  auto* curb_dirt = scene.add_mesh(
      fury::make_box({40.f, 0.02f, 0.2f}, Vec3{0.35f, 0.32f, 0.28f}));
  Material dirt_m = mat_concrete();
  dirt_m.albedo = {0.42f, 0.38f, 0.32f};
  dirt_m.roughness = 0.92f;
  for (float z : {8.7f, 19.3f}) {
    Entity e;
    e.name = "AaaCurbDirt";
    e.mesh = curb_dirt;
    e.transform.position = {5.f, 0.11f, z};
    e.material = dirt_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }

  // Plaza in front of Meridian Mutual (no flat placeholder ground)
  auto* plaza = scene.add_mesh(
      fury::make_plane(20.f, 10.f, Vec3{0.58f, 0.56f, 0.52f}, 6.f));
  {
    Entity e;
    e.name = "AaaMeridianPlaza";
    e.mesh = plaza;
    e.transform.position = {20.f, 0.07f, 8.5f};  // Meridian Mutual plaza
    e.material = mat_concrete();
    e.material.albedo = {1.05f, 1.02f, 0.96f};
    scene.add_entity(std::move(e));
  }

  // ---- Authored Harbor Metro frontages (MTL multi-material) ----
  // Soft path now ingests usemtl groups → paint/glass/rubber/chrome/etc.
  fury::Transform kit_xf;  // native Blender world positions
  Material bank_fb = mat_concrete();
  bank_fb.albedo = {0.95f, 0.96f, 0.98f};
  bank_fb.roughness = 0.58f;
  if (spawn_obj_mtl(scene, "harbor_metro/hm_bank_annex_v10_soft.obj",
                    "harbor_metro/hm_bank_annex_v10.obj", "AaaMeridianAnnex",
                    kit_xf, "bank", true,
                    Aabb::from_center_size({35.f, 8.f, -2.f}, {40.f, 20.f, 16.f}),
                    &bank_fb) == 0) {
    auto* bank_mesh = load_hm(
        scene, "harbor_metro/hm_bank_annex_v10_soft.obj",
        "harbor_metro/hm_bank_annex_v10.obj",
        fury::make_colored_box({18.f, 10.f, 14.f}, {0.82f, 0.84f, 0.88f},
                               {0.55f, 0.56f, 0.60f}));
    Entity e;
    e.name = "AaaMeridianAnnex";
    e.tag = "bank";
    e.mesh = bank_mesh;
    e.material = bank_fb;
    e.solid = true;
    e.collider = Aabb::from_center_size({35.f, 8.f, -2.f}, {40.f, 20.f, 16.f});
    scene.add_entity(std::move(e));
  }

  Material store_fb = mat_brick();
  if (spawn_obj_mtl(scene, "harbor_metro/hm_storefront_v10_soft.obj",
                    "harbor_metro/hm_storefront_v10.obj", "AaaStorefrontW",
                    kit_xf, "", true,
                    Aabb::from_center_size({-32.f, 5.f, 0.f}, {30.f, 12.f, 12.f}),
                    &store_fb) == 0) {
    auto* store_mesh = load_hm(
        scene, "harbor_metro/hm_storefront_v10_soft.obj",
        "harbor_metro/hm_storefront_v10.obj",
        fury::make_colored_box({12.f, 8.f, 10.f}, {0.55f, 0.42f, 0.36f},
                               {0.40f, 0.30f, 0.26f}));
    Entity e;
    e.name = "AaaStorefrontW";
    e.mesh = store_mesh;
    e.material = store_fb;
    e.solid = true;
    e.collider = Aabb::from_center_size({-32.f, 5.f, 0.f}, {30.f, 12.f, 12.f});
    scene.add_entity(std::move(e));
  }

  Material mid_fb = mat_concrete();
  mid_fb.roughness = 0.52f;
  if (spawn_obj_mtl(scene, "harbor_metro/hm_midrise_v10_soft.obj",
                    "harbor_metro/hm_midrise_v10.obj", "AaaMidriseE", kit_xf, "",
                    true, Aabb::from_center_size({1.f, 10.f, -1.f}, {50.f, 24.f, 16.f}),
                    &mid_fb) == 0) {
    auto* mid_mesh = load_hm(
        scene, "harbor_metro/hm_midrise_v10_soft.obj",
        "harbor_metro/hm_midrise_v10.obj",
        fury::make_colored_box({14.f, 16.f, 12.f}, {0.40f, 0.46f, 0.55f},
                               {0.28f, 0.32f, 0.40f}));
    Entity e;
    e.name = "AaaMidriseE";
    e.mesh = mid_mesh;
    e.material = mid_fb;
    e.solid = true;
    e.collider = Aabb::from_center_size({1.f, 10.f, -1.f}, {50.f, 24.f, 16.f});
    scene.add_entity(std::move(e));
  }

  // ---- 4+6. Hero HMPD cruiser full material stack via MTL ----
  const Vec3 cruiser_pos{18.f, 0.0f, 14.5f};
  fury::Transform cruiser_xf;
  cruiser_xf.position = cruiser_pos;
  cruiser_xf.rotation_euler = {0.f, 1.5708f, 0.f};
  Material cruiser_fb = mat_painted_metal({0.08f, 0.14f, 0.38f});
  cruiser_fb.roughness = 0.22f;
  cruiser_fb.clearcoat = 0.98f;
  cruiser_fb.roughness = 0.16f;
  cruiser_fb.metallic = std::max(cruiser_fb.metallic, 0.82f);
  cruiser_fb.albedo = {0.12f, 0.22f, 0.55f};
  // Cycle-5: prefer FULL v12b for hero stills (soft LOD reads as wire/fragmented)
  const int cruiser_parts = spawn_obj_mtl(
      scene, "harbor_metro/hmpd_cruiser_v12b.obj",
      "harbor_metro/hmpd_cruiser_v12b_soft.obj", "AaaHmpdCruiser", cruiser_xf,
      "hmpd_cruiser", true,
      Aabb::from_center_size({0.f, 0.85f, 0.f}, {5.2f, 1.7f, 2.2f}),
      &cruiser_fb);
  if (cruiser_parts == 0) {
    auto* cruiser = load_hm(
        scene, "harbor_metro/hmpd_cruiser_v12b_soft.obj",
        "harbor_metro/hmpd_cruiser_v12b.obj",
        fury::make_colored_box({2.0f, 1.5f, 4.6f}, {0.05f, 0.12f, 0.35f},
                               {0.04f, 0.08f, 0.22f}));
    Entity e;
    e.name = "AaaHmpdCruiser";
    e.tag = "hmpd_cruiser";
    e.mesh = cruiser;
    e.transform = cruiser_xf;
    e.material = cruiser_fb;
    e.solid = true;
    e.collider = Aabb::from_center_size({0.f, 0.85f, 0.f}, {5.2f, 1.7f, 2.2f});
    scene.add_entity(std::move(e));
  }
  fury::Log::info("AAA cruiser material parts=" + std::to_string(cruiser_parts));
  // Cycle-6: force paint/glass clearcoat so soft stills scream reflections
  for (Entity& e : scene.entities()) {
    if (e.tag != "hmpd_cruiser" && e.name.rfind("AaaHmpdCruiser", 0) != 0) continue;
    const std::string n = e.name;
    // Glass groups keep transmission; paint/metal get max coat
    if (e.material.transmission < 0.05f && e.material.emissive < 0.5f) {
      e.material.roughness = std::min(e.material.roughness, 0.14f);
      e.material.metallic = std::max(e.material.metallic, 0.78f);
      e.material.clearcoat = std::max(e.material.clearcoat, 0.98f);
      e.material.wetness = std::max(e.material.wetness, 0.25f);
      if (e.material.texture == TextureSlot::None) e.material.texture = TextureSlot::Metal;
    } else if (e.material.transmission > 0.05f || e.material.texture == TextureSlot::Glass) {
      e.material.roughness = std::min(e.material.roughness, 0.06f);
      e.material.clearcoat = std::max(e.material.clearcoat, 0.95f);
      e.material.wetness = std::max(e.material.wetness, 0.35f);
    }
  }

  // Cycle-6: bright env reflection cards (sky + warm façade) — must read on wet/paint
  {
    auto* sky_card = scene.add_mesh(
        fury::make_plane(40.f, 12.f, Vec3{0.75f, 0.82f, 0.95f}, 1.f));
    Material sky_m;
    sky_m.albedo = {0.70f, 0.78f, 0.95f};
    sky_m.roughness = 1.f;
    sky_m.emissive = 1.8f;
    sky_m.emissive_color = {0.75f, 0.82f, 1.0f};
    Entity sc;
    sc.name = "AaaSkyReflectCard";
    sc.mesh = sky_card;
    sc.transform.position = {18.f, 18.f, 8.f};
    sc.transform.rotation_euler = {1.2f, 0.f, 0.f};
    sc.material = sky_m;
    sc.detail = true;
    scene.add_entity(std::move(sc));
    auto* warm_card = scene.add_mesh(
        fury::make_plane(28.f, 10.f, Vec3{1.f, 0.85f, 0.55f}, 1.f));
    Material warm_m;
    warm_m.albedo = {1.0f, 0.82f, 0.55f};
    warm_m.roughness = 1.f;
    warm_m.emissive = 2.2f;
    warm_m.emissive_color = {1.0f, 0.78f, 0.45f};
    Entity wc;
    wc.name = "AaaWarmFacadeCard";
    wc.mesh = warm_card;
    wc.transform.position = {32.f, 10.f, 2.f};
    wc.transform.rotation_euler = {0.15f, -0.4f, 0.f};
    wc.material = warm_m;
    wc.detail = true;
    scene.add_entity(std::move(wc));
    // Vertical window strips for structured reflections
    auto* win = scene.add_mesh(fury::make_box({0.4f, 8.f, 0.15f}, {1.f, 0.95f, 0.8f}));
    Material win_m;
    win_m.albedo = {1.0f, 0.92f, 0.75f};
    win_m.emissive = 2.4f;
    win_m.emissive_color = {1.0f, 0.90f, 0.70f};
    win_m.roughness = 0.4f;
    for (int i = 0; i < 6; ++i) {
      Entity we;
      we.name = "AaaWindowStrip" + std::to_string(i);
      we.mesh = win;
      we.transform.position = {28.f + (i % 3) * 3.5f, 6.f + (i / 3) * 4.f, 1.5f};
      we.material = win_m;
      we.detail = true;
      scene.add_entity(std::move(we));
    }
  }

  // Contact blob kept as soft grounding assist under directional shadows.
  auto* blob_plane = scene.add_mesh(
      fury::make_plane(1.f, 1.f, Vec3{0.05f, 0.05f, 0.05f}, 1.f));
  add_contact_blob(scene, blob_plane, "AaaCruiserShadow", cruiser_pos, 5.4f, 2.4f);

  // ---- 8. Prop density: ATMs, bollards, benches, barriers ----
  auto* bollard = scene.add_mesh(
      fury::make_box({0.28f, 1.0f, 0.28f}, Vec3{0.55f, 0.55f, 0.50f}));
  Material bol_m = mat_painted_metal({0.62f, 0.62f, 0.58f});
  bol_m.roughness = 0.45f;
  for (float x = 14.f; x <= 30.f; x += 2.0f) {
    Entity e;
    e.name = "AaaBollard";
    e.mesh = bollard;
    e.transform.position = {x, 0.5f, 8.0f};
    e.material = bol_m;
    e.solid = true;
    e.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, {0.35f, 1.0f, 0.35f});
    e.detail = true;
    scene.add_entity(std::move(e));
    add_contact_blob(scene, blob_plane, "AaaBollardShadow", e.transform.position,
                     0.55f, 0.55f);
  }

  auto* bench = scene.add_mesh(
      fury::make_box({2.2f, 0.45f, 0.7f}, Vec3{0.35f, 0.28f, 0.20f}));
  Material bench_m;
  bench_m.albedo = {0.85f, 0.70f, 0.48f};
  bench_m.roughness = 0.72f;
  bench_m.texture = TextureSlot::Wood;
  for (const Vec3& p : {Vec3{12.f, 0.35f, 7.5f}, Vec3{28.f, 0.35f, 7.5f},
                        Vec3{-20.f, 0.35f, 18.f}, Vec3{8.f, 0.35f, 19.f}}) {
    Entity e;
    e.name = "AaaBench";
    e.mesh = bench;
    e.transform.position = p;
    e.material = bench_m;
    e.solid = true;
    e.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, {2.2f, 0.45f, 0.7f});
    scene.add_entity(std::move(e));
    add_contact_blob(scene, blob_plane, "AaaBenchShadow", p, 2.4f, 0.9f);
  }

  auto* barrier = scene.add_mesh(
      fury::make_box({1.6f, 1.05f, 0.22f}, Vec3{0.85f, 0.55f, 0.12f}));
  Material bar_m = mat_painted_metal({1.05f, 0.72f, 0.18f});
  bar_m.roughness = 0.5f;
  for (float x = 10.f; x <= 22.f; x += 2.5f) {
    Entity e;
    e.name = "AaaBarrier";
    e.mesh = barrier;
    e.transform.position = {x, 0.55f, 17.5f};
    e.material = bar_m;
    e.solid = true;
    e.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, {1.6f, 1.05f, 0.22f});
    e.detail = true;
    scene.add_entity(std::move(e));
  }

  auto* atm = scene.add_mesh(
      fury::make_box({0.9f, 1.6f, 0.45f}, Vec3{0.75f, 0.78f, 0.82f}));
  Material atm_m = mat_painted_metal({0.85f, 0.88f, 0.92f});
  atm_m.roughness = 0.32f;
  for (const Vec3& p : {Vec3{14.5f, 0.85f, 7.6f}, Vec3{15.8f, 0.85f, 7.6f}}) {
    Entity e;
    e.name = "AaaStreetAtm";
    e.mesh = atm;
    e.transform.position = p;
    e.material = atm_m;
    e.solid = true;
    e.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, {0.9f, 1.6f, 0.45f});
    scene.add_entity(std::move(e));
    add_contact_blob(scene, blob_plane, "AaaAtmShadow", p, 1.1f, 0.7f);
  }
  auto* atm_scr = scene.add_mesh(
      fury::make_box({0.7f, 0.45f, 0.05f}, Vec3{0.25f, 0.95f, 0.55f}));
  Material scr;
  scr.albedo = {0.35f, 1.0f, 0.55f};
  scr.emissive = 0.55f;
  scr.roughness = 0.85f;
  for (float x : {14.5f, 15.8f}) {
    Entity e;
    e.name = "AaaAtmScreen";
    e.mesh = atm_scr;
    e.transform.position = {x, 1.25f, 7.85f};
    e.material = scr;
    e.detail = true;
    scene.add_entity(std::move(e));
  }

  // Trash / hydrant from existing kits if present
  fury::Mesh crate_m;
  fury::Mesh* crate = nullptr;
  if (fury::load_obj_asset("crate.obj", crate_m)) {
    crate = scene.add_mesh(std::move(crate_m));
  } else {
    crate = scene.add_mesh(fury::make_box({1.f, 1.f, 1.f}, {0.55f, 0.4f, 0.22f}));
  }
  Material crate_mat;
  crate_mat.albedo = {1.0f, 0.9f, 0.75f};
  crate_mat.roughness = 0.78f;
  crate_mat.texture = TextureSlot::Wood;
  for (const Vec3& p : {Vec3{8.f, 0.55f, 17.f}, Vec3{-18.f, 0.55f, 16.f}}) {
    Entity e;
    e.name = "AaaCrate";
    e.mesh = crate;
    e.transform.position = p;
    e.transform.scale = {1.1f, 1.1f, 1.1f};
    e.material = crate_mat;
    e.solid = true;
    e.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, {1.1f, 1.1f, 1.1f});
    scene.add_entity(std::move(e));
    add_contact_blob(scene, blob_plane, "AaaCrateShadow", p, 1.3f, 1.3f);
  }

  // Meridian Mutual entrance signage (Harbor Metro branding only)
  auto* sign = scene.add_mesh(
      fury::make_box({4.5f, 0.7f, 0.18f}, Vec3{0.12f, 0.18f, 0.35f}));
  Material sign_m = mat_painted_metal({0.15f, 0.28f, 0.55f});
  sign_m.emissive = 0.12f;
  {
    Entity e;
    e.name = "AaaMeridianSign";
    e.mesh = sign;
    e.transform.position = {35.f, 4.2f, 7.2f};
    e.material = sign_m;
    scene.add_entity(std::move(e));
  }
  auto* sign_txt = scene.add_mesh(
      fury::make_box({4.1f, 0.45f, 0.06f}, Vec3{0.95f, 0.92f, 0.80f}));
  Material txt;
  txt.albedo = {1.1f, 1.05f, 0.85f};
  txt.emissive = 0.35f;
  txt.roughness = 0.9f;
  {
    Entity e;
    e.name = "AaaMeridianSignText";
    e.mesh = sign_txt;
    e.transform.position = {35.f, 4.2f, 7.35f};
    e.material = txt;
    scene.add_entity(std::move(e));
  }

  // ---- Cycle-3: Authored Meridian Mutual location finish ----
  // Entrance door + frame + trim
  auto* door = scene.add_mesh(
      fury::make_box({1.6f, 2.6f, 0.12f}, Vec3{0.12f, 0.14f, 0.18f}));
  Material door_m = mat_painted_metal({0.08f, 0.10f, 0.14f});
  door_m.roughness = 0.35f;
  door_m.clearcoat = 0.5f;
  {
    Entity e;
    e.name = "AaaMeridianDoor";
    e.mesh = door;
    e.transform.position = {35.f, 1.4f, 6.55f};
    e.material = door_m;
    e.solid = true;
    e.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, {1.6f, 2.6f, 0.2f});
    scene.add_entity(std::move(e));
  }
  auto* door_glass = scene.add_mesh(
      fury::make_box({1.2f, 1.4f, 0.04f}, Vec3{0.55f, 0.72f, 0.95f}));
  {
    Entity e;
    e.name = "AaaMeridianDoorGlass";
    e.mesh = door_glass;
    e.transform.position = {35.f, 1.7f, 6.62f};
    e.material = mat_glass();
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  auto* door_frame = scene.add_mesh(
      fury::make_box({2.0f, 2.9f, 0.18f}, Vec3{0.55f, 0.52f, 0.48f}));
  Material frame_m = mat_concrete();
  frame_m.albedo = {0.78f, 0.76f, 0.72f};
  frame_m.roughness = 0.55f;
  {
    Entity e;
    e.name = "AaaMeridianDoorFrame";
    e.mesh = door_frame;
    e.transform.position = {35.f, 1.5f, 6.45f};
    e.material = frame_m;
    scene.add_entity(std::move(e));
  }
  // Brass handle
  auto* handle = scene.add_mesh(
      fury::make_box({0.08f, 0.55f, 0.08f}, Vec3{0.72f, 0.58f, 0.28f}));
  Material brass = mat_chrome();
  brass.albedo = {0.78f, 0.62f, 0.28f};
  brass.metallic = 0.9f;
  {
    Entity e;
    e.name = "AaaMeridianDoorHandle";
    e.mesh = handle;
    e.transform.position = {35.7f, 1.35f, 6.7f};
    e.material = brass;
    e.detail = true;
    scene.add_entity(std::move(e));
  }

  // Window bays with frames, glass, blinds, interior room glow
  auto* win_frame = scene.add_mesh(
      fury::make_box({2.4f, 2.2f, 0.14f}, Vec3{0.45f, 0.44f, 0.42f}));
  auto* win_glass = scene.add_mesh(
      fury::make_box({2.1f, 1.9f, 0.05f}, Vec3{0.5f, 0.7f, 0.95f}));
  auto* win_blind = scene.add_mesh(
      fury::make_box({2.0f, 0.08f, 0.03f}, Vec3{0.75f, 0.72f, 0.65f}));
  auto* win_room = scene.add_mesh(
      fury::make_box({2.0f, 1.7f, 0.8f}, Vec3{0.55f, 0.48f, 0.38f}));
  Material blind_m;
  blind_m.albedo = {0.82f, 0.78f, 0.68f};
  blind_m.roughness = 0.7f;
  Material room_m;
  room_m.albedo = {0.55f, 0.48f, 0.40f};
  room_m.roughness = 0.85f;
  room_m.emissive = 0.45f;
  room_m.emissive_color = {1.0f, 0.92f, 0.75f};
  const Vec3 win_pos[] = {
      {28.5f, 3.2f, 6.4f}, {31.5f, 3.2f, 6.4f}, {38.5f, 3.2f, 6.4f},
      {41.5f, 3.2f, 6.4f}, {28.5f, 5.8f, 6.4f}, {31.5f, 5.8f, 6.4f},
      {38.5f, 5.8f, 6.4f}, {41.5f, 5.8f, 6.4f},
      {28.5f, 8.4f, 6.4f}, {38.5f, 8.4f, 6.4f},
  };
  for (const Vec3& wp : win_pos) {
    {
      Entity e;
      e.name = "AaaMeridianWinFrame";
      e.mesh = win_frame;
      e.transform.position = wp;
      e.material = frame_m;
      e.detail = true;
      scene.add_entity(std::move(e));
    }
    {
      Entity e;
      e.name = "AaaMeridianWinRoom";
      e.mesh = win_room;
      e.transform.position = {wp.x, wp.y, wp.z - 0.55f};
      e.material = room_m;
      e.detail = true;
      scene.add_entity(std::move(e));
    }
    {
      Entity e;
      e.name = "AaaMeridianWinGlass";
      e.mesh = win_glass;
      e.transform.position = {wp.x, wp.y, wp.z + 0.05f};
      e.material = mat_glass();
      e.material.emissive = 0.28f;
      e.detail = true;
      scene.add_entity(std::move(e));
    }
    for (int bi = 0; bi < 6; ++bi) {
      Entity e;
      e.name = "AaaMeridianBlind";
      e.mesh = win_blind;
      e.transform.position = {wp.x, wp.y + 0.7f - bi * 0.28f, wp.z + 0.02f};
      e.material = blind_m;
      e.detail = true;
      scene.add_entity(std::move(e));
    }
  }

  // Architectural trim / cornice
  auto* cornice = scene.add_mesh(
      fury::make_box({22.f, 0.35f, 0.55f}, Vec3{0.7f, 0.68f, 0.64f}));
  Material corn_m = mat_concrete();
  corn_m.albedo = {0.88f, 0.86f, 0.82f};
  corn_m.roughness = 0.48f;
  corn_m.clearcoat = 0.15f;
  {
    Entity e;
    e.name = "AaaMeridianCornice";
    e.mesh = cornice;
    e.transform.position = {35.f, 10.2f, 6.3f};
    e.material = corn_m;
    scene.add_entity(std::move(e));
  }
  auto* pilaster = scene.add_mesh(
      fury::make_box({0.45f, 9.5f, 0.4f}, Vec3{0.72f, 0.70f, 0.66f}));
  for (float x : {24.5f, 45.5f}) {
    Entity e;
    e.name = "AaaMeridianPilaster";
    e.mesh = pilaster;
    e.transform.position = {x, 5.0f, 6.35f};
    e.material = corn_m;
    scene.add_entity(std::move(e));
  }

  // Security camera + keypad + card reader
  auto* cam_body = scene.add_mesh(
      fury::make_box({0.22f, 0.16f, 0.28f}, Vec3{0.15f, 0.15f, 0.16f}));
  Material sec_m = mat_painted_metal({0.18f, 0.18f, 0.2f});
  {
    Entity e;
    e.name = "AaaMeridianSecCam";
    e.mesh = cam_body;
    e.transform.position = {33.2f, 3.6f, 6.7f};
    e.material = sec_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  auto* cam_lens = scene.add_mesh(
      fury::make_box({0.1f, 0.1f, 0.08f}, Vec3{0.05f, 0.05f, 0.06f}));
  Material lens_m = mat_glass();
  lens_m.albedo = {0.1f, 0.12f, 0.15f};
  lens_m.emissive = 0.05f;
  {
    Entity e;
    e.name = "AaaMeridianSecLens";
    e.mesh = cam_lens;
    e.transform.position = {33.2f, 3.55f, 6.85f};
    e.material = lens_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  auto* keypad = scene.add_mesh(
      fury::make_box({0.28f, 0.4f, 0.08f}, Vec3{0.2f, 0.22f, 0.25f}));
  {
    Entity e;
    e.name = "AaaMeridianKeypad";
    e.mesh = keypad;
    e.transform.position = {36.2f, 1.4f, 6.7f};
    e.material = sec_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  auto* key_led = scene.add_mesh(
      fury::make_box({0.18f, 0.06f, 0.02f}, Vec3{0.2f, 0.95f, 0.35f}));
  Material led_m;
  led_m.albedo = {0.3f, 1.0f, 0.4f};
  led_m.emissive = 0.85f;
  led_m.emissive_color = {0.3f, 1.0f, 0.4f};
  led_m.roughness = 0.9f;
  {
    Entity e;
    e.name = "AaaMeridianKeyLed";
    e.mesh = key_led;
    e.transform.position = {36.2f, 1.55f, 6.76f};
    e.material = led_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }

  // Roof HVAC + vents + pipes
  auto* hvac = scene.add_mesh(
      fury::make_box({2.8f, 1.4f, 2.2f}, Vec3{0.45f, 0.46f, 0.48f}));
  Material hvac_m = mat_painted_metal({0.55f, 0.56f, 0.58f});
  hvac_m.roughness = 0.55f;
  hvac_m.clearcoat = 0.15f;
  for (const Vec3& p : {Vec3{30.f, 11.2f, 2.f}, Vec3{40.f, 11.2f, 1.5f}}) {
    Entity e;
    e.name = "AaaMeridianHvac";
    e.mesh = hvac;
    e.transform.position = p;
    e.material = hvac_m;
    scene.add_entity(std::move(e));
  }
  auto* vent = scene.add_mesh(
      fury::make_box({0.9f, 0.35f, 0.9f}, Vec3{0.3f, 0.3f, 0.32f}));
  for (const Vec3& p : {Vec3{33.f, 10.6f, 4.f}, Vec3{37.f, 10.6f, 3.5f},
                        Vec3{42.f, 10.6f, 4.2f}}) {
    Entity e;
    e.name = "AaaMeridianVent";
    e.mesh = vent;
    e.transform.position = p;
    e.material = hvac_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  auto* pipe = scene.add_mesh(
      fury::make_box({0.18f, 3.5f, 0.18f}, Vec3{0.35f, 0.32f, 0.28f}));
  Material pipe_m = mat_painted_metal({0.42f, 0.38f, 0.32f});
  pipe_m.roughness = 0.6f;
  for (float x : {26.5f, 43.5f}) {
    Entity e;
    e.name = "AaaMeridianPipe";
    e.mesh = pipe;
    e.transform.position = {x, 8.5f, 5.8f};
    e.material = pipe_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  auto* pipe_h = scene.add_mesh(
      fury::make_box({8.f, 0.16f, 0.16f}, Vec3{0.35f, 0.32f, 0.28f}));
  {
    Entity e;
    e.name = "AaaMeridianPipeH";
    e.mesh = pipe_h;
    e.transform.position = {35.f, 9.8f, 5.8f};
    e.material = pipe_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }

  // ---- Lobby interior set dressing (visible from 01_lobby) ----
  auto* lobby_floor = scene.add_mesh(
      fury::make_plane(16.f, 10.f, Vec3{0.85f, 0.82f, 0.76f}, 6.f));
  {
    Entity e;
    e.name = "AaaLobbyFloor";
    e.mesh = lobby_floor;
    e.transform.position = {35.f, 0.08f, 2.5f};
    e.material = mat_marble();
    scene.add_entity(std::move(e));
  }
  // Floor tile accents
  auto* lobby_tile = scene.add_mesh(
      fury::make_plane(1.4f, 1.4f, Vec3{0.7f, 0.68f, 0.62f}, 1.f));
  for (int i = 0; i < 6; ++i) {
    Entity e;
    e.name = "AaaLobbyTile";
    e.mesh = lobby_tile;
    e.transform.position = {30.f + i * 1.6f, 0.09f, 4.5f};
    e.material = mat_marble();
    e.material.albedo = {0.78f + 0.04f * (i % 2), 0.76f, 0.70f};
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  // Reception desk (authored massing)
  auto* recv = scene.add_mesh(
      fury::make_box({5.5f, 1.15f, 1.4f}, Vec3{0.75f, 0.78f, 0.82f}));
  Material recv_m = mat_marble();
  recv_m.albedo = {0.88f, 0.90f, 0.93f};
  recv_m.metallic = 0.12f;
  {
    Entity e;
    e.name = "AaaLobbyReception";
    e.mesh = recv;
    e.transform.position = {35.f, 0.65f, 1.2f};
    e.material = recv_m;
    e.solid = true;
    e.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, {5.5f, 1.15f, 1.4f});
    scene.add_entity(std::move(e));
  }
  auto* recv_top = scene.add_mesh(
      fury::make_box({5.6f, 0.08f, 1.5f}, Vec3{0.55f, 0.35f, 0.22f}));
  {
    Entity e;
    e.name = "AaaLobbyReceptionTop";
    e.mesh = recv_top;
    e.transform.position = {35.f, 1.25f, 1.2f};
    e.material = mat_wood();
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  auto* recv_glass = scene.add_mesh(
      fury::make_box({5.2f, 0.55f, 0.06f}, Vec3{0.6f, 0.8f, 1.0f}));
  {
    Entity e;
    e.name = "AaaLobbyReceptionGlass";
    e.mesh = recv_glass;
    e.transform.position = {35.f, 1.55f, 1.85f};
    e.material = mat_glass();
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  // Ceiling panels + recessed lights
  auto* ceil = scene.add_mesh(
      fury::make_plane(16.f, 10.f, Vec3{0.9f, 0.9f, 0.88f}, 4.f));
  Material ceil_m = mat_concrete();
  ceil_m.albedo = {0.95f, 0.94f, 0.90f};
  ceil_m.roughness = 0.65f;
  {
    Entity e;
    e.name = "AaaLobbyCeiling";
    e.mesh = ceil;
    e.transform.position = {35.f, 4.6f, 2.5f};
    e.material = ceil_m;
    scene.add_entity(std::move(e));
  }
  auto* recess = scene.add_mesh(
      fury::make_box({0.55f, 0.08f, 0.55f}, Vec3{1.0f, 0.95f, 0.85f}));
  Material recess_m;
  recess_m.albedo = {1.0f, 0.96f, 0.88f};
  recess_m.emissive = 1.4f;
  recess_m.emissive_color = {1.0f, 0.95f, 0.85f};
  recess_m.roughness = 0.9f;
  for (float x = 29.f; x <= 41.f; x += 3.f) {
    for (float z : {0.5f, 3.5f}) {
      Entity e;
      e.name = "AaaLobbyRecessLight";
      e.mesh = recess;
      e.transform.position = {x, 4.5f, z};
      e.material = recess_m;
      e.tag = "lamp";
      e.detail = true;
      scene.add_entity(std::move(e));
    }
  }
  // Wall panel trim
  auto* wall_panel = scene.add_mesh(
      fury::make_box({14.f, 2.8f, 0.12f}, Vec3{0.65f, 0.55f, 0.42f}));
  Material panel_m = mat_wood();
  panel_m.albedo = {0.42f, 0.30f, 0.20f};
  {
    Entity e;
    e.name = "AaaLobbyWallPanel";
    e.mesh = wall_panel;
    e.transform.position = {35.f, 1.5f, -1.8f};
    e.material = panel_m;
    scene.add_entity(std::move(e));
  }
  // Rug
  auto* rug = scene.add_mesh(
      fury::make_plane(4.5f, 3.0f, Vec3{0.35f, 0.12f, 0.12f}, 1.f));
  Material rug_m;
  rug_m.albedo = {0.45f, 0.12f, 0.14f};
  rug_m.roughness = 0.92f;
  {
    Entity e;
    e.name = "AaaLobbyRug";
    e.mesh = rug;
    e.transform.position = {35.f, 0.095f, 4.2f};
    e.material = rug_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  // Waiting chairs with backs
  auto* chair_seat = scene.add_mesh(
      fury::make_box({0.65f, 0.12f, 0.65f}, Vec3{0.25f, 0.18f, 0.15f}));
  auto* chair_back = scene.add_mesh(
      fury::make_box({0.65f, 0.7f, 0.1f}, Vec3{0.25f, 0.18f, 0.15f}));
  auto* chair_leg = scene.add_mesh(
      fury::make_box({0.06f, 0.4f, 0.06f}, Vec3{0.15f, 0.15f, 0.16f}));
  Material chair_m;
  chair_m.albedo = {0.28f, 0.18f, 0.14f};
  chair_m.roughness = 0.75f;
  Material leg_m = mat_chrome();
  leg_m.albedo = {0.55f, 0.55f, 0.58f};
  for (float x : {30.5f, 31.4f, 38.6f, 39.5f}) {
    Entity seat;
    seat.name = "AaaLobbyChairSeat";
    seat.mesh = chair_seat;
    seat.transform.position = {x, 0.45f, 5.2f};
    seat.material = chair_m;
    scene.add_entity(std::move(seat));
    Entity back;
    back.name = "AaaLobbyChairBack";
    back.mesh = chair_back;
    back.transform.position = {x, 0.8f, 4.9f};
    back.material = chair_m;
    scene.add_entity(std::move(back));
    for (float dx : {-0.25f, 0.25f}) {
      for (float dz : {-0.25f, 0.25f}) {
        Entity leg;
        leg.name = "AaaLobbyChairLeg";
        leg.mesh = chair_leg;
        leg.transform.position = {x + dx, 0.22f, 5.2f + dz};
        leg.material = leg_m;
        leg.detail = true;
        scene.add_entity(std::move(leg));
      }
    }
  }
  // Potted plants
  auto* pot = scene.add_mesh(
      fury::make_box({0.45f, 0.5f, 0.45f}, Vec3{0.35f, 0.28f, 0.22f}));
  auto* foliage = scene.add_mesh(
      fury::make_box({0.55f, 0.9f, 0.55f}, Vec3{0.15f, 0.45f, 0.18f}));
  Material pot_m;
  pot_m.albedo = {0.4f, 0.3f, 0.22f};
  pot_m.roughness = 0.8f;
  Material leaf_m;
  leaf_m.albedo = {0.18f, 0.48f, 0.22f};
  leaf_m.roughness = 0.85f;
  for (const Vec3& p : {Vec3{29.f, 0.35f, 5.5f}, Vec3{41.f, 0.35f, 5.5f},
                        Vec3{29.f, 0.35f, 0.5f}, Vec3{41.f, 0.35f, 0.5f}}) {
    Entity e;
    e.name = "AaaLobbyPot";
    e.mesh = pot;
    e.transform.position = p;
    e.material = pot_m;
    scene.add_entity(std::move(e));
    Entity f;
    f.name = "AaaLobbyFoliage";
    f.mesh = foliage;
    f.transform.position = {p.x, p.y + 0.65f, p.z};
    f.material = leaf_m;
    f.detail = true;
    scene.add_entity(std::move(f));
  }
  // Wall art / poster frames
  auto* frame = scene.add_mesh(
      fury::make_box({1.4f, 1.0f, 0.06f}, Vec3{0.2f, 0.18f, 0.15f}));
  auto* art = scene.add_mesh(
      fury::make_box({1.2f, 0.8f, 0.03f}, Vec3{0.35f, 0.45f, 0.55f}));
  Material art_m;
  art_m.albedo = {0.4f, 0.5f, 0.62f};
  art_m.roughness = 0.7f;
  art_m.emissive = 0.04f;
  for (float x : {31.f, 39.f}) {
    Entity e;
    e.name = "AaaLobbyFrame";
    e.mesh = frame;
    e.transform.position = {x, 2.4f, -1.7f};
    e.material = mat_wood();
    e.detail = true;
    scene.add_entity(std::move(e));
    Entity a;
    a.name = "AaaLobbyArt";
    a.mesh = art;
    a.transform.position = {x, 2.4f, -1.65f};
    a.material = art_m;
    a.detail = true;
    scene.add_entity(std::move(a));
  }
  // Interior column
  auto* col = scene.add_mesh(
      fury::make_box({0.55f, 4.4f, 0.55f}, Vec3{0.85f, 0.84f, 0.80f}));
  auto* col_cap = scene.add_mesh(
      fury::make_box({0.7f, 0.18f, 0.7f}, Vec3{0.88f, 0.86f, 0.82f}));
  for (float x : {32.f, 38.f}) {
    Entity e;
    e.name = "AaaLobbyColumn";
    e.mesh = col;
    e.transform.position = {x, 2.3f, 2.8f};
    e.material = mat_marble();
    e.solid = true;
    e.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, {0.55f, 4.4f, 0.55f});
    scene.add_entity(std::move(e));
    Entity c;
    c.name = "AaaLobbyColCap";
    c.mesh = col_cap;
    c.transform.position = {x, 4.4f, 2.8f};
    c.material = mat_marble();
    c.detail = true;
    scene.add_entity(std::move(c));
    Entity b;
    b.name = "AaaLobbyColBase";
    b.mesh = col_cap;
    b.transform.position = {x, 0.2f, 2.8f};
    b.material = mat_marble();
    b.detail = true;
    scene.add_entity(std::move(b));
  }

  // ---- Cycle-4: denser Meridian Mutual set dressing (same footprint) ----
  // Teller counters behind reception (depth layer)
  auto* teller = scene.add_mesh(
      fury::make_box({2.2f, 1.1f, 0.7f}, Vec3{0.82f, 0.84f, 0.88f}));
  auto* teller_glass = scene.add_mesh(
      fury::make_box({2.0f, 0.7f, 0.05f}, Vec3{0.6f, 0.8f, 1.0f}));
  Material teller_m = mat_marble();
  teller_m.albedo = {0.90f, 0.91f, 0.93f};
  for (float x : {31.5f, 35.f, 38.5f}) {
    Entity e;
    e.name = "AaaTellerDesk";
    e.mesh = teller;
    e.transform.position = {x, 0.6f, -0.4f};
    e.material = teller_m;
    scene.add_entity(std::move(e));
    Entity g;
    g.name = "AaaTellerGlass";
    g.mesh = teller_glass;
    g.transform.position = {x, 1.35f, -0.05f};
    g.material = mat_glass();
    g.detail = true;
    scene.add_entity(std::move(g));
  }
  // Queue stanchions + rope
  auto* stanch = scene.add_mesh(
      fury::make_box({0.1f, 1.0f, 0.1f}, Vec3{0.7f, 0.65f, 0.55f}));
  auto* rope = scene.add_mesh(
      fury::make_box({1.6f, 0.04f, 0.04f}, Vec3{0.55f, 0.12f, 0.12f}));
  Material st_m = mat_chrome();
  st_m.albedo = {0.75f, 0.72f, 0.65f};
  Material rope_m;
  rope_m.albedo = {0.55f, 0.10f, 0.12f};
  rope_m.roughness = 0.85f;
  for (int i = 0; i < 4; ++i) {
    const float x = 32.5f + i * 1.7f;
    Entity e;
    e.name = "AaaQueueStanchion";
    e.mesh = stanch;
    e.transform.position = {x, 0.55f, 3.4f};
    e.material = st_m;
    e.detail = true;
    scene.add_entity(std::move(e));
    if (i < 3) {
      Entity r;
      r.name = "AaaQueueRope";
      r.mesh = rope;
      r.transform.position = {x + 0.85f, 0.95f, 3.4f};
      r.material = rope_m;
      r.detail = true;
      scene.add_entity(std::move(r));
    }
  }
  // Security desk + monitor
  auto* sec_desk = scene.add_mesh(
      fury::make_box({1.6f, 1.0f, 0.8f}, Vec3{0.25f, 0.26f, 0.28f}));
  auto* sec_mon = scene.add_mesh(
      fury::make_box({0.5f, 0.35f, 0.06f}, Vec3{0.2f, 0.4f, 0.3f}));
  Material sec_desk_m = mat_painted_metal({0.22f, 0.23f, 0.25f});
  Material mon_m;
  mon_m.albedo = {0.15f, 0.35f, 0.25f};
  mon_m.emissive = 0.45f;
  mon_m.emissive_color = {0.3f, 0.8f, 0.5f};
  {
    Entity e;
    e.name = "AaaSecurityDesk";
    e.mesh = sec_desk;
    e.transform.position = {41.5f, 0.55f, 3.5f};
    e.material = sec_desk_m;
    scene.add_entity(std::move(e));
    Entity m;
    m.name = "AaaSecurityMon";
    m.mesh = sec_mon;
    m.transform.position = {41.5f, 1.25f, 3.5f};
    m.material = mon_m;
    m.detail = true;
    scene.add_entity(std::move(m));
  }
  // Baseboard trim + ceiling cove
  auto* baseboard = scene.add_mesh(
      fury::make_box({14.5f, 0.12f, 0.08f}, Vec3{0.55f, 0.45f, 0.35f}));
  Material trim_m = mat_wood();
  trim_m.albedo = {0.35f, 0.25f, 0.16f};
  {
    Entity e;
    e.name = "AaaLobbyBaseboard";
    e.mesh = baseboard;
    e.transform.position = {35.f, 0.12f, -1.75f};
    e.material = trim_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  auto* cove = scene.add_mesh(
      fury::make_box({14.5f, 0.1f, 0.15f}, Vec3{0.9f, 0.9f, 0.88f}));
  {
    Entity e;
    e.name = "AaaLobbyCove";
    e.mesh = cove;
    e.transform.position = {35.f, 4.45f, -1.7f};
    e.material = mat_concrete();
    e.material.albedo = {0.92f, 0.91f, 0.88f};
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  // Ceiling coffer beams
  auto* coffer = scene.add_mesh(
      fury::make_box({14.f, 0.12f, 0.2f}, Vec3{0.85f, 0.84f, 0.80f}));
  for (float z : {0.2f, 2.5f, 4.8f}) {
    Entity e;
    e.name = "AaaLobbyCoffer";
    e.mesh = coffer;
    e.transform.position = {35.f, 4.5f, z};
    e.material = mat_marble();
    e.material.roughness = 0.5f;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  // Layered plant foliage (less boxy)
  auto* leaf2 = scene.add_mesh(
      fury::make_box({0.35f, 0.55f, 0.35f}, Vec3{0.12f, 0.42f, 0.15f}));
  auto* leaf3 = scene.add_mesh(
      fury::make_box({0.25f, 0.4f, 0.25f}, Vec3{0.20f, 0.50f, 0.18f}));
  for (const Vec3& p : {Vec3{29.f, 0.35f, 5.5f}, Vec3{41.f, 0.35f, 5.5f}}) {
    Entity f2;
    f2.name = "AaaLobbyFoliage2";
    f2.mesh = leaf2;
    f2.transform.position = {p.x + 0.15f, p.y + 0.9f, p.z + 0.1f};
    f2.material = leaf_m;
    f2.material.albedo = {0.14f, 0.40f, 0.18f};
    f2.detail = true;
    scene.add_entity(std::move(f2));
    Entity f3;
    f3.name = "AaaLobbyFoliage3";
    f3.mesh = leaf3;
    f3.transform.position = {p.x - 0.12f, p.y + 1.05f, p.z - 0.08f};
    f3.material = leaf_m;
    f3.material.albedo = {0.22f, 0.52f, 0.20f};
    f3.detail = true;
    scene.add_entity(std::move(f3));
  }
  // Floor wear strips near entrance
  auto* wear = scene.add_mesh(
      fury::make_plane(3.5f, 0.8f, Vec3{0.55f, 0.52f, 0.48f}, 1.f));
  Material wear_m = mat_marble();
  wear_m.albedo = {0.70f, 0.68f, 0.62f};
  wear_m.roughness = 0.55f;
  wear_m.clearcoat = 0.2f;
  {
    Entity e;
    e.name = "AaaLobbyWear";
    e.mesh = wear;
    e.transform.position = {35.f, 0.095f, 5.8f};
    e.material = wear_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  // Brochure stand / set dressing
  auto* stand = scene.add_mesh(
      fury::make_box({0.5f, 1.2f, 0.35f}, Vec3{0.7f, 0.72f, 0.75f}));
  {
    Entity e;
    e.name = "AaaBrochureStand";
    e.mesh = stand;
    e.transform.position = {29.5f, 0.65f, 2.5f};
    e.material = mat_painted_metal({0.75f, 0.76f, 0.78f});
    e.detail = true;
    scene.add_entity(std::move(e));
  }

  // ---- Kill blue void (Cycle-4): denser midrise ring + far skyline layer ----
  auto* bg_bldg = scene.add_mesh(
      fury::make_colored_box({12.f, 22.f, 10.f}, {0.42f, 0.44f, 0.48f},
                             {0.32f, 0.34f, 0.38f}));
  auto* bg_bldg_tall = scene.add_mesh(
      fury::make_colored_box({10.f, 34.f, 9.f}, {0.38f, 0.40f, 0.46f},
                             {0.28f, 0.30f, 0.36f}));
  auto* bg_bldg_wide = scene.add_mesh(
      fury::make_colored_box({18.f, 16.f, 12.f}, {0.44f, 0.42f, 0.40f},
                             {0.34f, 0.32f, 0.30f}));
  Material bg_m = mat_concrete();
  bg_m.albedo = {0.52f, 0.53f, 0.56f};
  bg_m.roughness = 0.72f;
  const Vec3 bg_pos[] = {
      // Near ring (terminates street cams)
      {-42.f, 11.f, -6.f}, {-42.f, 9.f, 12.f}, {-42.f, 13.f, 28.f},
      {52.f, 14.f, -4.f}, {54.f, 10.f, 14.f}, {50.f, 12.f, 30.f},
      {8.f, 16.f, -16.f}, {-18.f, 12.f, -14.f}, {28.f, 18.f, -18.f},
      {-8.f, 10.f, 36.f}, {22.f, 14.f, 38.f}, {40.f, 11.f, 36.f},
      {-28.f, 15.f, 34.f}, {12.f, 8.f, -20.f}, {-5.f, 20.f, -22.f},
      // Far skyline layer (atmospheric termination)
      {-68.f, 22.f, 0.f}, {-70.f, 18.f, 22.f}, {72.f, 26.f, 4.f},
      {74.f, 20.f, 28.f}, {0.f, 28.f, -38.f}, {35.f, 24.f, -40.f},
      {-30.f, 22.f, -36.f}, {15.f, 16.f, 52.f}, {-20.f, 18.f, 50.f},
  };
  int bgi = 0;
  for (const Vec3& bp : bg_pos) {
    Entity e;
    e.name = "AaaBgFill";
    const bool tall = (bgi % 5 == 0);
    const bool wide = (bgi % 5 == 2);
    e.mesh = tall ? bg_bldg_tall : (wide ? bg_bldg_wide : bg_bldg);
    e.transform.position = bp;
    e.transform.scale = {0.75f + 0.18f * (bgi % 3), 0.65f + 0.22f * (bgi % 4),
                         0.85f + 0.1f * (bgi % 2)};
    e.material = bg_m;
    e.material.albedo = {0.40f + 0.06f * (bgi % 4), 0.41f + 0.04f * (bgi % 3),
                         0.45f + 0.05f * (bgi % 5)};
    // Far buildings cooler / flatter (atmospheric)
    if (std::fabs(bp.x) > 60.f || bp.z < -30.f || bp.z > 45.f) {
      e.material.albedo = {0.48f, 0.50f, 0.56f};
      e.material.roughness = 0.85f;
    }
    scene.add_entity(std::move(e));
    ++bgi;
  }
  // Rooftop bulkheads on near ring (silhouette variety)
  auto* roof_bulk = scene.add_mesh(
      fury::make_box({3.5f, 2.2f, 3.0f}, Vec3{0.35f, 0.36f, 0.38f}));
  for (const Vec3& rp : {Vec3{-42.f, 23.f, -6.f}, Vec3{52.f, 26.f, -4.f},
                         Vec3{8.f, 28.f, -16.f}, Vec3{28.f, 30.f, -18.f}}) {
    Entity e;
    e.name = "AaaBgRoof";
    e.mesh = roof_bulk;
    e.transform.position = rp;
    e.material = bg_m;
    e.material.albedo = {0.32f, 0.33f, 0.36f};
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  // Distant window emissive strips on bg (night readable — denser)
  auto* bg_win = scene.add_mesh(
      fury::make_box({0.75f, 0.85f, 0.08f}, Vec3{0.8f, 0.75f, 0.55f}));
  Material bgw;
  bgw.albedo = {0.9f, 0.85f, 0.6f};
  bgw.emissive = 0.55f;
  bgw.emissive_color = {1.0f, 0.9f, 0.65f};
  bgw.roughness = 0.9f;
  bgw.texture = TextureSlot::Glass;
  for (const Vec3& bp :
       {Vec3{-42.f, 8.f, -0.5f}, Vec3{52.f, 10.f, 1.5f}, Vec3{8.f, 12.f, -10.5f},
        Vec3{28.f, 14.f, -12.5f}, Vec3{-42.f, 7.f, 17.5f}, Vec3{50.f, 9.f, 25.5f},
        Vec3{-8.f, 8.f, 30.5f}, Vec3{22.f, 11.f, 32.5f}}) {
    for (int row = 0; row < 5; ++row) {
      for (int col = 0; col < 4; ++col) {
        if ((row + col + static_cast<int>(bp.x)) % 5 == 0) continue;  // sparse dark
        Entity e;
        e.name = "AaaBgWin";
        e.mesh = bg_win;
        e.transform.position = {bp.x - 3.5f + col * 2.0f, bp.y + row * 2.3f,
                                bp.z};
        e.material = bgw;
        e.material.emissive = 0.35f + 0.25f * ((row + col) % 3 == 0 ? 1.f : 0.f);
        e.detail = true;
        scene.add_entity(std::move(e));
      }
    }
  }
  // Atmospheric sky/fog WALLS (vertical boxes — kill remaining blue wedge)
  auto* fog_ns = scene.add_mesh(
      fury::make_box({200.f, 70.f, 4.f}, Vec3{0.55f, 0.58f, 0.64f}));
  auto* fog_ew = scene.add_mesh(
      fury::make_box({4.f, 70.f, 200.f}, Vec3{0.55f, 0.58f, 0.64f}));
  Material fog_m;
  fog_m.albedo = {0.62f, 0.64f, 0.68f};
  fog_m.roughness = 1.f;
  fog_m.emissive = 0.12f;
  fog_m.emissive_color = {0.58f, 0.60f, 0.66f};
  // North / South walls
  for (const Vec3& fp : {Vec3{5.f, 28.f, -58.f}, Vec3{5.f, 24.f, 68.f}}) {
    Entity e;
    e.name = "AaaFogWallNS";
    e.mesh = fog_ns;
    e.transform.position = fp;
    e.material = fog_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  // East / West walls
  for (const Vec3& fp : {Vec3{-85.f, 26.f, 10.f}, Vec3{90.f, 26.f, 10.f}}) {
    Entity e;
    e.name = "AaaFogWallEW";
    e.mesh = fog_ew;
    e.transform.position = fp;
    e.material = fog_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  // Extra mid-distance filler towers covering street/cruiser/ped camera wedges
  auto* filler = scene.add_mesh(
      fury::make_colored_box({14.f, 28.f, 12.f}, {0.40f, 0.42f, 0.48f},
                             {0.30f, 0.32f, 0.38f}));
  const Vec3 fill_pos[] = {
      // Covers 02_street looking -X from (10,3.4,24): need geometry west/north
      {-30.f, 14.f, 22.f}, {-35.f, 18.f, 8.f}, {-25.f, 12.f, 32.f},
      // Covers 03_cruiser yaw~-2.6 from (24,1.9,17.5): SW wedge
      {5.f, 16.f, 30.f}, {15.f, 20.f, -8.f}, {35.f, 14.f, -10.f},
      // Covers 04_peds yaw~-0.3 from (6,1.8,16): NE sky wedge
      {30.f, 18.f, 8.f}, {38.f, 22.f, 22.f}, {20.f, 15.f, -5.f},
      {45.f, 12.f, 12.f}, {-15.f, 20.f, -5.f}, {0.f, 24.f, -28.f},
  };
  int fi = 0;
  for (const Vec3& bp : fill_pos) {
    Entity e;
    e.name = "AaaSkyFiller";
    e.mesh = filler;
    e.transform.position = bp;
    e.transform.scale = {0.7f + 0.2f * (fi % 3), 0.8f + 0.25f * (fi % 4), 0.85f};
    e.material = bg_m;
    e.material.albedo = {0.42f + 0.05f * (fi % 4), 0.44f + 0.03f * (fi % 3),
                         0.50f + 0.04f * (fi % 5)};
    e.material.roughness = 0.8f;
    scene.add_entity(std::move(e));
    ++fi;
  }

  // Street lamp posts with emissive heads (night hierarchy)
  auto* lamp_pole = scene.add_mesh(
      fury::make_box({0.12f, 4.5f, 0.12f}, Vec3{0.25f, 0.25f, 0.26f}));
  auto* lamp_head = scene.add_mesh(
      fury::make_box({0.55f, 0.2f, 0.55f}, Vec3{1.0f, 0.92f, 0.7f}));
  Material pole_m = mat_painted_metal({0.3f, 0.3f, 0.32f});
  Material lamp_m;
  lamp_m.albedo = {1.0f, 0.92f, 0.75f};
  lamp_m.emissive = 1.8f;
  lamp_m.emissive_color = {1.0f, 0.92f, 0.7f};
  lamp_m.roughness = 0.85f;
  for (const Vec3& lp : {Vec3{-8.f, 2.3f, 8.5f}, Vec3{12.f, 2.3f, 8.5f},
                         Vec3{26.f, 2.3f, 8.5f}, Vec3{8.f, 2.3f, 19.5f},
                         Vec3{22.f, 2.3f, 19.5f}}) {
    Entity p;
    p.name = "AaaStreetLampPole";
    p.mesh = lamp_pole;
    p.transform.position = lp;
    p.material = pole_m;
    scene.add_entity(std::move(p));
    Entity h;
    h.name = "AaaStreetLampHead";
    h.tag = "lamp";
    h.mesh = lamp_head;
    h.transform.position = {lp.x, lp.y + 2.35f, lp.z};
    h.material = lamp_m;
    h.detail = true;
    scene.add_entity(std::move(h));
  }

  // Cycle-5: large wet patches that survive capture distance (visible SSR/env)
  {
    auto* wet_pl = scene.add_mesh(
        fury::make_plane(3.8f, 2.4f, Vec3{0.10f, 0.10f, 0.11f}, 2.f));
    Material wet_m = mat_asphalt();
    wet_m.albedo = {0.48f, 0.50f, 0.54f};
    wet_m.roughness = 0.10f;
    wet_m.metallic = 0.18f;
    wet_m.wetness = 0.98f;
    wet_m.clearcoat = 0.75f;
    // Cycle-6: larger wet mirrors under hero cams (02/03/05)
    const Vec3 wet_spots[] = {
        {4.5f, 0.018f, 14.2f}, {12.f, 0.018f, 15.5f}, {22.f, 0.018f, 13.8f},
        {8.f, 0.018f, 12.5f}, {18.5f, 0.018f, 16.2f}, {26.f, 0.018f, 15.0f},
        {23.0f, 0.018f, 14.5f}, {10.5f, 0.018f, 22.5f}, {20.f, 0.018f, 20.5f},
        {6.5f, 0.018f, 15.8f}};
    int wi = 0;
    for (const Vec3& wp : wet_spots) {
      Entity e;
      e.name = "AaaWetPatch" + std::to_string(wi++);
      e.tag = "asphalt";
      e.mesh = wet_pl;
      e.transform.position = wp;
      e.transform.scale = {1.15f + 0.25f * (wi % 3), 1.f, 1.05f + 0.2f * (wi % 2)};
      e.material = wet_m;
      e.detail = true;
      scene.add_entity(std::move(e));
    }
  }
  // Cycle-5: lamp light pools on pavement (expensive night chain)
  {
    auto* pool = scene.add_mesh(
        fury::make_plane(2.8f, 2.8f, Vec3{1.f, 0.92f, 0.75f}, 1.f));
    Material pool_m;
    pool_m.albedo = {1.0f, 0.92f, 0.78f};
    pool_m.roughness = 0.55f;
    pool_m.metallic = 0.05f;
    pool_m.wetness = 0.65f;
    pool_m.emissive = 0.55f;
    pool_m.emissive_color = {1.0f, 0.90f, 0.70f};
    const Vec3 pools[] = {
        {12.f, 0.019f, 8.5f}, {26.f, 0.019f, 8.5f}, {22.f, 0.019f, 19.5f},
        {8.f, 0.019f, 19.5f}, {18.f, 0.019f, 14.0f}};
    int pi = 0;
    for (const Vec3& pp : pools) {
      Entity e;
      e.name = "AaaLampPool" + std::to_string(pi++);
      e.mesh = pool;
      e.transform.position = pp;
      e.material = pool_m;
      e.detail = true;
      scene.add_entity(std::move(e));
    }
  }
  // Cycle-5: visible aggregate / repair patches (raised slightly)
  {
    auto* chip = scene.add_mesh(fury::make_box({0.18f, 0.02f, 0.14f}, {0.32f, 0.32f, 0.30f}));
    Material chip_m = mat_asphalt();
    chip_m.albedo = {0.95f, 0.92f, 0.88f};
    chip_m.roughness = 0.9f;
    chip_m.wetness = 0.2f;
    for (int i = 0; i < 28; ++i) {
      const float fx = 2.f + (i % 6) * 4.2f + (i % 3) * 0.3f;
      const float fz = 12.5f + (i / 6) * 2.8f + (i % 2) * 0.4f;
      Entity e;
      e.name = "AaaAggregate" + std::to_string(i);
      e.mesh = chip;
      e.transform.position = {fx, 0.015f, fz};
      e.transform.rotation_euler = {0.f, i * 0.7f, 0.f};
      e.material = chip_m;
      e.detail = true;
      scene.add_entity(std::move(e));
    }
  }

  // Cycle-4 street physicality: tire wear, extra drains, curb grit
  auto* tire_mark = scene.add_mesh(
      fury::make_plane(8.f, 0.35f, Vec3{0.08f, 0.08f, 0.09f}, 1.f));
  Material tire_m = mat_asphalt();
  tire_m.albedo = {0.18f, 0.18f, 0.19f};
  tire_m.roughness = 0.95f;
  tire_m.wetness = 0.35f;
  for (const Vec3& tp : {Vec3{2.f, 0.012f, 13.2f}, Vec3{10.f, 0.012f, 14.8f},
                         Vec3{18.f, 0.012f, 13.5f}, Vec3{22.5f, 0.012f, 14.2f},
                         Vec3{8.f, 0.012f, 22.0f}, Vec3{14.f, 0.012f, 21.5f}}) {
    Entity e;
    e.name = "AaaTireWear";
    e.mesh = tire_mark;
    e.transform.position = tp;
    e.material = tire_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  auto* drain2 = scene.add_mesh(
      fury::make_box({0.7f, 0.05f, 0.35f}, Vec3{0.2f, 0.2f, 0.22f}));
  Material drain_m = mat_painted_metal({0.25f, 0.25f, 0.26f});
  drain_m.roughness = 0.7f;
  for (const Vec3& dp : {Vec3{-2.f, 0.05f, 10.8f}, Vec3{16.f, 0.05f, 10.8f},
                         Vec3{28.f, 0.05f, 10.8f}, Vec3{8.f, 0.05f, 18.5f}}) {
    Entity e;
    e.name = "AaaDrainGrille";
    e.mesh = drain2;
    e.transform.position = dp;
    e.material = drain_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
  auto* curb_grit = scene.add_mesh(
      fury::make_plane(6.f, 0.4f, Vec3{0.35f, 0.32f, 0.28f}, 1.f));
  Material grit_m = mat_concrete();
  grit_m.albedo = {0.45f, 0.40f, 0.34f};
  grit_m.roughness = 0.92f;
  for (float x = -8.f; x <= 30.f; x += 10.f) {
    Entity e;
    e.name = "AaaCurbGrit";
    e.mesh = curb_grit;
    e.transform.position = {x, 0.04f, 10.6f};
    e.material = grit_m;
    e.detail = true;
    scene.add_entity(std::move(e));
  }

  // Parked civilian car silhouettes (density / storytelling — not HMPD)
  auto* civ_car = scene.add_mesh(
      fury::make_colored_box({4.4f, 1.4f, 1.9f}, {0.55f, 0.18f, 0.14f},
                             {0.40f, 0.12f, 0.10f}));
  Material car_m = mat_painted_metal({0.55f, 0.12f, 0.10f});
  car_m.clearcoat = 0.9f;
  car_m.roughness = 0.22f;
  for (const Vec3& cp : {Vec3{-6.f, 0.7f, 17.5f}, Vec3{32.f, 0.7f, 17.8f}}) {
    Entity e;
    e.name = "AaaCivParked";
    e.mesh = civ_car;
    e.transform.position = cp;
    e.material = car_m;
    e.solid = true;
    e.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, {4.4f, 1.4f, 1.9f});
    scene.add_entity(std::move(e));
    add_contact_blob(scene, blob_plane, "AaaCivCarShadow", cp, 4.6f, 2.1f);
  }
  auto* civ_car2 = scene.add_mesh(
      fury::make_colored_box({4.2f, 1.5f, 1.85f}, {0.15f, 0.22f, 0.35f},
                             {0.10f, 0.15f, 0.25f}));
  Material car2_m = mat_painted_metal({0.12f, 0.20f, 0.38f});
  {
    Entity e;
    e.name = "AaaCivParkedB";
    e.mesh = civ_car2;
    e.transform.position = {-14.f, 0.75f, 12.5f};
    e.transform.rotation_euler = {0.f, 1.5708f, 0.f};
    e.material = car2_m;
    scene.add_entity(std::move(e));
  }

  fury::Log::info(
      "AAA Meridian block Cycle-5: AAA peds+visible reflect+clean render+expensive night "
      "lighting+dense Meridian Mutual+skyline ring (Harbor Metro / HMPD)");
}

/// Hide far-district clutter so soft capture focuses on the Meridian block.
inline void hide_outside_aaa_block(fury::Scene& scene) {
  for (Entity& e : scene.entities()) {
    const Vec3& p = e.transform.position;
    // Keep AAA-tagged / Meridian-local content
    if (e.name.rfind("Aaa", 0) == 0) continue;
    if (e.tag == "bank" || e.tag == "hmpd_cruiser" || e.tag == "asphalt" ||
        e.tag == "contact_shadow")
      continue;
    if (e.name.rfind("AaaPed", 0) == 0 || e.name.rfind("NpcCiv", 0) == 0)
      continue;
    if (e.name.rfind("Bank", 0) == 0 || e.name.rfind("Teller", 0) == 0 ||
        e.name.rfind("Lobby", 0) == 0 || e.name.rfind("ATM", 0) == 0 ||
        e.name.rfind("Vault", 0) == 0 || e.name.rfind("Column", 0) == 0 ||
        e.name.rfind("Queue", 0) == 0)
      continue;
    // Drop everything outside the controlled block AABB
    if (p.x < -60.f || p.x > 70.f || p.z < -20.f || p.z > 40.f) {
      e.visible = false;
    }
  }
}

/// Place 5 authored Harbor Metro pedestrians (clothing/hair/shoes/skin MTL splits).
inline void spawn_aaa_pedestrians(
    fury::Scene& scene,
    const std::function<void(fury::NpcAgent, const Vec3&)>& /*spawn_npc*/) {
  struct Spec {
    const char* file;
    const char* entity;
    Vec3 pos;
    float yaw;
  };
  const Spec specs[] = {
      // Cycle-4: conversation cluster (Suki↔Noah) + walk/idle/lean variety
      // Camera 04 at (6,1.8,16) yaw=-0.3 looks toward +X across the group
      // Cycle-5: tighter cluster nearer camera for facial/hand readability
      {"harbor_metro/peds/hm_ped_rae.obj", "AaaPedA", {7.6f, 0.f, 15.4f}, 0.20f},   // walking in
      {"harbor_metro/peds/hm_ped_dane.obj", "AaaPedB", {9.4f, 0.f, 14.9f}, -0.35f}, // idle watch
      {"harbor_metro/peds/hm_ped_suki.obj", "AaaPedC", {8.4f, 0.f, 16.5f}, 0.55f},  // converse → Noah
      {"harbor_metro/peds/hm_ped_noah.obj", "AaaPedD", {9.1f, 0.f, 17.0f}, -2.35f}, // converse → Suki
      {"harbor_metro/peds/hm_ped_ivy.obj", "AaaPedE", {10.6f, 0.f, 15.8f}, 0.85f},  // lean wait
  };
  auto* blob = scene.add_mesh(
      fury::make_plane(1.f, 1.f, Vec3{0.05f, 0.05f, 0.05f}, 1.f));
  for (const Spec& s : specs) {
    fury::Transform xf;
    xf.position = s.pos;
    xf.rotation_euler = {0.f, s.yaw, 0.f};
    const int parts =
        spawn_obj_mtl(scene, s.file, s.file, s.entity, xf, "aaa_ped");
    if (parts == 0) {
      // Fallback humanoid if ped OBJ missing
      Entity e;
      e.name = s.entity;
      e.tag = "aaa_ped";
      e.mesh = scene.add_mesh(fury::make_humanoid(1.7f, {0.55f, 0.45f, 0.35f}));
      e.transform = xf;
      e.material.albedo = {1.f, 1.f, 1.f};
      scene.add_entity(std::move(e));
    }
    add_contact_blob(scene, blob, (std::string(s.entity) + "Shadow").c_str(),
                     s.pos, 0.7f, 0.55f);
  }
  fury::Log::info("AAA Cycle-6 pedestrians placed (5 Blender higher-poly AAA characters, multi-pose)");
}

struct CaptureShot {
  const char* file_stem;
  Vec3 position;
  float yaw;
  float pitch;
  bool night;
  const char* note;
};

inline const CaptureShot* capture_shots(int& count) {
  static const CaptureShot kShots[] = {
      // yaw: 0→+X, -π/2→-Z (toward kit façades / Meridian Mutual annex)
      {"01_lobby", {35.f, 1.85f, 12.5f}, -1.5708f, -0.05f, false,
       "Meridian Mutual entrance + lobby glimpse"},
      {"02_street", {10.f, 3.2f, 23.5f}, -1.5708f, -0.22f, false,
       "Intersection wet asphalt reflections + road→curb→sidewalk to Meridian"},
      {"03_cruiser", {23.0f, 1.45f, 16.5f}, -2.52f, -0.10f, false,
       "Hero HMPD cruiser — hood/doors/windshield + wet asphalt env reflections"},
      {"04_peds", {7.55f, 1.42f, 15.15f}, 0.15f, -0.06f, false,
       "Five Cycle-6 Blender AAA Harbor Metro characters — close crop for faces/hands"},
      {"05_night_or_alt", {19.5f, 2.25f, 20.8f}, -1.85f, -0.14f, true,
       "Expensive night: lamp→wet asphalt→cruiser→glass→façade bounce→ped rim→haze"},
  };
  count = static_cast<int>(sizeof(kShots) / sizeof(kShots[0]));
  return kShots;
}

inline bool write_ppm(const std::string& path, const std::vector<std::uint8_t>& rgb,
                      int w, int h) {
  std::FILE* fp = std::fopen(path.c_str(), "wb");
  if (!fp) return false;
  std::fprintf(fp, "P6\n%d %d\n255\n", w, h);
  std::fwrite(rgb.data(), 1,
              static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u, fp);
  std::fclose(fp);
  return true;
}

inline bool ensure_dir(const std::string& path) {
  // Portable-ish: rely on POSIX mkdir via system for nested path.
  const std::string cmd = "mkdir -p \"" + path + "\"";
  return std::system(cmd.c_str()) == 0;
}

}  // namespace aaa_meridian
