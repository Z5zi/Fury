#pragma once
// AAA Meridian Mutual benchmark block — Harbor Metro / HMPD / Meridian Mutual only.
// Cycle-2: multi-mat OBJ/MTL, authored peds, directional soft shadows, street treatment.
// Implements ChatGPT Top 10 runtime visual fixes for one controlled street block.

#include <fury/fury.hpp>

#include <algorithm>
#include <cmath>
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
  m.albedo = {0.55f, 0.55f, 0.58f};
  m.roughness = 0.92f;
  m.metallic = 0.02f;
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
  m.albedo = {0.55f, 0.72f, 0.95f};
  m.roughness = 0.12f;
  m.metallic = 0.08f;
  m.emissive = 0.05f;
  m.texture = TextureSlot::Glass;
  return m;
}
inline Material mat_painted_metal(const Vec3& rgb) {
  Material m;
  m.albedo = rgb;
  m.roughness = 0.38f;
  m.metallic = 0.72f;
  m.texture = TextureSlot::Metal;
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
      fury::make_plane(90.f, 70.f, Vec3{0.12f, 0.12f, 0.13f}, 20.f));
  {
    Entity e;
    e.name = "AaaBaseFill";
    e.tag = "asphalt";
    e.mesh = base_fill;
    e.transform.position = {5.f, 0.005f, 14.f};
    e.material = mat_asphalt();
    e.material.albedo = {0.45f, 0.45f, 0.48f};
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
  patch_m.wetness = 0.08f;
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
  stain_m.wetness = 0.45f;
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
  cruiser_fb.roughness = 0.32f;
  cruiser_fb.albedo = {0.12f, 0.22f, 0.55f};
  const int cruiser_parts = spawn_obj_mtl(
      scene, "harbor_metro/hmpd_cruiser_v12b_soft.obj",
      "harbor_metro/hmpd_cruiser_v12b.obj", "AaaHmpdCruiser", cruiser_xf,
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

  fury::Log::info(
      "AAA Meridian block: annex+storefront+midrise+HMPD cruiser+street+"
      "props (Harbor Metro / HMPD / Meridian Mutual)");
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
      {"harbor_metro/peds/hm_ped_rae.obj", "AaaPedA", {10.f, 0.f, 12.f}, 0.4f},
      {"harbor_metro/peds/hm_ped_dane.obj", "AaaPedB", {22.f, 0.f, 16.f}, -1.2f},
      {"harbor_metro/peds/hm_ped_suki.obj", "AaaPedC", {-8.f, 0.f, 15.f}, 2.5f},
      {"harbor_metro/peds/hm_ped_noah.obj", "AaaPedD", {16.f, 0.f, 9.f}, 0.1f},
      {"harbor_metro/peds/hm_ped_ivy.obj", "AaaPedE", {28.f, 0.f, 13.f}, -0.8f},
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
  fury::Log::info("AAA authored pedestrians placed (5 multi-mat silhouettes)");
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
      {"02_street", {10.f, 3.4f, 24.f}, -1.5708f, -0.20f, false,
       "Intersection road→curb→sidewalk looking north to Meridian"},
      {"03_cruiser", {24.f, 1.9f, 17.5f}, -2.6f, -0.10f, false,
       "Hero HMPD cruiser v12b grounded on asphalt"},
      {"04_peds", {6.f, 1.8f, 16.f}, -0.3f, -0.05f, false,
       "Improved pedestrians on Meridian block"},
      {"05_night_or_alt", {20.f, 2.6f, 22.f}, -1.9f, -0.14f, true,
       "Alt/night lighting hierarchy on same block"},
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
