#include <fury/fury.hpp>

#include <SDL.h>

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {

using fury::Aabb;
using fury::Color;
using fury::Entity;
using fury::Material;
using fury::TextureSlot;
using fury::Transform;
using fury::Vec3;

constexpr int kSaveSlotCount = 3;
constexpr float kShopRadius = 6.5f;
const Vec3 kAshcourtShopPos{-86.f, 0.f, 48.f};
constexpr float kSafehouseEnterRadius = 5.8f;
/// Harbor loft safehouse (waterfront north of extraction).
const Vec3 kHarborLoftPos{42.f, 0.f, 52.f};
/// Loft workbench interact point (craft UI with G).
const Vec3 kLoftWorkbenchPos{44.2f, 0.f, 53.2f};
constexpr float kWorkbenchRadius = 4.2f;

struct BuyMenu {
  bool open{false};
  /// Selected loot chip to sell at the fence (0=BearerBond, 1=Sapphire, 2=LedgerDrive).
  int sell_selected{0};
};

struct InventoryPanel {
  bool open{false};
};

struct RepPanel {
  bool open{false};
};

struct HelpPanel {
  bool open{false};
};

struct MapPanel {
  bool open{false};
  int focus{0};  // 0..5 district index
};

constexpr int kDistrictCount = 6;
constexpr int kFastTravelCost = 250;
constexpr float kFastTravelCooldown = 45.f;

struct DistrictInfo {
  const char* name;
  Vec3 center;       // xz plane (y unused)
  Vec3 half_extents; // map rect size in world xz
  Vec3 hub;          // fast-travel landing (eye height)
  Color fill;
};

inline const DistrictInfo& district_info(int index) {
  static const DistrictInfo kDistricts[kDistrictCount] = {
      {"Harbor Metro", {0.f, 0.f, 0.f}, {36.f, 0.f, 28.f}, {0.f, 1.7f, 12.f},
       Color{70, 120, 200, 180}},
      {"Ridge Pier", {95.f, 0.f, 8.f}, {28.f, 0.f, 22.f}, {92.f, 1.7f, 8.f},
       Color{60, 180, 190, 180}},
      {"Ashcourt Market", {-88.f, 0.f, 42.f}, {26.f, 0.f, 22.f}, {-86.f, 1.7f, 48.f},
       Color{200, 130, 70, 180}},
      {"Harbor Depot", {58.f, 0.f, -48.f}, {20.f, 0.f, 18.f}, {58.f, 1.7f, -40.f},
       Color{150, 110, 200, 180}},
      {"Harbor Loft", {42.f, 0.f, 52.f}, {14.f, 0.f, 12.f}, {42.f, 1.7f, 54.f},
       Color{90, 220, 180, 180}},
      {"North Quay", {10.f, 0.f, 96.f}, {30.f, 0.f, 24.f}, {18.f, 1.7f, 88.f},
       Color{180, 160, 90, 180}},
  };
  return kDistricts[index % kDistrictCount];
}

/// Local vehicle offset: +X forward (matches Camera yaw=0), +Z right.
Vec3 vehicle_local_offset(float yaw, float lx, float ly, float lz) {
  const float c = std::cos(yaw);
  const float s = std::sin(yaw);
  return {lx * c - lz * s, ly, lx * s + lz * c};
}

constexpr const char* kRadioStations[3] = {
    "Harbor Wave FM",
    "Ashcourt Night",
    "Pierline Pulse",
};

enum class DriveKind { Van, CivSedan };

struct DriveableSlot {
  Vec3 pos{};
  float yaw{0.f};
  DriveKind kind{DriveKind::Van};
  const char* label{"vehicle"};
};


void add_solid_box(fury::Scene& scene, fury::Mesh* mesh, const char* name,
                   const Vec3& pos, const Vec3& size, Material mat,
                   const std::string& tag = {}, bool detail = false) {
  Entity e;
  e.name = name;
  e.mesh = mesh;
  e.transform.position = pos;
  e.material = mat;
  e.solid = true;
  e.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, size);
  e.tag = tag;
  e.detail = detail;
  scene.add_entity(std::move(e));
}

void add_prop(fury::Scene& scene, fury::Mesh* mesh, const char* name, const Vec3& pos,
              Material mat, bool solid = false, const Vec3& solid_size = {},
              bool detail = false, fury::Mesh* lod_mesh = nullptr) {
  Entity e;
  e.name = name;
  e.mesh = mesh;
  e.transform.position = pos;
  e.material = mat;
  e.detail = detail;
  e.lod_mesh = lod_mesh;
  if (solid) {
    e.solid = true;
    e.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, solid_size);
  }
  scene.add_entity(std::move(e));
}



/// Flat dark/reflective street puddles — visible when wet (Harbor + Ashcourt).
void place_street_puddles(fury::Scene& scene) {
  struct Spec {
    const char* name;
    float w, d;
    Vec3 pos;
  };
  const Spec specs[] = {
      // Harbor Metro asphalt
      {"PuddleHarborA", 4.2f, 2.6f, {8.f, 0.055f, 6.f}},
      {"PuddleHarborB", 3.4f, 2.2f, {-18.f, 0.055f, -12.f}},
      {"PuddleHarborC", 5.0f, 2.0f, {22.f, 0.055f, 18.f}},
      {"PuddleHarborD", 2.8f, 3.0f, {-6.f, 0.055f, 24.f}},
      // Ashcourt Market road / plaza edge
      {"PuddleAshA", 3.6f, 2.4f, {-70.f, 0.055f, 30.f}},
      {"PuddleAshB", 4.0f, 2.0f, {-88.f, 0.055f, 48.f}},
      {"PuddleAshC", 2.6f, 2.8f, {-96.f, 0.055f, 36.f}},
  };
  Material mat;
  mat.albedo = {0.08f, 0.10f, 0.14f};
  mat.roughness = 0.12f;
  mat.metallic = 0.72f;
  mat.wetness = 0.f;
  mat.texture = TextureSlot::Glass;
  mat.emissive = 0.02f;
  for (const Spec& s : specs) {
    auto* mesh = scene.add_mesh(fury::make_plane(s.w, s.d, Vec3{0.1f, 0.12f, 0.16f}, 1.f));
    Entity e;
    e.name = s.name;
    e.tag = "puddle";
    e.mesh = mesh;
    e.transform.position = s.pos;
    e.material = mat;
    e.visible = false;
    e.detail = true;
    scene.add_entity(std::move(e));
  }
}

void place_security_camera(fury::Scene& scene, fury::Mesh* body, fury::Mesh* lens,
                           const char* body_name, const char* lens_name,
                           const Vec3& pos, float yaw) {
  Material hous;
  hous.albedo = {0.18f, 0.20f, 0.24f};
  hous.metallic = 0.65f;
  hous.roughness = 0.4f;
  Material lens_m;
  lens_m.albedo = {0.35f, 0.85f, 1.1f};
  lens_m.emissive = 1.6f;
  lens_m.roughness = 0.25f;
  add_prop(scene, body, body_name, pos, hous);
  const float fx = std::cos(yaw);
  const float fz = std::sin(yaw);
  add_prop(scene, lens, lens_name,
           {pos.x + fx * 0.28f, pos.y - 0.05f, pos.z + fz * 0.28f}, lens_m);
}

void place_breaker_box(fury::Scene& scene, fury::Mesh* box, const char* name,
                       const Vec3& pos) {
  Material panel;
  panel.albedo = {0.75f, 0.72f, 0.28f};
  panel.emissive = 0.45f;
  panel.metallic = 0.4f;
  panel.roughness = 0.55f;
  add_prop(scene, box, name, pos, panel, true, {0.7f, 1.2f, 0.35f});
}

void place_lamp(fury::Scene& scene, fury::Mesh* pole, fury::Mesh* lamp_head, float x,
                float z) {
  Material dark;
  dark.albedo = {0.55f, 0.55f, 0.58f};
  dark.metallic = 0.75f;
  dark.roughness = 0.35f;
  Material glow;
  glow.albedo = {1.0f, 0.92f, 0.55f};
  glow.roughness = 0.95f;
  glow.emissive = 2.4f;  // base; DayNightCycle scales via tag "lamp"
  add_prop(scene, pole, "LampPole", {x, 2.2f, z}, dark);
  Entity head;
  head.name = "LampHead";
  head.tag = "lamp";
  head.mesh = lamp_head;
  head.transform.position = {x, 4.5f, z};
  head.material = glow;
  scene.add_entity(std::move(head));
}

/// 2.1.0 denser world — mid-block props, static parked cars, neon, rooftop AC.
void add_parked_car(fury::Scene& scene, fury::Mesh* body, fury::Mesh* cabin,
                    const Vec3& pos, const Vec3& body_rgb, float yaw_deg = 0.f) {
  Material body_mat;
  body_mat.albedo = body_rgb;
  body_mat.metallic = 0.55f;
  body_mat.roughness = 0.42f;
  body_mat.texture = TextureSlot::Metal;
  Material cabin_mat;
  cabin_mat.albedo = {0.35f, 0.55f, 0.70f};
  cabin_mat.metallic = 0.15f;
  cabin_mat.roughness = 0.22f;
  cabin_mat.emissive = 0.06f;
  cabin_mat.texture = TextureSlot::Glass;
  Entity car;
  car.name = "ParkedCar";
  car.tag = "prop";
  car.mesh = body;
  car.transform.position = pos;
  car.transform.rotation_euler = {0.f, yaw_deg * 0.01745329252f, 0.f};
  car.material = body_mat;
  car.solid = true;
  car.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, {4.2f, 1.5f, 1.9f});
  scene.add_entity(std::move(car));
  Entity cab;
  cab.name = "ParkedCarCabin";
  cab.tag = "prop";
  cab.detail = true;  // 2.8.0 LOD — skip cabin beyond mid range
  cab.mesh = cabin;
  cab.transform.position = {pos.x, pos.y + 0.55f, pos.z};
  cab.transform.rotation_euler = {0.f, yaw_deg * 0.01745329252f, 0.f};
  cab.material = cabin_mat;
  scene.add_entity(std::move(cab));
}

void add_neon_sign(fury::Scene& scene, fury::Mesh* board, const Vec3& pos,
                   const Vec3& rgb, float emissive = 1.4f) {
  Material neon;
  neon.albedo = rgb;
  neon.emissive = emissive;
  neon.roughness = 0.9f;
  add_prop(scene, board, "NeonSign", pos, neon, false, {}, true);
}

void add_rooftop_ac(fury::Scene& scene, fury::Mesh* box, float x, float roof_y,
                    float z) {
  Material ac;
  ac.albedo = {0.55f, 0.58f, 0.62f};
  ac.metallic = 0.65f;
  ac.roughness = 0.4f;
  ac.texture = TextureSlot::Metal;
  add_prop(scene, box, "RooftopAC", {x, roof_y + 0.55f, z}, ac, false, {}, true);
  // Small vent fan stub on top
  Material fan;
  fan.albedo = {0.25f, 0.26f, 0.28f};
  fan.metallic = 0.7f;
  fan.roughness = 0.35f;
  Entity e;
  e.name = "RooftopACFan";
  e.detail = true;
  e.mesh = box;
  e.transform.position = {x, roof_y + 1.05f, z};
  e.transform.scale = {0.45f, 0.25f, 0.45f};
  e.material = fan;
  scene.add_entity(std::move(e));
}

void add_midblock_fill(fury::Scene& scene, fury::Mesh* crate, fury::Mesh* trash,
                       fury::Mesh* hydrant, const Vec3& pos) {
  Material crate_mat;
  crate_mat.roughness = 0.78f;
  crate_mat.albedo = {1.0f, 0.92f, 0.78f};
  crate_mat.texture = TextureSlot::Checker;
  Material trash_mat;
  trash_mat.metallic = 0.5f;
  trash_mat.roughness = 0.45f;
  Material hyd;
  hyd.albedo = {0.75f, 0.12f, 0.14f};
  hyd.metallic = 0.35f;
  hyd.roughness = 0.4f;
  add_solid_box(scene, crate, "MidCrate", {pos.x, 0.55f, pos.z},
                {1.0f, 1.0f, 1.0f}, crate_mat, "detail", true);
  add_solid_box(scene, trash, "MidTrash", {pos.x + 1.4f, 0.55f, pos.z + 0.3f},
                {0.65f, 1.05f, 0.65f}, trash_mat, "detail", true);
  add_solid_box(scene, hydrant, "Hydrant", {pos.x - 1.2f, 0.45f, pos.z - 0.4f},
                {0.4f, 0.9f, 0.4f}, hyd, "detail", true);
}

void build_harbor_density(fury::Scene& scene) {
  auto* car_body = scene.add_mesh(
      fury::make_box({4.2f, 1.5f, 1.9f}, Vec3{0.55f, 0.18f, 0.16f}));
  auto* car_cabin = scene.add_mesh(
      fury::make_box({1.8f, 0.85f, 1.7f}, Vec3{0.30f, 0.50f, 0.65f}));
  auto* neon_board = scene.add_mesh(
      fury::make_box({3.2f, 1.1f, 0.18f}, Vec3{1.0f, 0.3f, 0.5f}));
  auto* ac_box = scene.add_mesh(
      fury::make_box({1.8f, 1.1f, 1.4f}, Vec3{0.55f, 0.58f, 0.62f}));
  auto* crate = scene.add_mesh(
      fury::make_box({1.0f, 1.0f, 1.0f}, Vec3{0.55f, 0.42f, 0.28f}));
  auto* trash = scene.add_mesh(
      fury::make_box({0.65f, 1.05f, 0.65f}, Vec3{0.25f, 0.28f, 0.22f}));
  auto* hydrant = scene.add_mesh(
      fury::make_box({0.4f, 0.9f, 0.4f}, Vec3{0.75f, 0.12f, 0.14f}));

  // Parked cars filling empty Harbor stretches (static — not driveable)
  const struct { Vec3 p; Vec3 rgb; float yaw; } cars[] = {
      {{-24.f, 0.75f, 8.f}, {0.55f, 0.18f, 0.16f}, 90.f},
      {{-24.f, 0.75f, -8.f}, {0.15f, 0.22f, 0.45f}, 90.f},
      {{24.f, 0.75f, 10.f}, {0.72f, 0.72f, 0.70f}, -90.f},
      {{24.f, 0.75f, -12.f}, {0.12f, 0.12f, 0.14f}, -90.f},
      {{-8.f, 0.75f, -24.f}, {0.20f, 0.45f, 0.28f}, 0.f},
      {{10.f, 0.75f, 24.f}, {0.45f, 0.35f, 0.15f}, 180.f},
      {{48.f, 0.75f, 4.f}, {0.65f, 0.25f, 0.20f}, 90.f},  // toward bridge
      {{52.f, 0.75f, -14.f}, {0.18f, 0.28f, 0.40f}, 90.f},
      {{-48.f, 0.75f, 10.f}, {0.80f, 0.78f, 0.72f}, -90.f},  // toward Ashcourt
      {{-52.f, 0.75f, 22.f}, {0.22f, 0.24f, 0.28f}, 0.f},
      {{6.f, 0.75f, -40.f}, {0.40f, 0.12f, 0.18f}, 90.f},
      {{-32.f, 0.75f, 32.f}, {0.30f, 0.50f, 0.55f}, 180.f},
  };
  for (const auto& c : cars) {
    add_parked_car(scene, car_body, car_cabin, c.p, c.rgb, c.yaw);
  }

  // Neon signs on mid-block facades / empty stretches
  add_neon_sign(scene, neon_board, {-38.f, 4.2f, -0.5f}, {1.0f, 0.25f, 0.55f});
  add_neon_sign(scene, neon_board, {22.f, 5.5f, -0.2f}, {0.25f, 0.85f, 1.0f}, 1.6f);
  add_neon_sign(scene, neon_board, {-20.f, 4.0f, 26.5f}, {1.0f, 0.75f, 0.20f}, 1.3f);
  add_neon_sign(scene, neon_board, {40.f, 5.0f, 22.5f}, {0.45f, 1.0f, 0.40f});
  add_neon_sign(scene, neon_board, {55.f, 4.8f, 36.5f}, {1.0f, 0.35f, 0.20f}, 1.5f);
  add_neon_sign(scene, neon_board, {-50.f, 4.5f, 8.5f}, {0.70f, 0.40f, 1.0f});

  // Rooftop AC boxes on Harbor buildings (roof_y = building height)
  const struct { float x, h, z; } roofs[] = {
      {-38.f, 7.f, -6.f}, {22.f, 14.f, -6.f}, {40.f, 11.f, -10.f},
      {-20.f, 8.f, 22.f}, {18.f, 6.f, 20.f}, {-40.f, 10.f, 16.f},
      {38.f, 13.f, 18.f}, {-22.f, 7.f, -32.f}, {20.f, 9.f, -34.f},
      {0.f, 5.f, 36.f}, {-36.f, 12.f, -30.f}, {48.f, 8.f, 6.f},
      {-50.f, 9.f, 4.f}, {52.f, 15.f, -28.f}, {30.f, 9.f, 48.f},
      {-30.f, 11.f, 40.f}, {55.f, 10.f, 32.f}, {-58.f, 16.f, 22.f},
      {62.f, 18.f, -8.f}, {14.f, 20.f, 58.f},
  };
  for (const auto& r : roofs) {
    add_rooftop_ac(scene, ac_box, r.x + 1.2f, r.h, r.z - 1.0f);
    if (static_cast<int>(r.h) % 2 == 0) {
      add_rooftop_ac(scene, ac_box, r.x - 1.5f, r.h, r.z + 1.2f);
    }
  }

  // Mid-block props along empty street corridors
  const Vec3 mid_pts[] = {
      {-16.f, 0.f, 0.f}, {16.f, 0.f, 0.f}, {0.f, 0.f, 16.f}, {0.f, 0.f, -22.f},
      {-34.f, 0.f, 6.f}, {34.f, 0.f, -4.f}, {-28.f, 0.f, -18.f}, {28.f, 0.f, 14.f},
      {46.f, 0.f, 12.f}, {-46.f, 0.f, -6.f}, {-56.f, 0.f, 28.f}, {58.f, 0.f, 8.f},
      {8.f, 0.f, 44.f}, {-12.f, 0.f, -48.f}, {42.f, 0.f, -22.f},
  };
  for (const Vec3& m : mid_pts) {
    add_midblock_fill(scene, crate, trash, hydrant, m);
  }
}

void build_ridge_density(fury::Scene& scene) {
  const float ox = 95.f;
  const float oz = 8.f;
  auto* car_body = scene.add_mesh(
      fury::make_box({4.2f, 1.5f, 1.9f}, Vec3{0.40f, 0.42f, 0.48f}));
  auto* car_cabin = scene.add_mesh(
      fury::make_box({1.8f, 0.85f, 1.7f}, Vec3{0.30f, 0.50f, 0.65f}));
  auto* neon_board = scene.add_mesh(
      fury::make_box({3.6f, 1.2f, 0.2f}, Vec3{0.2f, 0.9f, 1.0f}));
  auto* ac_box = scene.add_mesh(
      fury::make_box({1.8f, 1.1f, 1.4f}, Vec3{0.55f, 0.58f, 0.62f}));
  auto* crate = scene.add_mesh(
      fury::make_box({1.0f, 1.0f, 1.0f}, Vec3{0.50f, 0.40f, 0.28f}));
  auto* trash = scene.add_mesh(
      fury::make_box({0.65f, 1.05f, 0.65f}, Vec3{0.25f, 0.28f, 0.22f}));
  auto* hydrant = scene.add_mesh(
      fury::make_box({0.4f, 0.9f, 0.4f}, Vec3{0.75f, 0.12f, 0.14f}));

  // Bridge approach + plaza parked cars
  add_parked_car(scene, car_body, car_cabin, {78.f, 0.75f, 3.f},
                 {0.35f, 0.38f, 0.42f}, 0.f);
  add_parked_car(scene, car_body, car_cabin, {82.f, 0.75f, 10.f},
                 {0.70f, 0.20f, 0.18f}, 180.f);
  add_parked_car(scene, car_body, car_cabin, {ox - 6.f, 0.75f, oz + 4.f},
                 {0.15f, 0.35f, 0.55f}, 90.f);
  add_parked_car(scene, car_body, car_cabin, {ox + 8.f, 0.75f, oz - 14.f},
                 {0.55f, 0.55f, 0.50f}, 0.f);
  add_parked_car(scene, car_body, car_cabin, {ox + 16.f, 0.75f, oz + 8.f},
                 {0.25f, 0.45f, 0.30f}, -90.f);

  add_neon_sign(scene, neon_board, {ox - 14.f, 5.0f, oz - 5.f},
                {0.30f, 0.95f, 1.0f}, 1.5f);
  add_neon_sign(scene, neon_board, {ox + 12.f, 6.0f, oz - 2.5f},
                {1.0f, 0.45f, 0.20f}, 1.4f);
  add_neon_sign(scene, neon_board, {ox + 10.f, 4.2f, oz + 16.5f},
                {0.85f, 1.0f, 0.35f});

  const struct { float x, h, z; } roofs[] = {
      {ox - 14.f, 8.f, oz - 10.f}, {ox + 12.f, 10.f, oz - 8.f},
      {ox + 10.f, 6.f, oz + 12.f}, {ox - 12.f, 7.f, oz + 12.f},
      {ox + 22.f, 12.f, oz + 2.f},
  };
  for (const auto& r : roofs) {
    add_rooftop_ac(scene, ac_box, r.x + 1.0f, r.h, r.z);
  }

  const Vec3 mid_pts[] = {
      {66.f, 0.f, 4.f}, {74.f, 0.f, 8.f}, {ox, 0.f, oz - 4.f},
      {ox + 6.f, 0.f, oz + 6.f}, {ox - 8.f, 0.f, oz + 8.f},
      {ox + 14.f, 0.f, oz + 18.f},
  };
  for (const Vec3& m : mid_pts) {
    add_midblock_fill(scene, crate, trash, hydrant, m);
  }
}

void build_ashcourt_density(fury::Scene& scene) {
  const float ox = -88.f;
  const float oz = 42.f;
  auto* car_body = scene.add_mesh(
      fury::make_box({4.2f, 1.5f, 1.9f}, Vec3{0.50f, 0.40f, 0.22f}));
  auto* car_cabin = scene.add_mesh(
      fury::make_box({1.8f, 0.85f, 1.7f}, Vec3{0.30f, 0.50f, 0.65f}));
  auto* neon_board = scene.add_mesh(
      fury::make_box({3.4f, 1.15f, 0.18f}, Vec3{0.4f, 1.0f, 0.5f}));
  auto* ac_box = scene.add_mesh(
      fury::make_box({1.8f, 1.1f, 1.4f}, Vec3{0.55f, 0.58f, 0.62f}));
  auto* crate = scene.add_mesh(
      fury::make_box({1.0f, 1.0f, 1.0f}, Vec3{0.55f, 0.42f, 0.28f}));
  auto* trash = scene.add_mesh(
      fury::make_box({0.65f, 1.05f, 0.65f}, Vec3{0.25f, 0.28f, 0.22f}));
  auto* hydrant = scene.add_mesh(
      fury::make_box({0.4f, 0.9f, 0.4f}, Vec3{0.75f, 0.12f, 0.14f}));

  // Connector road + market edge parked cars
  add_parked_car(scene, car_body, car_cabin, {-70.f, 0.75f, 26.f},
                 {0.55f, 0.30f, 0.18f}, 0.f);
  add_parked_car(scene, car_body, car_cabin, {-58.f, 0.75f, 32.f},
                 {0.20f, 0.22f, 0.28f}, 180.f);
  add_parked_car(scene, car_body, car_cabin, {ox + 4.f, 0.75f, oz - 18.f},
                 {0.65f, 0.65f, 0.60f}, 90.f);
  add_parked_car(scene, car_body, car_cabin, {ox - 16.f, 0.75f, oz + 2.f},
                 {0.18f, 0.40f, 0.55f}, 0.f);
  add_parked_car(scene, car_body, car_cabin, {ox + 18.f, 0.75f, oz + 14.f},
                 {0.45f, 0.15f, 0.20f}, -90.f);

  add_neon_sign(scene, neon_board, {ox - 12.f, 4.0f, oz - 3.5f},
                {1.0f, 0.55f, 0.20f}, 1.5f);
  add_neon_sign(scene, neon_board, {ox + 10.f, 4.5f, oz - 5.f},
                {0.40f, 1.0f, 0.70f}, 1.6f);
  add_neon_sign(scene, neon_board, {ox + 12.f, 3.8f, oz + 14.5f},
                {1.0f, 0.30f, 0.55f});
  add_neon_sign(scene, neon_board, {ox + 20.f, 5.0f, oz + 6.f},
                {0.85f, 0.90f, 0.25f}, 1.3f);

  const struct { float x, h, z; } roofs[] = {
      {ox - 12.f, 6.f, oz - 8.f}, {ox + 10.f, 7.f, oz - 10.f},
      {ox + 12.f, 5.5f, oz + 10.f}, {ox - 10.f, 6.5f, oz + 12.f},
      {ox + 20.f, 8.f, oz + 2.f},
  };
  for (const auto& r : roofs) {
    add_rooftop_ac(scene, ac_box, r.x, r.h, r.z + 0.8f);
  }

  const Vec3 mid_pts[] = {
      {-74.f, 0.f, 28.f}, {-66.f, 0.f, 30.f}, {ox, 0.f, oz},
      {ox + 6.f, 0.f, oz + 8.f}, {ox - 6.f, 0.f, oz - 6.f},
      {ox + 14.f, 0.f, oz - 4.f}, {ox - 4.f, 0.f, oz + 16.f},
  };
  for (const Vec3& m : mid_pts) {
    add_midblock_fill(scene, crate, trash, hydrant, m);
  }
}

void build_meridian_mutual(fury::Scene& scene) {
  const float bank_cx = 0.f;
  const float bank_cz = -10.f;
  const float wall_h = 8.f;
  const float wall_t = 1.0f;
  const float bank_w = 18.f;
  const float bank_d = 14.f;

  Material bank_stone;
  bank_stone.albedo = {0.85f, 0.88f, 0.92f};
  bank_stone.roughness = 0.55f;
  bank_stone.metallic = 0.05f;
  bank_stone.texture = TextureSlot::Concrete;

  Material bank_dark;
  bank_dark.albedo = {0.35f, 0.38f, 0.45f};
  bank_dark.roughness = 0.5f;
  bank_dark.texture = TextureSlot::Concrete;

  auto* wall_n = scene.add_mesh(
      fury::make_colored_box({bank_w, wall_h, wall_t}, bank_stone.albedo,
                             bank_dark.albedo));
  auto* wall_w = scene.add_mesh(
      fury::make_colored_box({wall_t, wall_h, bank_d}, bank_stone.albedo,
                             bank_dark.albedo));
  auto* wall_e = scene.add_mesh(
      fury::make_colored_box({wall_t, wall_h, bank_d}, bank_stone.albedo,
                             bank_dark.albedo));
  auto* wall_s_l = scene.add_mesh(
      fury::make_colored_box({6.5f, wall_h, wall_t}, bank_stone.albedo,
                             bank_dark.albedo));
  auto* wall_s_r = scene.add_mesh(
      fury::make_colored_box({6.5f, wall_h, wall_t}, bank_stone.albedo,
                             bank_dark.albedo));

  add_solid_box(scene, wall_n, "BankWallN",
                {bank_cx, wall_h * 0.5f, bank_cz - bank_d * 0.5f},
                {bank_w, wall_h, wall_t}, bank_stone, "bank");
  add_solid_box(scene, wall_w, "BankWallW",
                {bank_cx - bank_w * 0.5f, wall_h * 0.5f, bank_cz},
                {wall_t, wall_h, bank_d}, bank_stone);
  add_solid_box(scene, wall_e, "BankWallE",
                {bank_cx + bank_w * 0.5f, wall_h * 0.5f, bank_cz},
                {wall_t, wall_h, bank_d}, bank_stone);
  add_solid_box(scene, wall_s_l, "BankWallSL",
                {bank_cx - 5.75f, wall_h * 0.5f, bank_cz + bank_d * 0.5f},
                {6.5f, wall_h, wall_t}, bank_stone);
  add_solid_box(scene, wall_s_r, "BankWallSR",
                {bank_cx + 5.75f, wall_h * 0.5f, bank_cz + bank_d * 0.5f},
                {6.5f, wall_h, wall_t}, bank_stone);

  auto* roof = scene.add_mesh(
      fury::make_box({bank_w + 0.5f, 0.6f, bank_d + 0.5f},
                     Vec3{0.55f, 0.58f, 0.62f}));
  {
    Entity r;
    r.name = "BankRoof";
    r.mesh = roof;
    r.transform.position = {bank_cx, wall_h + 0.2f, bank_cz};
    r.material.roughness = 0.7f;
    r.material.metallic = 0.15f;
    scene.add_entity(std::move(r));
  }

  auto* int_floor = scene.add_mesh(
      fury::make_plane(bank_w - 1.5f, bank_d - 1.5f, Vec3{0.55f, 0.52f, 0.45f},
                       4.f));
  {
    Entity f;
    f.name = "BankFloor";
    f.mesh = int_floor;
    f.transform.position = {bank_cx, 0.08f, bank_cz};
    f.material.texture = TextureSlot::Checker;
    f.material.roughness = 0.6f;
    scene.add_entity(std::move(f));
  }

  auto* col = scene.add_mesh(
      fury::make_box({1.1f, 6.5f, 1.1f}, Vec3{0.88f, 0.88f, 0.90f}));
  Material col_mat;
  col_mat.albedo = {1.f, 1.f, 1.f};
  col_mat.roughness = 0.4f;
  col_mat.metallic = 0.1f;
  for (float x : {-4.f, -1.4f, 1.4f, 4.f}) {
    add_solid_box(scene, col, "Column", {x, 3.25f, bank_cz + bank_d * 0.5f - 0.2f},
                  {1.1f, 6.5f, 1.1f}, col_mat);
  }

  Material part_mat = bank_dark;
  auto* part_l = scene.add_mesh(
      fury::make_box({4.5f, 5.5f, 0.8f}, Vec3{0.38f, 0.40f, 0.46f}));
  auto* part_r = scene.add_mesh(
      fury::make_box({4.5f, 5.5f, 0.8f}, Vec3{0.38f, 0.40f, 0.46f}));
  add_solid_box(scene, part_l, "VaultPartitionL",
                {bank_cx - 4.0f, 2.75f, bank_cz - 2.5f}, {4.5f, 5.5f, 0.8f},
                part_mat);
  add_solid_box(scene, part_r, "VaultPartitionR",
                {bank_cx + 4.0f, 2.75f, bank_cz - 2.5f}, {4.5f, 5.5f, 0.8f},
                part_mat);

  auto* vault_mesh = scene.add_mesh(
      fury::make_box({3.2f, 2.8f, 2.2f}, Vec3{0.95f, 0.72f, 0.18f}));
  Material vault_mat;
  vault_mat.albedo = {1.35f, 1.05f, 0.42f};
  vault_mat.metallic = 0.98f;
  vault_mat.roughness = 0.16f;
  vault_mat.emissive = 0.35f;
  {
    Entity vault;
    vault.name = "VaultDoor";
    vault.tag = "vault";
    vault.mesh = vault_mesh;
    vault.transform.position = {bank_cx, 1.4f, bank_cz - 5.2f};
    vault.material = vault_mat;
    vault.solid = true;
    vault.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, {3.2f, 2.8f, 2.2f});
    scene.add_entity(std::move(vault));
  }


  // Alarm beacon (siren visual — flashes when heat high during loot)
  auto* siren_mesh = scene.add_mesh(
      fury::make_box({0.55f, 0.35f, 0.55f}, Vec3{0.95f, 0.15f, 0.12f}));
  {
    Entity s;
    s.name = "MeridianSiren";
    s.tag = "siren";
    s.mesh = siren_mesh;
    s.transform.position = {bank_cx, wall_h + 0.4f, bank_cz};
    s.material.albedo = {1.0f, 0.2f, 0.15f};
    s.material.emissive = 0.2f;
    s.material.roughness = 0.85f;
    scene.add_entity(std::move(s));
  }

  // Teller desks (more interior props)
  auto* desk = scene.add_mesh(
      fury::make_box({4.2f, 1.15f, 1.3f}, Vec3{0.22f, 0.25f, 0.30f}));
  Material desk_mat;
  desk_mat.metallic = 0.45f;
  desk_mat.roughness = 0.4f;
  desk_mat.albedo = {0.9f, 0.92f, 0.95f};
  add_solid_box(scene, desk, "TellerDesk1", {-5.2f, 0.58f, bank_cz + 2.2f},
                {4.2f, 1.15f, 1.3f}, desk_mat);
  add_solid_box(scene, desk, "TellerDesk2", {5.2f, 0.58f, bank_cz + 2.2f},
                {4.2f, 1.15f, 1.3f}, desk_mat);
  add_solid_box(scene, desk, "TellerDesk3", {0.f, 0.58f, bank_cz + 3.6f},
                {4.2f, 1.15f, 1.3f}, desk_mat);

  auto* desk_top = scene.add_mesh(
      fury::make_box({4.0f, 0.12f, 0.35f}, Vec3{0.15f, 0.16f, 0.18f}));
  Material glass_mat;
  glass_mat.metallic = 0.15f;
  glass_mat.roughness = 0.12f;
  glass_mat.albedo = {0.65f, 0.82f, 1.05f};
  glass_mat.emissive = 0.12f;
  glass_mat.texture = TextureSlot::Glass;
  add_prop(scene, desk_top, "TellerGlass1", {-5.2f, 1.35f, bank_cz + 1.7f},
           glass_mat);
  add_prop(scene, desk_top, "TellerGlass2", {5.2f, 1.35f, bank_cz + 1.7f},
           glass_mat);

  // ATMs
  auto* atm = scene.add_mesh(
      fury::make_box({1.1f, 1.8f, 0.55f}, Vec3{0.12f, 0.14f, 0.18f}));
  Material atm_mat;
  atm_mat.metallic = 0.72f;
  atm_mat.roughness = 0.32f;
  atm_mat.albedo = {0.9f, 0.92f, 0.98f};
  atm_mat.texture = TextureSlot::Metal;
  add_solid_box(scene, atm, "ATM1", {-7.2f, 0.9f, bank_cz + 5.4f},
                {1.1f, 1.8f, 0.55f}, atm_mat);
  add_solid_box(scene, atm, "ATM2", {-5.8f, 0.9f, bank_cz + 5.4f},
                {1.1f, 1.8f, 0.55f}, atm_mat);

  auto* atm_screen = scene.add_mesh(
      fury::make_box({0.7f, 0.45f, 0.06f}, Vec3{0.2f, 0.85f, 0.55f}));
  Material screen;
  screen.albedo = {0.35f, 1.0f, 0.6f};
  screen.emissive = 1.8f;
  screen.roughness = 0.9f;
  add_prop(scene, atm_screen, "ATMScreen1", {-7.2f, 1.35f, bank_cz + 5.15f},
           screen);
  add_prop(scene, atm_screen, "ATMScreen2", {-5.8f, 1.35f, bank_cz + 5.15f},
           screen);

  // Waiting chairs / rope queue stubs (denser lobby)
  auto* chair = scene.add_mesh(
      fury::make_box({0.7f, 0.85f, 0.7f}, Vec3{0.35f, 0.22f, 0.18f}));
  Material chair_mat;
  chair_mat.roughness = 0.7f;
  for (float x = -3.5f; x <= 3.5f; x += 1.15f) {
    add_prop(scene, chair, "LobbyChair", {x, 0.42f, bank_cz + 5.0f}, chair_mat,
             true, {0.7f, 0.85f, 0.7f});
  }
  for (float x = -3.0f; x <= 3.0f; x += 1.5f) {
    add_prop(scene, chair, "LobbyChairRow2", {x, 0.42f, bank_cz + 3.9f},
             chair_mat, true, {0.7f, 0.85f, 0.7f});
  }

  auto* plant = scene.add_mesh(
      fury::make_box({0.6f, 1.4f, 0.6f}, Vec3{0.18f, 0.45f, 0.22f}));
  Material plant_mat;
  plant_mat.roughness = 0.85f;
  plant_mat.albedo = {0.7f, 1.0f, 0.7f};
  add_prop(scene, plant, "LobbyPlant", {7.2f, 0.7f, bank_cz + 5.2f}, plant_mat);
  add_prop(scene, plant, "LobbyPlant2", {-7.5f, 0.7f, bank_cz - 0.5f}, plant_mat);
  add_prop(scene, plant, "LobbyPlant3", {7.0f, 0.7f, bank_cz - 1.0f}, plant_mat);
  add_prop(scene, plant, "LobbyPlant4", {-6.8f, 0.7f, bank_cz + 4.0f}, plant_mat);

  // Rope stanchions / queue guides
  auto* stanchion = scene.add_mesh(
      fury::make_box({0.22f, 1.05f, 0.22f}, Vec3{0.75f, 0.72f, 0.55f}));
  Material brass;
  brass.metallic = 0.85f;
  brass.roughness = 0.3f;
  brass.albedo = {1.1f, 0.95f, 0.55f};
  for (float x = -2.8f; x <= 2.8f; x += 1.4f) {
    add_prop(scene, stanchion, "QueuePost", {x, 0.52f, bank_cz + 4.35f}, brass,
             true, {0.22f, 1.05f, 0.22f});
  }

  // Info kiosk + brochure rack
  auto* kiosk = scene.add_mesh(
      fury::make_box({1.4f, 1.6f, 0.7f}, Vec3{0.30f, 0.34f, 0.40f}));
  Material kiosk_mat;
  kiosk_mat.roughness = 0.45f;
  kiosk_mat.metallic = 0.25f;
  add_solid_box(scene, kiosk, "LobbyKiosk", {6.5f, 0.8f, bank_cz + 0.5f},
                {1.4f, 1.6f, 0.7f}, kiosk_mat);
  auto* rack = scene.add_mesh(
      fury::make_box({1.1f, 1.2f, 0.35f}, Vec3{0.55f, 0.40f, 0.28f}));
  add_prop(scene, rack, "BrochureRack", {-6.6f, 0.6f, bank_cz + 1.0f}, chair_mat,
           true, {1.1f, 1.2f, 0.35f});

  // Interior ceiling lamps (2.5.0 lighting zones feed from these positions)
  {
    auto* ceil_lamp = scene.add_mesh(
        fury::make_box({1.4f, 0.22f, 1.4f}, Vec3{0.95f, 0.92f, 0.75f}));
    Material ceil_mat;
    ceil_mat.albedo = {1.f, 0.95f, 0.78f};
    ceil_mat.emissive = 1.5f;
    ceil_mat.roughness = 0.9f;
    Entity e;
    e.name = "BankCeilLampL";
    e.tag = "lamp";
    e.mesh = ceil_lamp;
    e.transform.position = {bank_cx - 4.f, wall_h - 0.8f, bank_cz + 1.5f};
    e.material = ceil_mat;
    scene.add_entity(std::move(e));
    Entity e2;
    e2.name = "BankCeilLampR";
    e2.tag = "lamp";
    e2.mesh = ceil_lamp;
    e2.transform.position = {bank_cx + 4.f, wall_h - 0.8f, bank_cz + 1.5f};
    e2.material = ceil_mat;
    scene.add_entity(std::move(e2));
  }

  // Clearer bank doorway: frame pillars + threshold mat + lintel
  auto* door_post = scene.add_mesh(
      fury::make_box({0.55f, 4.2f, 0.55f}, Vec3{0.92f, 0.90f, 0.86f}));
  Material frame_mat;
  frame_mat.albedo = {1.05f, 1.02f, 0.95f};
  frame_mat.roughness = 0.4f;
  frame_mat.metallic = 0.08f;
  const float door_z = bank_cz + bank_d * 0.5f;
  add_solid_box(scene, door_post, "BankDoorPostL", {-2.35f, 2.1f, door_z},
                {0.55f, 4.2f, 0.55f}, frame_mat);
  add_solid_box(scene, door_post, "BankDoorPostR", {2.35f, 2.1f, door_z},
                {0.55f, 4.2f, 0.55f}, frame_mat);
  auto* lintel = scene.add_mesh(
      fury::make_box({5.4f, 0.55f, 0.7f}, Vec3{0.88f, 0.86f, 0.82f}));
  add_prop(scene, lintel, "BankDoorLintel", {0.f, 4.35f, door_z}, frame_mat);
  auto* threshold = scene.add_mesh(
      fury::make_box({4.6f, 0.12f, 1.4f}, Vec3{0.25f, 0.22f, 0.20f}));
  Material thresh_mat;
  thresh_mat.albedo = {0.55f, 0.48f, 0.40f};
  thresh_mat.roughness = 0.7f;
  thresh_mat.emissive = 0.12f;
  add_prop(scene, threshold, "BankThreshold", {0.f, 0.08f, door_z + 0.35f},
           thresh_mat);

  // 3.5.0 security cameras + breaker (site 0 = Meridian Mutual)
  {
    auto* cam_body = scene.add_mesh(
        fury::make_box({0.35f, 0.28f, 0.45f}, Vec3{0.15f, 0.16f, 0.18f}));
    auto* cam_lens = scene.add_mesh(
        fury::make_box({0.16f, 0.16f, 0.16f}, Vec3{0.3f, 0.8f, 1.0f}));
    auto* brk = scene.add_mesh(
        fury::make_box({0.7f, 1.2f, 0.35f}, Vec3{0.7f, 0.68f, 0.25f}));
    // Lobby corners facing inward / toward vault approach
    place_security_camera(scene, cam_body, cam_lens, "BankCamL", "BankCamLLens",
                          {bank_cx - 7.2f, 3.4f, bank_cz + 5.0f}, -0.35f);
    place_security_camera(scene, cam_body, cam_lens, "BankCamR", "BankCamRLens",
                          {bank_cx + 7.2f, 3.4f, bank_cz + 5.0f}, 3.49f);
    place_security_camera(scene, cam_body, cam_lens, "BankCamVault", "BankCamVaultLens",
                          {bank_cx, 3.6f, bank_cz - 3.8f}, 1.5708f);
    place_breaker_box(scene, brk, "BankBreaker",
                      {bank_cx - 8.2f, 1.1f, bank_cz + 0.5f});
  }
}

void build_crown_cutler(fury::Scene& scene) {
  // Second heist target stub: Crown & Cutler jewelry front (east block).
  const float cx = -22.f;
  const float cz = 8.f;
  const float w = 12.f;
  const float d = 10.f;
  const float h = 6.5f;

  Material stone;
  stone.albedo = {0.72f, 0.68f, 0.62f};
  stone.roughness = 0.55f;
  stone.texture = TextureSlot::Concrete;

  Material dark;
  dark.albedo = {0.28f, 0.26f, 0.30f};
  dark.roughness = 0.45f;

  auto* wall_n = scene.add_mesh(
      fury::make_colored_box({w, h, 0.8f}, stone.albedo, dark.albedo));
  auto* wall_w = scene.add_mesh(
      fury::make_colored_box({0.8f, h, d}, stone.albedo, dark.albedo));
  auto* wall_e = scene.add_mesh(
      fury::make_colored_box({0.8f, h, d}, stone.albedo, dark.albedo));
  auto* wall_s_l = scene.add_mesh(
      fury::make_colored_box({4.2f, h, 0.8f}, stone.albedo, dark.albedo));
  auto* wall_s_r = scene.add_mesh(
      fury::make_colored_box({4.2f, h, 0.8f}, stone.albedo, dark.albedo));

  add_solid_box(scene, wall_n, "JewelWallN", {cx, h * 0.5f, cz - d * 0.5f},
                {w, h, 0.8f}, stone, "jewelry");
  add_solid_box(scene, wall_w, "JewelWallW", {cx - w * 0.5f, h * 0.5f, cz},
                {0.8f, h, d}, stone, "jewelry");
  add_solid_box(scene, wall_e, "JewelWallE", {cx + w * 0.5f, h * 0.5f, cz},
                {0.8f, h, d}, stone, "jewelry");
  add_solid_box(scene, wall_s_l, "JewelWallSL",
                {cx - 3.6f, h * 0.5f, cz + d * 0.5f}, {4.2f, h, 0.8f}, stone);
  add_solid_box(scene, wall_s_r, "JewelWallSR",
                {cx + 3.6f, h * 0.5f, cz + d * 0.5f}, {4.2f, h, 0.8f}, stone);

  auto* roof = scene.add_mesh(
      fury::make_box({w + 0.4f, 0.45f, d + 0.4f}, Vec3{0.45f, 0.42f, 0.40f}));
  add_prop(scene, roof, "JewelRoof", {cx, h + 0.15f, cz}, stone);

  auto* floor = scene.add_mesh(
      fury::make_plane(w - 1.2f, d - 1.2f, Vec3{0.35f, 0.22f, 0.22f}, 3.f));
  {
    Entity f;
    f.name = "JewelFloor";
    f.mesh = floor;
    f.transform.position = {cx, 0.07f, cz};
    f.material.texture = TextureSlot::Checker;
    f.material.albedo = {1.1f, 0.85f, 0.85f};
    f.material.roughness = 0.5f;
    scene.add_entity(std::move(f));
  }

  // Storefront awning / neon stub
  auto* awning = scene.add_mesh(
      fury::make_box({8.f, 0.25f, 1.6f}, Vec3{0.55f, 0.12f, 0.18f}));
  Material neon;
  neon.albedo = {1.0f, 0.35f, 0.45f};
  neon.emissive = 1.6f;
  neon.roughness = 0.9f;
  add_prop(scene, awning, "JewelAwning", {cx, 3.6f, cz + d * 0.5f + 0.6f}, neon);

  // Display cases (heist target stub)
  auto* case_mesh = scene.add_mesh(
      fury::make_box({2.4f, 1.1f, 1.1f}, Vec3{0.85f, 0.88f, 0.95f}));
  Material case_mat;
  case_mat.metallic = 0.55f;
  case_mat.roughness = 0.22f;
  case_mat.albedo = {1.05f, 1.05f, 1.1f};
  case_mat.emissive = 0.25f;
  add_solid_box(scene, case_mesh, "DisplayCaseA", {cx - 2.5f, 0.55f, cz - 1.5f},
                {2.4f, 1.1f, 1.1f}, case_mat);
  add_solid_box(scene, case_mesh, "DisplayCaseB", {cx + 2.5f, 0.55f, cz - 1.5f},
                {2.4f, 1.1f, 1.1f}, case_mat);

  auto* jewel_target = scene.add_mesh(
      fury::make_box({1.6f, 1.4f, 1.6f}, Vec3{0.95f, 0.75f, 0.25f}));
  Material jt;
  jt.albedo = {1.4f, 1.08f, 0.38f};
  jt.metallic = 0.98f;
  jt.roughness = 0.14f;
  jt.emissive = 0.55f;
  {
    Entity t;
    t.name = "JewelSafe";
    t.tag = "vault_alt";
    t.mesh = jewel_target;
    t.transform.position = {cx, 0.7f, cz - 3.2f};
    t.material = jt;
    t.solid = true;
    t.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, {1.6f, 1.4f, 1.6f});
    scene.add_entity(std::move(t));
  }

  auto* counter = scene.add_mesh(
      fury::make_box({5.5f, 1.0f, 1.2f}, Vec3{0.25f, 0.18f, 0.14f}));
  Material wood;
  wood.roughness = 0.65f;
  wood.albedo = {1.f, 0.9f, 0.8f};
  add_solid_box(scene, counter, "JewelCounter", {cx, 0.5f, cz + 1.5f},
                {5.5f, 1.0f, 1.2f}, wood);

  // Enterable interior polish: wall shelves, pedestal, carpet, ceiling lamp
  auto* shelf = scene.add_mesh(
      fury::make_box({3.2f, 2.2f, 0.45f}, Vec3{0.40f, 0.32f, 0.28f}));
  Material shelf_mat;
  shelf_mat.roughness = 0.55f;
  shelf_mat.albedo = {0.95f, 0.85f, 0.75f};
  add_solid_box(scene, shelf, "JewelShelfL", {cx - 4.6f, 1.4f, cz - 0.5f},
                {3.2f, 2.2f, 0.45f}, shelf_mat);
  add_solid_box(scene, shelf, "JewelShelfR", {cx + 4.6f, 1.4f, cz - 0.5f},
                {3.2f, 2.2f, 0.45f}, shelf_mat);

  auto* pedestal = scene.add_mesh(
      fury::make_box({0.9f, 1.3f, 0.9f}, Vec3{0.85f, 0.82f, 0.78f}));
  Material ped_mat;
  ped_mat.metallic = 0.35f;
  ped_mat.roughness = 0.35f;
  add_solid_box(scene, pedestal, "JewelPedestal", {cx, 0.65f, cz + 0.2f},
                {0.9f, 1.3f, 0.9f}, ped_mat);
  auto* gem = scene.add_mesh(
      fury::make_box({0.35f, 0.35f, 0.35f}, Vec3{0.55f, 0.85f, 1.0f}));
  Material gem_mat;
  gem_mat.albedo = {0.6f, 0.95f, 1.2f};
  gem_mat.emissive = 1.1f;
  gem_mat.metallic = 0.4f;
  gem_mat.roughness = 0.2f;
  add_prop(scene, gem, "JewelGem", {cx, 1.5f, cz + 0.2f}, gem_mat);

  auto* carpet = scene.add_mesh(
      fury::make_plane(6.5f, 4.5f, Vec3{0.45f, 0.12f, 0.16f}, 2.f));
  {
    Entity e;
    e.name = "JewelCarpet";
    e.mesh = carpet;
    e.transform.position = {cx, 0.09f, cz + 0.8f};
    e.material.albedo = {1.15f, 0.55f, 0.55f};
    e.material.roughness = 0.85f;
    scene.add_entity(std::move(e));
  }

  auto* ceil_lamp = scene.add_mesh(
      fury::make_box({1.6f, 0.25f, 1.6f}, Vec3{0.95f, 0.90f, 0.70f}));
  Material ceil_mat;
  ceil_mat.albedo = {1.f, 0.95f, 0.75f};
  ceil_mat.emissive = 1.4f;
  ceil_mat.roughness = 0.9f;
  {
    Entity e;
    e.name = "JewelCeilLamp";
    e.tag = "lamp";
    e.mesh = ceil_lamp;
    e.transform.position = {cx, 5.5f, cz};
    e.material = ceil_mat;
    scene.add_entity(std::move(e));
  }

  // Clearer jewelry doorway frame + threshold
  auto* door_post = scene.add_mesh(
      fury::make_box({0.45f, 3.6f, 0.45f}, Vec3{0.75f, 0.55f, 0.35f}));
  Material jframe;
  jframe.albedo = {1.05f, 0.85f, 0.55f};
  jframe.metallic = 0.55f;
  jframe.roughness = 0.35f;
  const float door_z = cz + d * 0.5f;
  add_solid_box(scene, door_post, "JewelDoorPostL", {cx - 1.7f, 1.8f, door_z},
                {0.45f, 3.6f, 0.45f}, jframe);
  add_solid_box(scene, door_post, "JewelDoorPostR", {cx + 1.7f, 1.8f, door_z},
                {0.45f, 3.6f, 0.45f}, jframe);
  auto* jlintel = scene.add_mesh(
      fury::make_box({4.0f, 0.4f, 0.55f}, Vec3{0.70f, 0.50f, 0.30f}));
  add_prop(scene, jlintel, "JewelDoorLintel", {cx, 3.7f, door_z}, jframe);
  auto* jthresh = scene.add_mesh(
      fury::make_box({3.2f, 0.1f, 1.1f}, Vec3{0.35f, 0.18f, 0.16f}));
  Material jtmat;
  jtmat.albedo = {0.85f, 0.45f, 0.40f};
  jtmat.emissive = 0.2f;
  jtmat.roughness = 0.65f;
  add_prop(scene, jthresh, "JewelThreshold", {cx, 0.08f, door_z + 0.3f}, jtmat);

  // Side display plinths inside the shop
  auto* plinth = scene.add_mesh(
      fury::make_box({1.2f, 0.7f, 1.2f}, Vec3{0.70f, 0.68f, 0.65f}));
  add_solid_box(scene, plinth, "JewelPlinthA", {cx - 3.2f, 0.35f, cz + 2.6f},
                {1.2f, 0.7f, 1.2f}, ped_mat);
  add_solid_box(scene, plinth, "JewelPlinthB", {cx + 3.2f, 0.35f, cz + 2.6f},
                {1.2f, 0.7f, 1.2f}, ped_mat);

  // 3.5.0 security cameras + breaker (site 1 = Crown & Cutler)
  {
    auto* cam_body = scene.add_mesh(
        fury::make_box({0.32f, 0.26f, 0.4f}, Vec3{0.15f, 0.16f, 0.18f}));
    auto* cam_lens = scene.add_mesh(
        fury::make_box({0.14f, 0.14f, 0.14f}, Vec3{0.3f, 0.8f, 1.0f}));
    auto* brk = scene.add_mesh(
        fury::make_box({0.7f, 1.2f, 0.35f}, Vec3{0.7f, 0.68f, 0.25f}));
    place_security_camera(scene, cam_body, cam_lens, "JewelCamFront", "JewelCamFrontLens",
                          {cx, 4.2f, cz + 3.6f}, -1.5708f);
    place_security_camera(scene, cam_body, cam_lens, "JewelCamSide", "JewelCamSideLens",
                          {cx - 4.5f, 3.8f, cz}, 0.2f);
    place_breaker_box(scene, brk, "JewelBreaker",
                      {cx + 4.6f, 1.1f, cz - 2.8f});
  }
}


void build_ridge_pier(fury::Scene& scene) {
  // Second district stub east of Harbor Metro, linked by a road bridge.
  const float ox = 95.f;
  const float oz = 8.f;

  Material stone;
  stone.albedo = {0.55f, 0.58f, 0.62f};
  stone.roughness = 0.65f;
  stone.texture = TextureSlot::Concrete;

  Material pier_wood;
  pier_wood.albedo = {0.95f, 0.85f, 0.65f};
  pier_wood.roughness = 0.8f;

  Material teal;
  teal.albedo = {0.35f, 0.55f, 0.58f};
  teal.roughness = 0.55f;
  teal.texture = TextureSlot::Concrete;

  // Bridge deck connecting Harbor (~x=55) to Ridge Pier (~x=80)
  auto* bridge = scene.add_mesh(
      fury::make_box({36.f, 0.45f, 7.f}, Vec3{0.40f, 0.40f, 0.42f}));
  Material bridge_mat;
  bridge_mat.albedo = {0.9f, 0.9f, 0.92f};
  bridge_mat.roughness = 0.7f;
  bridge_mat.metallic = 0.15f;
  bridge_mat.texture = TextureSlot::Asphalt;
  add_prop(scene, bridge, "MetroBridge", {70.f, 0.35f, 6.f}, bridge_mat);

  auto* rail = scene.add_mesh(
      fury::make_box({36.f, 0.9f, 0.25f}, Vec3{0.55f, 0.55f, 0.58f}));
  Material rail_mat;
  rail_mat.metallic = 0.7f;
  rail_mat.roughness = 0.35f;
  add_prop(scene, rail, "BridgeRailN", {70.f, 0.9f, 2.6f}, rail_mat);
  add_prop(scene, rail, "BridgeRailS", {70.f, 0.9f, 9.4f}, rail_mat);

  auto* pillar = scene.add_mesh(
      fury::make_box({1.4f, 4.5f, 1.4f}, Vec3{0.35f, 0.36f, 0.38f}));
  for (float x : {58.f, 70.f, 82.f}) {
    add_solid_box(scene, pillar, "BridgePillar", {x, -1.5f, 6.f},
                  {1.4f, 4.5f, 1.4f}, stone);
  }

  // Ridge Pier plaza + warehouses
  auto* plaza = scene.add_mesh(
      fury::make_plane(48.f, 36.f, Vec3{0.45f, 0.44f, 0.40f}, 8.f));
  {
    Entity e;
    e.name = "RidgePlaza";
    e.mesh = plaza;
    e.transform.position = {ox, 0.06f, oz};
    e.material = stone;
    e.material.albedo = {1.05f, 1.0f, 0.92f};
    scene.add_entity(std::move(e));
  }

  struct Bldg {
    Vec3 pos;
    Vec3 size;
    Vec3 top;
    Vec3 side;
  };
  const Bldg ridge_bldgs[] = {
      {{ox - 14.f, 0.f, oz - 10.f}, {10.f, 8.f, 9.f}, {0.42f, 0.50f, 0.52f}, {0.30f, 0.36f, 0.38f}},
      {{ox + 12.f, 0.f, oz - 8.f}, {12.f, 10.f, 10.f}, {0.50f, 0.45f, 0.40f}, {0.36f, 0.32f, 0.28f}},
      {{ox + 10.f, 0.f, oz + 12.f}, {9.f, 6.f, 8.f}, {0.38f, 0.44f, 0.48f}, {0.28f, 0.32f, 0.35f}},
      {{ox - 12.f, 0.f, oz + 12.f}, {11.f, 7.f, 8.f}, {0.55f, 0.48f, 0.42f}, {0.40f, 0.34f, 0.30f}},
      {{ox + 22.f, 0.f, oz + 2.f}, {8.f, 12.f, 8.f}, {0.32f, 0.38f, 0.45f}, {0.24f, 0.28f, 0.34f}},
  };
  int ri = 0;
  for (const Bldg& spec : ridge_bldgs) {
    auto* mesh = scene.add_mesh(
        fury::make_colored_box(spec.size, spec.top, spec.side));
    Material bm = teal;
    bm.albedo = {1.f, 1.f, 1.f};
    const Vec3 pos{spec.pos.x, spec.size.y * 0.5f, spec.pos.z};
    const std::string name = "RidgeBldg" + std::to_string(ri++);
    add_solid_box(scene, mesh, name.c_str(), pos, spec.size, bm);
  }

  // Pier deck + water tongue
  auto* deck = scene.add_mesh(
      fury::make_box({28.f, 0.4f, 7.f}, Vec3{0.42f, 0.34f, 0.24f}));
  add_prop(scene, deck, "RidgePierDeck", {ox + 6.f, 0.25f, oz + 22.f}, pier_wood);

  auto* water = scene.add_mesh(
      fury::make_plane(50.f, 28.f, Vec3{0.12f, 0.32f, 0.52f}, 8.f));
  {
    Entity w;
    w.name = "RidgeWater";
    w.mesh = water;
    w.transform.position = {ox + 8.f, -0.4f, oz + 30.f};
    w.material.texture = TextureSlot::Water;
    w.material.roughness = 0.18f;
    w.material.metallic = 0.45f;
    w.material.albedo = {0.7f, 0.92f, 1.12f};
    w.material.uv_scroll_u = 0.045f;
    w.material.uv_scroll_v = 0.028f;
    scene.add_entity(std::move(w));
  }

  auto* beacon = scene.add_mesh(
      fury::make_box({1.2f, 5.5f, 1.2f}, Vec3{0.85f, 0.85f, 0.80f}));
  Material beacon_mat;
  beacon_mat.roughness = 0.4f;
  beacon_mat.metallic = 0.2f;
  add_solid_box(scene, beacon, "RidgeBeacon", {ox + 18.f, 2.75f, oz + 24.f},
                {1.2f, 5.5f, 1.2f}, beacon_mat);
  auto* beacon_light = scene.add_mesh(
      fury::make_box({1.4f, 0.5f, 1.4f}, Vec3{0.95f, 0.85f, 0.45f}));
  Material bl;
  bl.albedo = {1.f, 0.9f, 0.5f};
  bl.emissive = 2.2f;
  bl.roughness = 0.9f;
  {
    Entity e;
    e.name = "RidgeBeaconLamp";
    e.tag = "lamp";
    e.mesh = beacon_light;
    e.transform.position = {ox + 18.f, 5.6f, oz + 24.f};
    e.material = bl;
    scene.add_entity(std::move(e));
  }

  auto* pole = scene.add_mesh(
      fury::make_box({0.22f, 4.4f, 0.22f}, Vec3{0.12f, 0.12f, 0.12f}));
  auto* lamp = scene.add_mesh(
      fury::make_box({0.75f, 0.28f, 0.75f}, Vec3{0.95f, 0.90f, 0.55f}));
  const Vec3 ridge_lamps[] = {
      {ox - 10.f, 0.f, oz}, {ox + 10.f, 0.f, oz}, {ox, 0.f, oz + 14.f},
      {ox + 16.f, 0.f, oz + 20.f}, {82.f, 0.f, 6.f},
  };
  for (const Vec3& p : ridge_lamps) {
    place_lamp(scene, pole, lamp, p.x, p.z);
  }

  // District sign stub
  auto* sign = scene.add_mesh(
      fury::make_box({6.f, 2.2f, 0.35f}, Vec3{0.15f, 0.35f, 0.45f}));
  Material sign_mat;
  sign_mat.albedo = {0.4f, 0.85f, 0.95f};
  sign_mat.emissive = 0.85f;
  sign_mat.roughness = 0.9f;
  add_prop(scene, sign, "RidgeSign", {ox - 2.f, 3.2f, oz - 16.f}, sign_mat);
}


void build_ashcourt_market(fury::Scene& scene) {
  // Third district stub west/south of Harbor Metro — Ashcourt Market.
  const float ox = -88.f;
  const float oz = 42.f;

  Material stone;
  stone.albedo = {0.58f, 0.52f, 0.46f};
  stone.roughness = 0.7f;
  stone.texture = TextureSlot::Concrete;

  Material stall;
  stall.albedo = {0.85f, 0.55f, 0.28f};
  stall.roughness = 0.65f;

  Material canvas;
  canvas.albedo = {0.75f, 0.22f, 0.28f};
  canvas.roughness = 0.85f;
  canvas.emissive = 0.15f;

  // Connector road from Harbor west edge toward Ashcourt
  auto* road = scene.add_mesh(
      fury::make_box({34.f, 0.35f, 8.f}, Vec3{0.28f, 0.28f, 0.30f}));
  Material road_mat;
  road_mat.albedo = {0.95f, 0.95f, 0.98f};
  road_mat.roughness = 0.8f;
  road_mat.texture = TextureSlot::Asphalt;
  {
    Entity e;
    e.name = "AshcourtRoad";
    e.tag = "asphalt";
    e.mesh = road;
    e.transform.position = {-62.f, 0.2f, 28.f};
    e.material = road_mat;
    scene.add_entity(std::move(e));
  }

  auto* plaza = scene.add_mesh(
      fury::make_plane(42.f, 34.f, Vec3{0.50f, 0.46f, 0.40f}, 8.f));
  {
    Entity e;
    e.name = "AshcourtPlaza";
    e.mesh = plaza;
    e.transform.position = {ox, 0.06f, oz};
    e.material = stone;
    e.material.albedo = {1.05f, 0.98f, 0.88f};
    scene.add_entity(std::move(e));
  }

  struct Shop {
    Vec3 pos;
    Vec3 size;
    Vec3 top;
    Vec3 side;
  };
  const Shop shops[] = {
      {{ox - 12.f, 0.f, oz - 8.f}, {9.f, 6.f, 8.f}, {0.62f, 0.48f, 0.36f}, {0.45f, 0.34f, 0.26f}},
      {{ox + 10.f, 0.f, oz - 10.f}, {10.f, 7.f, 9.f}, {0.48f, 0.52f, 0.55f}, {0.34f, 0.38f, 0.40f}},
      {{ox + 12.f, 0.f, oz + 10.f}, {8.f, 5.5f, 8.f}, {0.55f, 0.40f, 0.42f}, {0.40f, 0.28f, 0.30f}},
      {{ox - 10.f, 0.f, oz + 12.f}, {11.f, 6.5f, 8.f}, {0.40f, 0.46f, 0.42f}, {0.30f, 0.34f, 0.30f}},
      {{ox + 20.f, 0.f, oz + 2.f}, {7.f, 8.f, 7.f}, {0.35f, 0.38f, 0.48f}, {0.26f, 0.28f, 0.36f}},
  };
  int si = 0;
  for (const Shop& s : shops) {
    auto* mesh = scene.add_mesh(fury::make_colored_box(s.size, s.top, s.side));
    Material bm = stone;
    bm.albedo = {1.f, 1.f, 1.f};
    const Vec3 pos{s.pos.x, s.size.y * 0.5f, s.pos.z};
    const std::string name = "AshShop" + std::to_string(si++);
    add_solid_box(scene, mesh, name.c_str(), pos, s.size, bm);
  }

  // Market stall awnings / crates
  auto* awning = scene.add_mesh(
      fury::make_box({4.5f, 0.18f, 3.2f}, Vec3{0.70f, 0.20f, 0.22f}));
  add_prop(scene, awning, "StallAwningA", {ox - 2.f, 2.6f, oz + 2.f}, canvas);
  add_prop(scene, awning, "StallAwningB", {ox + 4.f, 2.6f, oz - 2.f}, canvas);

  auto* stall_box = scene.add_mesh(
      fury::make_box({3.6f, 1.1f, 1.4f}, Vec3{0.55f, 0.38f, 0.22f}));
  add_solid_box(scene, stall_box, "MarketStallA", {ox - 2.f, 0.55f, oz + 2.f},
                {3.6f, 1.1f, 1.4f}, stall);
  add_solid_box(scene, stall_box, "MarketStallB", {ox + 4.f, 0.55f, oz - 2.f},
                {3.6f, 1.1f, 1.4f}, stall);

  auto* crate = scene.add_mesh(
      fury::make_box({1.2f, 1.2f, 1.2f}, Vec3{0.50f, 0.36f, 0.20f}));
  Material crate_mat;
  crate_mat.roughness = 0.75f;
  crate_mat.texture = TextureSlot::Checker;
  add_solid_box(scene, crate, "AshCrateA", {ox + 1.f, 0.6f, oz + 6.f},
                {1.2f, 1.2f, 1.2f}, crate_mat);
  add_solid_box(scene, crate, "AshCrateB", {ox - 4.f, 0.6f, oz + 5.f},
                {1.2f, 1.2f, 1.2f}, crate_mat);
  add_solid_box(scene, crate, "AshCrateC", {ox + 6.f, 0.6f, oz + 4.f},
                {1.2f, 1.2f, 1.2f}, crate_mat);

  // ATM heist-lite — recessed alcove booth (side walls + canopy)
  const float atm_x = ox - 2.f;
  const float atm_z = oz - 14.f;
  Material booth;
  booth.metallic = 0.55f;
  booth.roughness = 0.4f;
  booth.albedo = {0.9f, 0.92f, 0.98f};
  Material alcove_wall;
  alcove_wall.albedo = {0.42f, 0.40f, 0.38f};
  alcove_wall.roughness = 0.65f;
  alcove_wall.texture = TextureSlot::Concrete;

  auto* atm_back = scene.add_mesh(
      fury::make_box({3.6f, 3.0f, 0.45f}, Vec3{0.35f, 0.36f, 0.38f}));
  add_solid_box(scene, atm_back, "AshAtmAlcoveBack", {atm_x, 1.5f, atm_z - 1.1f},
                {3.6f, 3.0f, 0.45f}, alcove_wall);
  auto* atm_side = scene.add_mesh(
      fury::make_box({0.4f, 3.0f, 2.4f}, Vec3{0.32f, 0.33f, 0.35f}));
  add_solid_box(scene, atm_side, "AshAtmAlcoveL", {atm_x - 1.7f, 1.5f, atm_z},
                {0.4f, 3.0f, 2.4f}, alcove_wall);
  add_solid_box(scene, atm_side, "AshAtmAlcoveR", {atm_x + 1.7f, 1.5f, atm_z},
                {0.4f, 3.0f, 2.4f}, alcove_wall);
  auto* atm_canopy = scene.add_mesh(
      fury::make_box({3.8f, 0.28f, 2.6f}, Vec3{0.55f, 0.52f, 0.48f}));
  add_prop(scene, atm_canopy, "AshAtmCanopy", {atm_x, 3.15f, atm_z + 0.1f},
           booth);

  auto* atm_booth = scene.add_mesh(
      fury::make_box({2.0f, 2.4f, 1.2f}, Vec3{0.18f, 0.20f, 0.24f}));
  add_solid_box(scene, atm_booth, "AshAtmBooth", {atm_x, 1.2f, atm_z - 0.35f},
                {2.0f, 2.4f, 1.2f}, booth);

  auto* atm_face = scene.add_mesh(
      fury::make_box({1.4f, 1.6f, 0.35f}, Vec3{0.10f, 0.12f, 0.14f}));
  Material atm_mat;
  atm_mat.metallic = 0.7f;
  atm_mat.roughness = 0.3f;
  {
    Entity t;
    t.name = "AshcourtAtm";
    t.tag = "vault_atm";
    t.mesh = atm_face;
    t.transform.position = {atm_x, 1.4f, atm_z + 0.55f};
    t.material = atm_mat;
    t.solid = true;
    t.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, {1.4f, 1.6f, 0.35f});
    scene.add_entity(std::move(t));
  }

  auto* atm_screen = scene.add_mesh(
      fury::make_box({0.85f, 0.55f, 0.06f}, Vec3{0.25f, 0.9f, 0.55f}));
  Material screen;
  screen.albedo = {0.4f, 1.0f, 0.65f};
  screen.emissive = 1.7f;
  screen.roughness = 0.9f;
  add_prop(scene, atm_screen, "AshAtmScreen", {atm_x, 1.65f, atm_z + 0.75f},
           screen);

  // Alcove floor strip + side lights
  auto* alcove_floor = scene.add_mesh(
      fury::make_box({3.4f, 0.1f, 2.2f}, Vec3{0.22f, 0.22f, 0.24f}));
  Material strip;
  strip.albedo = {0.7f, 0.72f, 0.78f};
  strip.emissive = 0.15f;
  add_prop(scene, alcove_floor, "AshAtmFloor", {atm_x, 0.08f, atm_z + 0.2f},
           strip);
  auto* alcove_bulb = scene.add_mesh(
      fury::make_box({0.35f, 0.2f, 0.35f}, Vec3{0.95f, 0.9f, 0.55f}));
  Material bulb;
  bulb.albedo = {1.f, 0.92f, 0.6f};
  bulb.emissive = 2.0f;
  bulb.roughness = 0.9f;
  {
    Entity e;
    e.name = "AshAtmLamp";
    e.tag = "lamp";
    e.mesh = alcove_bulb;
    e.transform.position = {atm_x, 2.95f, atm_z + 0.4f};
    e.material = bulb;
    scene.add_entity(std::move(e));
  }

  // Ashcourt fence shop — buy crew / heat / loot perks with cash (B menu)
  {
    const float sx = kAshcourtShopPos.x;
    const float sz = kAshcourtShopPos.z;
    auto* shop_body = scene.add_mesh(
        fury::make_box({4.5f, 2.8f, 3.2f}, Vec3{0.28f, 0.22f, 0.18f}));
    Material shop_mat;
    shop_mat.albedo = {0.85f, 0.55f, 0.30f};
    shop_mat.roughness = 0.6f;
    add_solid_box(scene, shop_body, "AshFenceShop", {sx, 1.4f, sz},
                  {4.5f, 2.8f, 3.2f}, shop_mat);
    auto* awning = scene.add_mesh(
        fury::make_box({5.0f, 0.2f, 1.8f}, Vec3{0.15f, 0.45f, 0.35f}));
    Material awn;
    awn.albedo = {0.35f, 0.85f, 0.55f};
    awn.emissive = 0.45f;
    awn.roughness = 0.85f;
    add_prop(scene, awning, "AshFenceAwning", {sx, 3.0f, sz + 1.8f}, awn);
    auto* counter = scene.add_mesh(
        fury::make_box({3.6f, 1.0f, 0.9f}, Vec3{0.40f, 0.32f, 0.25f}));
    Material wood;
    wood.roughness = 0.7f;
    add_solid_box(scene, counter, "AshFenceCounter", {sx, 0.5f, sz + 1.4f},
                  {3.6f, 1.0f, 0.9f}, wood);
    auto* neon = scene.add_mesh(
        fury::make_box({3.2f, 0.55f, 0.2f}, Vec3{0.2f, 0.9f, 0.55f}));
    Material neon_mat;
    neon_mat.albedo = {0.4f, 1.0f, 0.7f};
    neon_mat.emissive = 1.5f;
    neon_mat.roughness = 0.9f;
    add_prop(scene, neon, "AshFenceNeon", {sx, 2.5f, sz + 1.7f}, neon_mat);
    // Tag marker entity for proximity checks
    Entity marker;
    marker.name = "AshFenceShopMarker";
    marker.tag = "shop";
    marker.transform.position = {sx, 0.f, sz};
    scene.add_entity(std::move(marker));
  }

  // District sign
  auto* sign = scene.add_mesh(
      fury::make_box({7.f, 2.0f, 0.35f}, Vec3{0.45f, 0.25f, 0.15f}));
  Material sign_mat;
  sign_mat.albedo = {1.0f, 0.75f, 0.35f};
  sign_mat.emissive = 0.9f;
  sign_mat.roughness = 0.9f;
  add_prop(scene, sign, "AshcourtSign", {ox + 2.f, 3.0f, oz + 18.f}, sign_mat);

  auto* pole = scene.add_mesh(
      fury::make_box({0.22f, 4.4f, 0.22f}, Vec3{0.12f, 0.12f, 0.12f}));
  auto* lamp = scene.add_mesh(
      fury::make_box({0.75f, 0.28f, 0.75f}, Vec3{0.95f, 0.90f, 0.55f}));
  const Vec3 lamps[] = {
      {ox - 8.f, 0.f, oz}, {ox + 8.f, 0.f, oz}, {ox, 0.f, oz + 12.f},
      {ox - 2.f, 0.f, oz - 12.f}, {-70.f, 0.f, 28.f}, {-55.f, 0.f, 28.f},
  };
  for (const Vec3& p : lamps) {
    place_lamp(scene, pole, lamp, p.x, p.z);
  }
}


void build_harbor_armored_depot(fury::Scene& scene) {
  // Fourth heist target (1.2.0): Harbor Metro armored cash depot — short loot, tier 2.
  // Southeast industrial stub; original fictional location (no third-party IP).
  const float ox = 58.f;
  const float oz = -48.f;

  Material steel;
  steel.albedo = {0.55f, 0.58f, 0.64f};
  steel.metallic = 0.78f;
  steel.roughness = 0.34f;
  steel.texture = TextureSlot::Metal;

  Material dark;
  dark.albedo = {0.22f, 0.24f, 0.28f};
  dark.metallic = 0.55f;
  dark.roughness = 0.45f;

  Material warning;
  warning.albedo = {0.95f, 0.72f, 0.12f};
  warning.emissive = 0.35f;
  warning.roughness = 0.85f;

  // Connector stub from plaza SE toward depot
  auto* road = scene.add_mesh(
      fury::make_box({28.f, 0.32f, 7.f}, Vec3{0.28f, 0.28f, 0.30f}));
  Material road_mat;
  road_mat.albedo = {0.95f, 0.95f, 0.98f};
  road_mat.roughness = 0.8f;
  road_mat.texture = TextureSlot::Asphalt;
  add_prop(scene, road, "DepotRoad", {36.f, 0.18f, -36.f}, road_mat);

  auto* yard = scene.add_mesh(
      fury::make_plane(28.f, 24.f, Vec3{0.40f, 0.40f, 0.38f}, 6.f));
  {
    Entity e;
    e.name = "DepotYard";
    e.mesh = yard;
    e.transform.position = {ox, 0.05f, oz};
    e.material = steel;
    e.material.albedo = {0.95f, 0.95f, 0.92f};
    e.material.metallic = 0.1f;
    e.material.roughness = 0.8f;
    scene.add_entity(std::move(e));
  }

  // Main depot hall (solid shell with doorway gap on +Z via missing front mid)
  const float hall_w = 16.f;
  const float hall_d = 12.f;
  const float hall_h = 7.f;
  auto* wall_n = scene.add_mesh(
      fury::make_box({hall_w, hall_h, 1.0f}, Vec3{0.40f, 0.44f, 0.50f}));
  auto* wall_s = scene.add_mesh(
      fury::make_box({hall_w * 0.35f, hall_h, 1.0f}, Vec3{0.38f, 0.42f, 0.48f}));
  auto* wall_e = scene.add_mesh(
      fury::make_box({1.0f, hall_h, hall_d}, Vec3{0.36f, 0.40f, 0.46f}));
  auto* wall_w = scene.add_mesh(
      fury::make_box({1.0f, hall_h, hall_d}, Vec3{0.36f, 0.40f, 0.46f}));
  add_solid_box(scene, wall_n, "DepotWallN", {ox, hall_h * 0.5f, oz - hall_d * 0.5f},
                {hall_w, hall_h, 1.0f}, steel, "depot");
  // Front split walls leave a doorway
  add_solid_box(scene, wall_s, "DepotWallSL",
                {ox - hall_w * 0.32f, hall_h * 0.5f, oz + hall_d * 0.5f},
                {hall_w * 0.35f, hall_h, 1.0f}, steel, "depot");
  add_solid_box(scene, wall_s, "DepotWallSR",
                {ox + hall_w * 0.32f, hall_h * 0.5f, oz + hall_d * 0.5f},
                {hall_w * 0.35f, hall_h, 1.0f}, steel, "depot");
  add_solid_box(scene, wall_e, "DepotWallE", {ox + hall_w * 0.5f, hall_h * 0.5f, oz},
                {1.0f, hall_h, hall_d}, steel, "depot");
  add_solid_box(scene, wall_w, "DepotWallW", {ox - hall_w * 0.5f, hall_h * 0.5f, oz},
                {1.0f, hall_h, hall_d}, steel, "depot");

  auto* roof = scene.add_mesh(
      fury::make_box({hall_w + 0.6f, 0.45f, hall_d + 0.6f}, Vec3{0.30f, 0.32f, 0.36f}));
  add_prop(scene, roof, "DepotRoof", {ox, hall_h + 0.2f, oz}, dark);

  // Garage bay / loading dock
  auto* bay = scene.add_mesh(
      fury::make_box({6.5f, 4.2f, 5.0f}, Vec3{0.35f, 0.38f, 0.42f}));
  add_solid_box(scene, bay, "DepotGarageBay", {ox + 11.5f, 2.1f, oz + 2.f},
                {6.5f, 4.2f, 5.0f}, dark);
  auto* ramp = scene.add_mesh(
      fury::make_box({5.5f, 0.35f, 4.0f}, Vec3{0.45f, 0.45f, 0.42f}));
  add_prop(scene, ramp, "DepotRamp", {ox + 11.5f, 0.2f, oz + 6.5f}, steel);

  // Armored cage / loot target (short grab)
  auto* cage = scene.add_mesh(
      fury::make_box({2.8f, 2.4f, 2.2f}, Vec3{0.75f, 0.70f, 0.25f}));
  Material cage_mat;
  cage_mat.albedo = {1.15f, 0.95f, 0.35f};
  cage_mat.metallic = 0.92f;
  cage_mat.roughness = 0.22f;
  cage_mat.emissive = 0.4f;
  {
    Entity t;
    t.name = "HarborDepotCage";
    t.tag = "vault_depot";
    t.mesh = cage;
    t.transform.position = {ox, 1.2f, oz - 3.2f};
    t.material = cage_mat;
    t.solid = true;
    t.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, {2.8f, 2.4f, 2.2f});
    scene.add_entity(std::move(t));
  }

  // Cash crates (short loot props)
  auto* crate = scene.add_mesh(
      fury::make_box({1.3f, 1.0f, 1.0f}, Vec3{0.20f, 0.45f, 0.22f}));
  Material crate_mat;
  crate_mat.roughness = 0.7f;
  crate_mat.albedo = {0.25f, 0.55f, 0.30f};
  add_solid_box(scene, crate, "DepotCashA", {ox - 3.2f, 0.5f, oz - 1.5f},
                {1.3f, 1.0f, 1.0f}, crate_mat);
  add_solid_box(scene, crate, "DepotCashB", {ox + 3.0f, 0.5f, oz - 1.2f},
                {1.3f, 1.0f, 1.0f}, crate_mat);
  add_solid_box(scene, crate, "DepotCashC", {ox + 1.2f, 0.5f, oz - 4.0f},
                {1.3f, 1.0f, 1.0f}, crate_mat);

  // Interior ceiling lamps (2.5.0)
  {
    auto* ceil_lamp = scene.add_mesh(
        fury::make_box({1.5f, 0.22f, 1.5f}, Vec3{0.9f, 0.88f, 0.7f}));
    Material ceil_mat;
    ceil_mat.albedo = {0.95f, 0.92f, 0.75f};
    ceil_mat.emissive = 1.55f;
    ceil_mat.roughness = 0.9f;
    Entity e;
    e.name = "DepotCeilLampL";
    e.tag = "lamp";
    e.mesh = ceil_lamp;
    e.transform.position = {ox - 4.f, hall_h - 0.9f, oz};
    e.material = ceil_mat;
    scene.add_entity(std::move(e));
    Entity e2;
    e2.name = "DepotCeilLampR";
    e2.tag = "lamp";
    e2.mesh = ceil_lamp;
    e2.transform.position = {ox + 4.f, hall_h - 0.9f, oz};
    e2.material = ceil_mat;
    scene.add_entity(std::move(e2));
  }

  // Fence / barriers
  auto* fence = scene.add_mesh(
      fury::make_box({0.15f, 2.2f, 10.f}, Vec3{0.55f, 0.55f, 0.50f}));
  Material fence_mat;
  fence_mat.metallic = 0.6f;
  fence_mat.roughness = 0.4f;
  add_solid_box(scene, fence, "DepotFenceL", {ox - 14.f, 1.1f, oz + 2.f},
                {0.15f, 2.2f, 10.f}, fence_mat);
  add_solid_box(scene, fence, "DepotFenceR", {ox + 18.f, 1.1f, oz + 2.f},
                {0.15f, 2.2f, 10.f}, fence_mat);

  auto* stripe = scene.add_mesh(
      fury::make_box({8.f, 0.12f, 0.45f}, Vec3{0.95f, 0.75f, 0.15f}));
  add_prop(scene, stripe, "DepotStripeA", {ox, 0.12f, oz + 7.2f}, warning);
  add_prop(scene, stripe, "DepotStripeB", {ox + 11.5f, 0.12f, oz + 8.5f}, warning);

  // District sign
  auto* sign = scene.add_mesh(
      fury::make_box({8.5f, 1.8f, 0.35f}, Vec3{0.55f, 0.35f, 0.20f}));
  Material sign_mat;
  sign_mat.albedo = {0.85f, 0.55f, 0.15f};
  sign_mat.emissive = 0.85f;
  sign_mat.roughness = 0.9f;
  add_prop(scene, sign, "DepotSign", {ox - 2.f, 3.2f, oz + 10.f}, sign_mat);

  // Alarm sirens (roof + bay)
  auto* siren = scene.add_mesh(
      fury::make_box({0.5f, 0.32f, 0.5f}, Vec3{0.95f, 0.15f, 0.12f}));
  Material siren_mat;
  siren_mat.albedo = {1.0f, 0.18f, 0.12f};
  siren_mat.emissive = 0.2f;
  siren_mat.roughness = 0.85f;
  {
    Entity s;
    s.name = "DepotSirenRoof";
    s.tag = "siren";
    s.mesh = siren;
    s.transform.position = {ox, hall_h + 0.7f, oz};
    s.material = siren_mat;
    scene.add_entity(std::move(s));
  }
  {
    Entity s;
    s.name = "DepotSirenBay";
    s.tag = "siren";
    s.mesh = siren;
    s.transform.position = {ox + 11.5f, 4.5f, oz + 2.f};
    s.material = siren_mat;
    scene.add_entity(std::move(s));
  }

  auto* pole = scene.add_mesh(
      fury::make_box({0.22f, 4.4f, 0.22f}, Vec3{0.12f, 0.12f, 0.12f}));
  auto* lamp = scene.add_mesh(
      fury::make_box({0.75f, 0.28f, 0.75f}, Vec3{0.95f, 0.90f, 0.55f}));
  place_lamp(scene, pole, lamp, ox - 10.f, oz + 8.f);
  place_lamp(scene, pole, lamp, ox + 14.f, oz + 8.f);
  place_lamp(scene, pole, lamp, ox, oz - 8.f);

  // 3.5.0 security cameras + breaker (site 2 = Harbor Depot)
  {
    auto* cam_body = scene.add_mesh(
        fury::make_box({0.35f, 0.28f, 0.45f}, Vec3{0.12f, 0.14f, 0.16f}));
    auto* cam_lens = scene.add_mesh(
        fury::make_box({0.16f, 0.16f, 0.16f}, Vec3{0.3f, 0.8f, 1.0f}));
    auto* brk = scene.add_mesh(
        fury::make_box({0.7f, 1.2f, 0.35f}, Vec3{0.7f, 0.68f, 0.25f}));
    place_security_camera(scene, cam_body, cam_lens, "DepotCamHall", "DepotCamHallLens",
                          {ox, 4.5f, oz + 4.5f}, -1.5708f);
    place_security_camera(scene, cam_body, cam_lens, "DepotCamBay", "DepotCamBayLens",
                          {ox + 11.5f, 4.0f, oz + 4.5f}, -1.8f);
    place_breaker_box(scene, brk, "DepotBreaker",
                      {ox - 6.5f, 1.1f, oz + 4.8f});
  }
}

void build_harbor_loft(fury::Scene& scene) {
  // Enterable Harbor loft safehouse — clears heat while inside (no IP refs).
  const float cx = kHarborLoftPos.x;
  const float cz = kHarborLoftPos.z;
  const float w = 11.f;
  const float d = 9.f;
  const float h = 5.5f;

  Material brick;
  brick.albedo = {1.05f, 0.95f, 0.9f};
  brick.roughness = 0.68f;
  brick.texture = TextureSlot::Brick;

  Material dark;
  dark.albedo = {0.28f, 0.24f, 0.22f};
  dark.roughness = 0.55f;

  auto* wall_n = scene.add_mesh(
      fury::make_colored_box({w, h, 0.7f}, brick.albedo, dark.albedo));
  auto* wall_w = scene.add_mesh(
      fury::make_colored_box({0.7f, h, d}, brick.albedo, dark.albedo));
  auto* wall_e = scene.add_mesh(
      fury::make_colored_box({0.7f, h, d}, brick.albedo, dark.albedo));
  auto* wall_s_l = scene.add_mesh(
      fury::make_colored_box({3.6f, h, 0.7f}, brick.albedo, dark.albedo));
  auto* wall_s_r = scene.add_mesh(
      fury::make_colored_box({3.6f, h, 0.7f}, brick.albedo, dark.albedo));

  add_solid_box(scene, wall_n, "LoftWallN", {cx, h * 0.5f, cz - d * 0.5f},
                {w, h, 0.7f}, brick, "loft");
  add_solid_box(scene, wall_w, "LoftWallW", {cx - w * 0.5f, h * 0.5f, cz},
                {0.7f, h, d}, brick, "loft");
  add_solid_box(scene, wall_e, "LoftWallE", {cx + w * 0.5f, h * 0.5f, cz},
                {0.7f, h, d}, brick, "loft");
  add_solid_box(scene, wall_s_l, "LoftWallSL",
                {cx - 3.2f, h * 0.5f, cz + d * 0.5f}, {3.6f, h, 0.7f}, brick,
                "loft");
  add_solid_box(scene, wall_s_r, "LoftWallSR",
                {cx + 3.2f, h * 0.5f, cz + d * 0.5f}, {3.6f, h, 0.7f}, brick,
                "loft");

  auto* roof = scene.add_mesh(
      fury::make_box({w + 0.3f, 0.4f, d + 0.3f}, Vec3{0.35f, 0.32f, 0.30f}));
  add_prop(scene, roof, "LoftRoof", {cx, h + 0.12f, cz}, brick);

  auto* floor = scene.add_mesh(
      fury::make_plane(w - 1.0f, d - 1.0f, Vec3{0.42f, 0.34f, 0.28f}, 2.5f));
  {
    Entity f;
    f.name = "LoftFloor";
    f.tag = "loft";
    f.mesh = floor;
    f.transform.position = {cx, 0.06f, cz};
    f.material.texture = TextureSlot::Checker;
    f.material.albedo = {1.05f, 0.9f, 0.8f};
    f.material.roughness = 0.65f;
    scene.add_entity(std::move(f));
  }

  // Soft loft interior: couch stub, lamp, rug
  auto* couch = scene.add_mesh(
      fury::make_box({3.2f, 0.7f, 1.1f}, Vec3{0.35f, 0.28f, 0.45f}));
  Material couch_mat;
  couch_mat.roughness = 0.85f;
  couch_mat.albedo = {0.55f, 0.42f, 0.65f};
  add_prop(scene, couch, "LoftCouch", {cx - 1.5f, 0.4f, cz - 1.8f}, couch_mat);

  auto* table = scene.add_mesh(
      fury::make_box({1.4f, 0.45f, 0.9f}, Vec3{0.40f, 0.28f, 0.18f}));
  Material wood;
  wood.roughness = 0.7f;
  wood.albedo = {0.7f, 0.5f, 0.32f};
  add_prop(scene, table, "LoftTable", {cx + 1.8f, 0.3f, cz - 0.5f}, wood);

  auto* loft_lamp = scene.add_mesh(
      fury::make_box({0.35f, 0.35f, 0.35f}, Vec3{1.0f, 0.92f, 0.65f}));
  Material glow;
  glow.albedo = {1.0f, 0.92f, 0.6f};
  glow.emissive = 1.8f;
  glow.roughness = 0.9f;
  {
    Entity lamp;
    lamp.name = "LoftLamp";
    lamp.tag = "lamp";
    lamp.mesh = loft_lamp;
    lamp.transform.position = {cx + 1.8f, 1.15f, cz - 0.5f};
    lamp.material = glow;
    scene.add_entity(std::move(lamp));
  }

  // Doorway frame on +Z
  auto* door_post = scene.add_mesh(
      fury::make_box({0.45f, 3.2f, 0.45f}, Vec3{0.55f, 0.48f, 0.40f}));
  Material frame_mat;
  frame_mat.roughness = 0.5f;
  frame_mat.albedo = {0.75f, 0.68f, 0.55f};
  const float door_z = cz + d * 0.5f;
  add_solid_box(scene, door_post, "LoftDoorPostL", {cx - 1.55f, 1.7f, door_z},
                {0.45f, 3.2f, 0.45f}, frame_mat);
  add_solid_box(scene, door_post, "LoftDoorPostR", {cx + 1.55f, 1.7f, door_z},
                {0.45f, 3.2f, 0.45f}, frame_mat);
  auto* lintel = scene.add_mesh(
      fury::make_box({3.6f, 0.35f, 0.5f}, Vec3{0.55f, 0.48f, 0.40f}));
  add_prop(scene, lintel, "LoftDoorLintel", {cx, 3.4f, door_z}, frame_mat);
  auto* threshold = scene.add_mesh(
      fury::make_box({3.0f, 0.12f, 1.0f}, Vec3{0.35f, 0.32f, 0.28f}));
  Material tmat;
  tmat.roughness = 0.8f;
  add_prop(scene, threshold, "LoftThreshold", {cx, 0.07f, door_z + 0.35f}, tmat);

  // 3.6.0 loft workbench — craft SignalJammer / SmokePellet (G near)
  {
    auto* bench = scene.add_mesh(
        fury::make_box({1.8f, 0.85f, 0.95f}, Vec3{0.32f, 0.36f, 0.40f}));
    Material bench_mat;
    bench_mat.albedo = {0.45f, 0.48f, 0.52f};
    bench_mat.metallic = 0.55f;
    bench_mat.roughness = 0.45f;
    bench_mat.texture = TextureSlot::Metal;
    add_prop(scene, bench, "LoftWorkbench",
             {kLoftWorkbenchPos.x, 0.48f, kLoftWorkbenchPos.z}, bench_mat);
    auto* tools = scene.add_mesh(
        fury::make_box({0.55f, 0.22f, 0.35f}, Vec3{0.85f, 0.55f, 0.25f}));
    Material tool_mat;
    tool_mat.albedo = {1.0f, 0.65f, 0.25f};
    tool_mat.emissive = 0.35f;
    tool_mat.roughness = 0.55f;
    add_prop(scene, tools, "LoftWorkbenchTools",
             {kLoftWorkbenchPos.x, 1.05f, kLoftWorkbenchPos.z}, tool_mat);
  }

  // Exterior sign plate
  auto* sign = scene.add_mesh(
      fury::make_box({2.8f, 0.55f, 0.18f}, Vec3{0.2f, 0.55f, 0.7f}));
  Material sign_mat;
  sign_mat.albedo = {0.35f, 0.85f, 1.1f};
  sign_mat.emissive = 0.9f;
  sign_mat.roughness = 0.85f;
  add_prop(scene, sign, "LoftSign", {cx, 3.8f, door_z + 0.5f}, sign_mat);
}


void build_north_quay(fury::Scene& scene) {
  // Fifth district stub north of Harbor Metro — North Quay industrial.
  // Original fictional waterfront yards (no third-party IP).
  const float ox = 10.f;
  const float oz = 96.f;

  Material steel;
  steel.albedo = {0.50f, 0.54f, 0.60f};
  steel.metallic = 0.82f;
  steel.roughness = 0.32f;
  steel.texture = TextureSlot::Metal;

  Material concrete;
  concrete.albedo = {0.48f, 0.46f, 0.44f};
  concrete.roughness = 0.78f;
  concrete.texture = TextureSlot::Concrete;

  Material asphalt;
  asphalt.albedo = {0.92f, 0.92f, 0.95f};
  asphalt.roughness = 0.82f;
  asphalt.texture = TextureSlot::Asphalt;

  // Road / bridge link from Harbor loft waterfront (~z=52) north into the quay
  auto* road = scene.add_mesh(
      fury::make_box({8.f, 0.35f, 42.f}, Vec3{0.28f, 0.28f, 0.30f}));
  add_prop(scene, road, "NorthQuayRoad", {18.f, 0.2f, 72.f}, asphalt);

  auto* bridge = scene.add_mesh(
      fury::make_box({10.f, 0.45f, 14.f}, Vec3{0.40f, 0.40f, 0.42f}));
  Material bridge_mat = asphalt;
  bridge_mat.metallic = 0.2f;
  add_prop(scene, bridge, "NorthQuayBridge", {18.f, 0.35f, 58.f}, bridge_mat);

  auto* rail = scene.add_mesh(
      fury::make_box({0.3f, 0.9f, 14.f}, Vec3{0.55f, 0.55f, 0.58f}));
  Material rail_mat;
  rail_mat.metallic = 0.75f;
  rail_mat.roughness = 0.35f;
  add_prop(scene, rail, "NQBridgeRailE", {22.8f, 0.9f, 58.f}, rail_mat);
  add_prop(scene, rail, "NQBridgeRailW", {13.2f, 0.9f, 58.f}, rail_mat);

  auto* pillar = scene.add_mesh(
      fury::make_box({1.3f, 4.2f, 1.3f}, Vec3{0.35f, 0.36f, 0.38f}));
  for (float z : {54.f, 62.f}) {
    add_solid_box(scene, pillar, "NQBridgePillar", {18.f, -1.4f, z},
                  {1.3f, 4.2f, 1.3f}, concrete);
  }

  // Industrial plaza pad
  auto* plaza = scene.add_mesh(
      fury::make_plane(52.f, 40.f, Vec3{0.42f, 0.42f, 0.40f}, 8.f));
  {
    Entity e;
    e.name = "NorthQuayPlaza";
    e.mesh = plaza;
    e.transform.position = {ox, 0.05f, oz};
    e.material = concrete;
    e.material.albedo = {0.95f, 0.95f, 0.92f};
    scene.add_entity(std::move(e));
  }

  // Warehouses (long industrial boxes)
  struct Wh {
    Vec3 pos;
    Vec3 size;
    Vec3 rgb;
  };
  const Wh warehouses[] = {
      {{ox - 16.f, 0.f, oz - 6.f}, {14.f, 9.f, 12.f}, {0.42f, 0.46f, 0.52f}},
      {{ox + 16.f, 0.f, oz - 8.f}, {12.f, 8.f, 14.f}, {0.48f, 0.44f, 0.40f}},
      {{ox - 14.f, 0.f, oz + 12.f}, {11.f, 7.f, 10.f}, {0.38f, 0.42f, 0.48f}},
      {{ox + 18.f, 0.f, oz + 10.f}, {10.f, 10.f, 9.f}, {0.45f, 0.40f, 0.36f}},
  };
  int wi = 0;
  for (const Wh& w : warehouses) {
    auto* mesh = scene.add_mesh(
        fury::make_colored_box(w.size, w.rgb,
                               {w.rgb.x * 0.72f, w.rgb.y * 0.72f, w.rgb.z * 0.72f}));
    Material bm = steel;
    bm.albedo = {1.f, 1.f, 1.f};
    bm.metallic = 0.35f;
    bm.roughness = 0.55f;
    const Vec3 pos{w.pos.x, w.size.y * 0.5f, w.pos.z};
    const std::string name = "NQWarehouse" + std::to_string(wi++);
    add_solid_box(scene, mesh, name.c_str(), pos, w.size, bm);
  }

  // Cranes as boxes — mast + horizontal boom
  auto* mast = scene.add_mesh(
      fury::make_box({1.6f, 18.f, 1.6f}, Vec3{0.85f, 0.55f, 0.12f}));
  auto* boom = scene.add_mesh(
      fury::make_box({14.f, 1.2f, 1.4f}, Vec3{0.90f, 0.60f, 0.15f}));
  Material crane_mat;
  crane_mat.albedo = {0.95f, 0.62f, 0.12f};
  crane_mat.metallic = 0.7f;
  crane_mat.roughness = 0.4f;
  const Vec3 crane_bases[] = {{ox + 2.f, 0.f, oz + 2.f},
                              {ox - 4.f, 0.f, oz - 14.f}};
  int ci = 0;
  for (const Vec3& b : crane_bases) {
    add_solid_box(scene, mast, ("NQCraneMast" + std::to_string(ci)).c_str(),
                  {b.x, 9.f, b.z}, {1.6f, 18.f, 1.6f}, crane_mat);
    add_prop(scene, boom, ("NQCraneBoom" + std::to_string(ci)).c_str(),
             {b.x + 6.5f, 16.5f, b.z}, crane_mat);
    ++ci;
  }

  // Container stacks (colored boxes)
  auto place_stack = [&](float x, float z, const Vec3& rgb, int cols, int rows,
                         int tiers) {
    auto* box = scene.add_mesh(
        fury::make_box({2.4f, 2.2f, 2.0f}, rgb));
    Material cm;
    cm.albedo = rgb;
    cm.metallic = 0.55f;
    cm.roughness = 0.45f;
    cm.texture = TextureSlot::Metal;
    for (int t = 0; t < tiers; ++t) {
      for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
          const float px = x + static_cast<float>(c) * 2.55f;
          const float pz = z + static_cast<float>(r) * 2.15f;
          const float py = 1.1f + static_cast<float>(t) * 2.25f;
          add_solid_box(scene, box, "NQContainer", {px, py, pz},
                        {2.4f, 2.2f, 2.0f}, cm);
        }
      }
    }
  };
  place_stack(ox - 6.f, oz + 4.f, {0.15f, 0.45f, 0.75f}, 3, 2, 3);
  place_stack(ox + 6.f, oz + 6.f, {0.75f, 0.22f, 0.18f}, 2, 2, 2);
  place_stack(ox + 4.f, oz - 2.f, {0.20f, 0.55f, 0.35f}, 2, 1, 2);

  // Tier-1 heist-lite target: sealed high-value container face
  auto* sealed = scene.add_mesh(
      fury::make_box({2.6f, 2.4f, 2.2f}, Vec3{0.85f, 0.72f, 0.20f}));
  Material sealed_mat;
  sealed_mat.albedo = {1.15f, 0.95f, 0.35f};
  sealed_mat.metallic = 0.88f;
  sealed_mat.roughness = 0.25f;
  sealed_mat.emissive = 0.35f;
  {
    Entity t;
    t.name = "NorthQuaySealedContainer";
    t.tag = "vault_container";
    t.mesh = sealed;
    t.transform.position = {ox - 2.f, 1.2f, oz + 16.f};
    t.material = sealed_mat;
    t.solid = true;
    t.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, {2.6f, 2.4f, 2.2f});
    scene.add_entity(std::move(t));
  }

  // Water tongue north of yard
  auto* water = scene.add_mesh(
      fury::make_plane(56.f, 22.f, Vec3{0.12f, 0.32f, 0.52f}, 8.f));
  {
    Entity w;
    w.name = "NorthQuayWater";
    w.mesh = water;
    w.transform.position = {ox + 4.f, -0.4f, oz + 28.f};
    w.material.texture = TextureSlot::Water;
    w.material.roughness = 0.18f;
    w.material.metallic = 0.45f;
    w.material.albedo = {0.7f, 0.92f, 1.12f};
    w.material.uv_scroll_u = 0.04f;
    w.material.uv_scroll_v = 0.025f;
    scene.add_entity(std::move(w));
  }

  // District lamps + sign
  auto* pole = scene.add_mesh(
      fury::make_box({0.22f, 4.4f, 0.22f}, Vec3{0.12f, 0.12f, 0.12f}));
  auto* lamp = scene.add_mesh(
      fury::make_box({0.75f, 0.28f, 0.75f}, Vec3{0.95f, 0.90f, 0.55f}));
  const Vec3 nq_lamps[] = {
      {18.f, 0.f, 58.f}, {18.f, 0.f, 78.f}, {ox - 8.f, 0.f, oz},
      {ox + 12.f, 0.f, oz}, {ox, 0.f, oz + 18.f},
  };
  for (const Vec3& p : nq_lamps) {
    place_lamp(scene, pole, lamp, p.x, p.z);
  }

  auto* sign = scene.add_mesh(
      fury::make_box({7.f, 2.0f, 0.35f}, Vec3{0.20f, 0.30f, 0.40f}));
  Material sign_mat;
  sign_mat.albedo = {0.55f, 0.85f, 0.95f};
  sign_mat.emissive = 0.8f;
  sign_mat.roughness = 0.9f;
  add_prop(scene, sign, "NorthQuaySign", {ox, 3.0f, oz - 18.f}, sign_mat);
}

void build_harbor_metro(fury::Scene& scene) {
  auto* asphalt = scene.add_mesh(
      fury::make_plane(320.f, 260.f, Vec3{0.22f, 0.22f, 0.24f}, 36.f));
  {
    Entity ground;
    ground.name = "StreetGrid";
    ground.tag = "asphalt";
    ground.mesh = asphalt;
    ground.material.texture = TextureSlot::Asphalt;
    ground.material.roughness = 0.85f;
    ground.material.albedo = {0.95f, 0.95f, 0.98f};
    scene.add_entity(std::move(ground));
  }

  auto* sidewalk = scene.add_mesh(
      fury::make_plane(140.f, 14.f, Vec3{0.42f, 0.41f, 0.38f}, 10.f));
  Material concrete_mat;
  concrete_mat.texture = TextureSlot::Concrete;
  concrete_mat.roughness = 0.75f;

  for (float z : {-42.f, -28.f, 0.f, 28.f, 42.f}) {
    Entity sw;
    sw.name = "Sidewalk";
    sw.mesh = sidewalk;
    sw.transform.position = {0.f, 0.03f, z};
    sw.material = concrete_mat;
    scene.add_entity(std::move(sw));
  }
  auto* sidewalk_ns = scene.add_mesh(
      fury::make_plane(14.f, 140.f, Vec3{0.42f, 0.41f, 0.38f}, 10.f));
  for (float x : {-42.f, -28.f, 0.f, 28.f, 42.f}) {
    Entity sw;
    sw.name = "SidewalkNS";
    sw.mesh = sidewalk_ns;
    sw.transform.position = {x, 0.04f, 0.f};
    sw.material = concrete_mat;
    scene.add_entity(std::move(sw));
  }

  auto* plaza = scene.add_mesh(
      fury::make_plane(32.f, 24.f, Vec3{0.48f, 0.46f, 0.42f}, 6.f));
  {
    Entity plaza_e;
    plaza_e.name = "BankPlaza";
    plaza_e.mesh = plaza;
    plaza_e.transform.position = {0.f, 0.05f, -8.f};
    plaza_e.material = concrete_mat;
    plaza_e.material.albedo = {1.05f, 1.02f, 0.95f};
    scene.add_entity(std::move(plaza_e));
  }

  build_meridian_mutual(scene);
  build_crown_cutler(scene);

  struct BldgSpec {
    Vec3 pos;
    Vec3 size;
    Vec3 top;
    Vec3 side;
  };
  const BldgSpec buildings[] = {
      {{-38.f, 0.f, -6.f}, {8.f, 7.f, 9.f}, {0.50f, 0.48f, 0.42f}, {0.38f, 0.36f, 0.32f}},
      {{22.f, 0.f, -6.f}, {12.f, 14.f, 10.f}, {0.38f, 0.44f, 0.55f}, {0.28f, 0.32f, 0.40f}},
      {{40.f, 0.f, -10.f}, {10.f, 11.f, 12.f}, {0.42f, 0.40f, 0.48f}, {0.30f, 0.28f, 0.35f}},
      {{-20.f, 0.f, 22.f}, {11.f, 8.f, 8.f}, {0.55f, 0.50f, 0.38f}, {0.40f, 0.36f, 0.28f}},
      {{18.f, 0.f, 20.f}, {9.f, 6.f, 9.f}, {0.48f, 0.52f, 0.50f}, {0.35f, 0.38f, 0.36f}},
      {{-40.f, 0.f, 16.f}, {10.f, 10.f, 8.f}, {0.45f, 0.40f, 0.42f}, {0.32f, 0.28f, 0.30f}},
      {{38.f, 0.f, 18.f}, {11.f, 13.f, 9.f}, {0.35f, 0.42f, 0.50f}, {0.25f, 0.30f, 0.36f}},
      {{-22.f, 0.f, -32.f}, {9.f, 7.f, 8.f}, {0.58f, 0.48f, 0.42f}, {0.42f, 0.34f, 0.30f}},
      {{20.f, 0.f, -34.f}, {10.f, 9.f, 9.f}, {0.40f, 0.45f, 0.52f}, {0.30f, 0.34f, 0.40f}},
      {{0.f, 0.f, 36.f}, {14.f, 5.f, 8.f}, {0.52f, 0.50f, 0.45f}, {0.38f, 0.36f, 0.32f}},
      {{-36.f, 0.f, -30.f}, {8.f, 12.f, 8.f}, {0.32f, 0.36f, 0.42f}, {0.24f, 0.26f, 0.32f}},
      {{48.f, 0.f, 6.f}, {9.f, 8.f, 10.f}, {0.46f, 0.40f, 0.36f}, {0.34f, 0.30f, 0.26f}},
      {{-50.f, 0.f, 4.f}, {8.f, 9.f, 9.f}, {0.40f, 0.46f, 0.50f}, {0.28f, 0.32f, 0.36f}},
      {{52.f, 0.f, -28.f}, {10.f, 15.f, 8.f}, {0.30f, 0.34f, 0.40f}, {0.22f, 0.24f, 0.30f}},
      {{-48.f, 0.f, -18.f}, {9.f, 6.f, 10.f}, {0.55f, 0.42f, 0.38f}, {0.40f, 0.30f, 0.28f}},
      {{8.f, 0.f, -48.f}, {12.f, 7.f, 8.f}, {0.44f, 0.48f, 0.42f}, {0.32f, 0.34f, 0.30f}},
      {{-8.f, 0.f, 50.f}, {10.f, 6.f, 7.f}, {0.50f, 0.45f, 0.40f}, {0.36f, 0.32f, 0.28f}},
      {{30.f, 0.f, 48.f}, {8.f, 9.f, 8.f}, {0.36f, 0.40f, 0.48f}, {0.26f, 0.28f, 0.34f}},
      {{-30.f, 0.f, 40.f}, {9.f, 11.f, 9.f}, {0.42f, 0.38f, 0.44f}, {0.30f, 0.28f, 0.32f}},
      {{55.f, 0.f, 32.f}, {11.f, 10.f, 9.f}, {0.33f, 0.38f, 0.45f}, {0.24f, 0.28f, 0.34f}},
      // denser 0.8.0 fill-ins — varied heights / warm-cool facades
      {{-58.f, 0.f, 22.f}, {7.f, 16.f, 7.f}, {0.62f, 0.38f, 0.32f}, {0.45f, 0.26f, 0.22f}},
      {{62.f, 0.f, -8.f}, {8.f, 18.f, 8.f}, {0.28f, 0.34f, 0.48f}, {0.18f, 0.22f, 0.34f}},
      {{-14.f, 0.f, -55.f}, {9.f, 4.f, 10.f}, {0.70f, 0.62f, 0.45f}, {0.52f, 0.46f, 0.34f}},
      {{14.f, 0.f, 58.f}, {8.f, 20.f, 8.f}, {0.25f, 0.40f, 0.42f}, {0.16f, 0.28f, 0.30f}},
      {{-62.f, 0.f, -36.f}, {10.f, 5.f, 9.f}, {0.48f, 0.55f, 0.38f}, {0.34f, 0.40f, 0.28f}},
      {{44.f, 0.f, -48.f}, {7.f, 13.f, 7.f}, {0.58f, 0.32f, 0.40f}, {0.42f, 0.22f, 0.28f}},
      {{-44.f, 0.f, 52.f}, {12.f, 8.f, 7.f}, {0.34f, 0.48f, 0.55f}, {0.24f, 0.34f, 0.40f}},
      {{68.f, 0.f, 18.f}, {9.f, 6.f, 11.f}, {0.72f, 0.55f, 0.30f}, {0.50f, 0.38f, 0.22f}},
  };

  auto* win_strip = scene.add_mesh(
      fury::make_box({1.f, 1.f, 1.f}, Vec3{0.55f, 0.75f, 1.0f}));
  Material win_mat;
  win_mat.albedo = {0.7f, 0.9f, 1.2f};
  win_mat.roughness = 0.22f;
  win_mat.metallic = 0.12f;
  win_mat.emissive = 0.2f;  // scaled by night via tag "window"
  win_mat.texture = TextureSlot::Glass;

  int bi = 0;
  int wi = 0;
  for (const BldgSpec& spec : buildings) {
    auto* mesh = scene.add_mesh(
        fury::make_colored_box(spec.size, spec.top, spec.side));
    Material bm;
    // Mix brick / concrete / metal facades across the district
    const int face = (bi * 17) % 5;
    if (face == 0 || face == 3) {
      bm.texture = TextureSlot::Brick;
      bm.roughness = 0.62f;
    } else if (face == 4) {
      bm.texture = TextureSlot::Metal;
      bm.metallic = 0.55f;
      bm.roughness = 0.4f;
    } else {
      bm.texture = TextureSlot::Concrete;
      bm.roughness = 0.55f + 0.25f * static_cast<float>(face) / 4.f;
    }
    bm.albedo = {1.f, 1.f, 1.f};
    const Vec3 pos{spec.pos.x, spec.size.y * 0.5f, spec.pos.z};
    const std::string bname = "Bldg" + std::to_string(bi++);
    add_solid_box(scene, mesh, bname.c_str(), pos, spec.size, bm);

    // Night window emissive strips on +Z / +X faces
    const float hy = spec.size.y;
    for (float y = 1.6f; y < hy - 0.8f; y += 2.4f) {
      Entity w;
      w.name = "WinZ" + std::to_string(wi);
      w.tag = "window";
      w.mesh = win_strip;
      w.transform.position = {pos.x, y, pos.z + spec.size.z * 0.5f + 0.06f};
      w.transform.scale = {spec.size.x * 0.72f, 0.35f, 0.08f};
      w.material = win_mat;
      scene.add_entity(std::move(w));
      ++wi;
      Entity wx;
      wx.name = "WinX" + std::to_string(wi);
      wx.tag = "window";
      wx.mesh = win_strip;
      wx.transform.position = {pos.x + spec.size.x * 0.5f + 0.06f, y, pos.z};
      wx.transform.scale = {0.08f, 0.35f, spec.size.z * 0.72f};
      wx.material = win_mat;
      scene.add_entity(std::move(wx));
      ++wi;
    }
  }

  // Waterfront with animated UV scroll
  auto* water = scene.add_mesh(
      fury::make_plane(90.f, 36.f, Vec3{0.15f, 0.35f, 0.55f}, 10.f));
  {
    Entity w;
    w.name = "HarborWater";
    w.mesh = water;
    w.transform.position = {20.f, -0.35f, 56.f};
    w.material.texture = TextureSlot::Water;
    w.material.roughness = 0.18f;
    w.material.metallic = 0.45f;
    w.material.albedo = {0.75f, 0.95f, 1.15f};
    w.material.uv_scroll_u = 0.05f;
    w.material.uv_scroll_v = 0.028f;
    scene.add_entity(std::move(w));
  }

  auto* pier = scene.add_mesh(
      fury::make_box({48.f, 0.5f, 8.f}, Vec3{0.40f, 0.32f, 0.22f}));
  Material wood;
  wood.roughness = 0.8f;
  wood.albedo = {1.f, 0.95f, 0.85f};
  add_prop(scene, pier, "PierDeck", {15.f, 0.25f, 44.f}, wood);

  auto* pier_post = scene.add_mesh(
      fury::make_box({0.6f, 3.f, 0.6f}, Vec3{0.30f, 0.24f, 0.16f}));
  for (float x = -6.f; x <= 36.f; x += 6.f) {
    add_prop(scene, pier_post, "PierPost", {x, -0.5f, 47.5f}, wood);
  }

  auto* crate = scene.add_mesh(
      fury::make_box({1.6f, 1.6f, 1.6f}, Vec3{0.55f, 0.40f, 0.22f}));
  Material crate_mat;
  crate_mat.roughness = 0.75f;
  crate_mat.texture = TextureSlot::Checker;
  add_solid_box(scene, crate, "CrateA", {10.f, 1.0f, 43.f}, {1.6f, 1.6f, 1.6f},
                crate_mat);
  add_solid_box(scene, crate, "CrateB", {12.f, 1.0f, 44.5f}, {1.6f, 1.6f, 1.6f},
                crate_mat);
  add_solid_box(scene, crate, "CrateC", {8.f, 1.0f, 45.f}, {1.6f, 1.6f, 1.6f},
                crate_mat);

  auto* escape_mesh = scene.add_mesh(
      fury::make_box({7.f, 0.25f, 5.f}, Vec3{0.18f, 0.70f, 0.28f}));
  Material escape_mat;
  escape_mat.albedo = {0.7f, 1.2f, 0.7f};
  escape_mat.roughness = 0.9f;
  escape_mat.emissive = 0.2f;
  {
    Entity escape;
    escape.name = "ExtractionPad";
    escape.tag = "escape";
    escape.mesh = escape_mesh;
    escape.transform.position = {34.f, 0.15f, 30.f};
    escape.material = escape_mat;
    scene.add_entity(std::move(escape));
  }

  // 3.3.0 — getaway van cab + cargo bed + headlights (driveable)
  auto* van_bed = scene.add_mesh(
      fury::make_box({2.8f, 2.0f, 2.15f}, Vec3{0.14f, 0.16f, 0.18f}));
  auto* van_cab = scene.add_mesh(
      fury::make_box({1.7f, 1.55f, 2.05f}, Vec3{0.18f, 0.20f, 0.24f}));
  auto* van_glass = scene.add_mesh(
      fury::make_box({0.22f, 0.85f, 1.75f}, Vec3{0.35f, 0.60f, 0.75f}));
  auto* van_head = scene.add_mesh(
      fury::make_box({0.22f, 0.28f, 0.35f}, Vec3{1.0f, 0.95f, 0.70f}));
  Material van_mat;
  van_mat.metallic = 0.62f;
  van_mat.roughness = 0.38f;
  van_mat.albedo = {0.92f, 0.93f, 0.96f};
  van_mat.texture = TextureSlot::Metal;
  Material van_cab_mat = van_mat;
  van_cab_mat.albedo = {0.88f, 0.90f, 0.94f};
  Material glass_mat;
  glass_mat.metallic = 0.15f;
  glass_mat.roughness = 0.18f;
  glass_mat.albedo = {0.45f, 0.70f, 0.88f};
  glass_mat.emissive = 0.08f;
  glass_mat.texture = TextureSlot::Glass;
  Material head_mat;
  head_mat.albedo = {1.0f, 0.96f, 0.75f};
  head_mat.roughness = 0.85f;
  head_mat.emissive = 0.05f;  // brightens at night while driving
  {
    Entity bed;
    bed.name = "GetawayVanBed";
    bed.tag = "vehicle";
    bed.mesh = van_bed;
    bed.transform.position = {34.f - 0.55f, 1.15f, 33.5f};
    bed.material = van_mat;
    bed.solid = false;
    scene.add_entity(std::move(bed));
  }
  {
    Entity cab;
    cab.name = "GetawayVanCab";
    cab.tag = "vehicle_part";
    cab.mesh = van_cab;
    cab.transform.position = {34.f + 1.55f, 1.25f, 33.5f};
    cab.material = van_cab_mat;
    cab.solid = false;
    scene.add_entity(std::move(cab));
  }
  add_prop(scene, van_glass, "GetawayVanGlass", {34.f + 2.35f, 1.55f, 33.5f},
           glass_mat);
  {
    Entity hl;
    hl.name = "GetawayVanHeadL";
    hl.tag = "headlight";
    hl.mesh = van_head;
    hl.transform.position = {34.f + 2.45f, 0.95f, 33.5f - 0.72f};
    hl.material = head_mat;
    scene.add_entity(std::move(hl));
  }
  {
    Entity hr;
    hr.name = "GetawayVanHeadR";
    hr.tag = "headlight";
    hr.mesh = van_head;
    hr.transform.position = {34.f + 2.45f, 0.95f, 33.5f + 0.72f};
    hr.material = head_mat;
    scene.add_entity(std::move(hr));
  }

  // 3.3.0 — stealable civilian sedan near Ashcourt Market (F when close)
  auto* civ_body = scene.add_mesh(
      fury::make_box({3.6f, 1.15f, 1.85f}, Vec3{0.12f, 0.42f, 0.48f}));
  auto* civ_cabin = scene.add_mesh(
      fury::make_box({1.55f, 0.95f, 1.65f}, Vec3{0.28f, 0.52f, 0.62f}));
  auto* civ_head = scene.add_mesh(
      fury::make_box({0.20f, 0.24f, 0.32f}, Vec3{1.0f, 0.95f, 0.70f}));
  Material civ_mat;
  civ_mat.metallic = 0.58f;
  civ_mat.roughness = 0.40f;
  civ_mat.albedo = {0.14f, 0.46f, 0.50f};
  civ_mat.texture = TextureSlot::Metal;
  Material civ_cab_mat;
  civ_cab_mat.metallic = 0.12f;
  civ_cab_mat.roughness = 0.22f;
  civ_cab_mat.albedo = {0.32f, 0.55f, 0.68f};
  civ_cab_mat.emissive = 0.06f;
  civ_cab_mat.texture = TextureSlot::Glass;
  Material civ_head_mat = head_mat;
  const Vec3 civ_spawn{-82.f, 0.85f, 38.f};
  {
    Entity body;
    body.name = "CivSedanBody";
    body.tag = "stealable";
    body.mesh = civ_body;
    body.transform.position = civ_spawn;
    body.transform.rotation_euler = {0.f, 1.5707963f, 0.f};
    body.material = civ_mat;
    body.solid = false;
    scene.add_entity(std::move(body));
  }
  {
    Entity cab;
    cab.name = "CivSedanCabin";
    cab.tag = "vehicle_part";
    cab.mesh = civ_cabin;
    cab.transform.position = {civ_spawn.x + 0.15f, civ_spawn.y + 0.55f, civ_spawn.z};
    cab.transform.rotation_euler = {0.f, 1.5707963f, 0.f};
    cab.material = civ_cab_mat;
    cab.solid = false;
    scene.add_entity(std::move(cab));
  }
  {
    Entity hl;
    hl.name = "CivSedanHeadL";
    hl.tag = "headlight";
    hl.mesh = civ_head;
    hl.transform.position = {civ_spawn.x + 1.75f, civ_spawn.y - 0.15f,
                             civ_spawn.z - 0.62f};
    hl.transform.rotation_euler = {0.f, 1.5707963f, 0.f};
    hl.material = civ_head_mat;
    scene.add_entity(std::move(hl));
  }
  {
    Entity hr;
    hr.name = "CivSedanHeadR";
    hr.tag = "headlight";
    hr.mesh = civ_head;
    hr.transform.position = {civ_spawn.x + 1.75f, civ_spawn.y - 0.15f,
                             civ_spawn.z + 0.62f};
    hr.transform.rotation_euler = {0.f, 1.5707963f, 0.f};
    hr.material = civ_head_mat;
    scene.add_entity(std::move(hr));
  }

  // Extra street props near extraction
  auto* bollard = scene.add_mesh(
      fury::make_box({0.35f, 1.0f, 0.35f}, Vec3{0.55f, 0.55f, 0.50f}));
  Material bollard_mat;
  bollard_mat.metallic = 0.4f;
  bollard_mat.roughness = 0.5f;
  for (float z = 26.f; z <= 32.f; z += 2.f) {
    add_solid_box(scene, bollard, "Bollard", {30.f, 0.5f, z},
                  {0.35f, 1.0f, 0.35f}, bollard_mat);
  }
  auto* bench = scene.add_mesh(
      fury::make_box({2.2f, 0.45f, 0.7f}, Vec3{0.35f, 0.28f, 0.20f}));
  Material bench_mat;
  bench_mat.roughness = 0.7f;
  add_solid_box(scene, bench, "StreetBenchA", {16.f, 0.35f, 12.f},
                {2.2f, 0.45f, 0.7f}, bench_mat);
  add_solid_box(scene, bench, "StreetBenchB", {-16.f, 0.35f, 14.f},
                {2.2f, 0.45f, 0.7f}, bench_mat);
  auto* trash = scene.add_mesh(
      fury::make_box({0.7f, 1.1f, 0.7f}, Vec3{0.25f, 0.28f, 0.22f}));
  Material trash_mat;
  trash_mat.metallic = 0.5f;
  trash_mat.roughness = 0.45f;
  add_solid_box(scene, trash, "TrashCanA", {12.f, 0.55f, 6.f},
                {0.7f, 1.1f, 0.7f}, trash_mat);
  add_solid_box(scene, trash, "TrashCanB", {-10.f, 0.55f, 18.f},
                {0.7f, 1.1f, 0.7f}, trash_mat);
  add_solid_box(scene, trash, "TrashCanC", {40.f, 0.55f, 24.f},
                {0.7f, 1.1f, 0.7f}, trash_mat);

  auto* dumpster = scene.add_mesh(
      fury::make_box({2.2f, 1.4f, 1.4f}, Vec3{0.20f, 0.45f, 0.22f}));
  Material dump_mat;
  dump_mat.metallic = 0.55f;
  dump_mat.roughness = 0.5f;
  add_solid_box(scene, dumpster, "Dumpster", {28.f, 0.7f, 26.f},
                {2.2f, 1.4f, 1.4f}, dump_mat);
  add_solid_box(scene, dumpster, "Dumpster2", {26.f, 0.7f, 28.f},
                {2.2f, 1.4f, 1.4f}, dump_mat);

  // 1.1.0 world props polish — crates, barriers, planters, street signs
  auto* polish_crate = scene.add_mesh(
      fury::make_box({1.1f, 1.1f, 1.1f}, Vec3{0.55f, 0.42f, 0.28f}));
  Material polish_crate_mat;
  polish_crate_mat.roughness = 0.8f;
  polish_crate_mat.albedo = {1.05f, 0.95f, 0.8f};
  add_solid_box(scene, polish_crate, "PolishCrateA", {22.f, 0.55f, 24.f},
                {1.1f, 1.1f, 1.1f}, polish_crate_mat);
  add_solid_box(scene, polish_crate, "PolishCrateB", {23.3f, 0.55f, 24.4f},
                {1.1f, 1.1f, 1.1f}, polish_crate_mat);
  add_solid_box(scene, polish_crate, "PolishCrateC", {22.6f, 1.65f, 24.2f},
                {1.1f, 1.1f, 1.1f}, polish_crate_mat);
  add_solid_box(scene, polish_crate, "PolishCratePlaza", {-8.f, 0.55f, 6.f},
                {1.1f, 1.1f, 1.1f}, polish_crate_mat);

  auto* barrier = scene.add_mesh(
      fury::make_box({2.4f, 1.05f, 0.35f}, Vec3{0.85f, 0.55f, 0.12f}));
  Material barrier_mat;
  barrier_mat.roughness = 0.55f;
  barrier_mat.metallic = 0.15f;
  add_solid_box(scene, barrier, "BarrierA", {31.5f, 0.55f, 28.f},
                {2.4f, 1.05f, 0.35f}, barrier_mat);
  add_solid_box(scene, barrier, "BarrierB", {31.5f, 0.55f, 31.f},
                {2.4f, 1.05f, 0.35f}, barrier_mat);
  add_solid_box(scene, barrier, "BarrierPlaza", {6.f, 0.55f, -2.f},
                {2.4f, 1.05f, 0.35f}, barrier_mat);

  auto* planter = scene.add_mesh(
      fury::make_box({1.6f, 0.7f, 1.6f}, Vec3{0.40f, 0.32f, 0.28f}));
  Material planter_mat;
  planter_mat.roughness = 0.75f;
  add_solid_box(scene, planter, "PlanterA", {-12.f, 0.35f, 4.f},
                {1.6f, 0.7f, 1.6f}, planter_mat);
  add_solid_box(scene, planter, "PlanterB", {12.f, 0.35f, 4.f},
                {1.6f, 0.7f, 1.6f}, planter_mat);
  auto* shrub = scene.add_mesh(
      fury::make_box({1.2f, 1.1f, 1.2f}, Vec3{0.18f, 0.48f, 0.22f}));
  Material shrub_mat;
  shrub_mat.roughness = 0.9f;
  add_prop(scene, shrub, "PlanterShrubA", {-12.f, 1.15f, 4.f}, shrub_mat);
  add_prop(scene, shrub, "PlanterShrubB", {12.f, 1.15f, 4.f}, shrub_mat);

  auto* sign_post = scene.add_mesh(
      fury::make_box({0.12f, 2.8f, 0.12f}, Vec3{0.2f, 0.2f, 0.22f}));
  auto* sign_board = scene.add_mesh(
      fury::make_box({1.6f, 0.9f, 0.1f}, Vec3{0.15f, 0.35f, 0.55f}));
  Material sign_mat;
  sign_mat.roughness = 0.5f;
  sign_mat.emissive = 0.08f;
  add_prop(scene, sign_post, "StreetSignPost", {8.f, 1.4f, 10.f}, sign_mat);
  add_prop(scene, sign_board, "StreetSignBoard", {8.f, 2.6f, 10.f}, sign_mat);
  add_prop(scene, sign_post, "ExtractSignPost", {36.f, 1.4f, 26.f}, sign_mat);
  Material extract_sign = sign_mat;
  extract_sign.albedo = {0.2f, 0.75f, 0.35f};
  extract_sign.emissive = 0.25f;
  add_prop(scene, sign_board, "ExtractSignBoard", {36.f, 2.6f, 26.f}, extract_sign);

  add_solid_box(scene, bench, "StreetBenchC", {10.f, 0.35f, -14.f},
                {2.2f, 0.45f, 0.7f}, bench_mat);
  add_solid_box(scene, bench, "StreetBenchD", {-10.f, 0.35f, -12.f},
                {2.2f, 0.45f, 0.7f}, bench_mat);
  add_solid_box(scene, trash, "TrashCanD", {-6.f, 0.55f, -10.f},
                {0.7f, 1.1f, 0.7f}, trash_mat);
  add_solid_box(scene, trash, "TrashCanE", {38.f, 0.55f, 34.f},
                {0.7f, 1.1f, 0.7f}, trash_mat);


  auto* pole = scene.add_mesh(
      fury::make_box({0.22f, 4.4f, 0.22f}, Vec3{0.12f, 0.12f, 0.12f}));
  auto* lamp = scene.add_mesh(
      fury::make_box({0.75f, 0.28f, 0.75f}, Vec3{0.95f, 0.90f, 0.55f}));
  const Vec3 lamp_pts[] = {
      {-14.f, 0.f, 8.f},  {14.f, 0.f, 8.f},   {-14.f, 0.f, -20.f},
      {14.f, 0.f, -20.f}, {-14.f, 0.f, 28.f}, {14.f, 0.f, 28.f},
      {-42.f, 0.f, 0.f},  {42.f, 0.f, 0.f},   {30.f, 0.f, 40.f},
      {8.f, 0.f, 40.f},   {-30.f, 0.f, -16.f},{30.f, 0.f, -16.f},
      {-22.f, 0.f, 14.f}, {0.f, 0.f, -40.f},  {48.f, 0.f, 20.f},
      {-48.f, 0.f, 24.f},
  };
  for (const Vec3& p : lamp_pts) {
    place_lamp(scene, pole, lamp, p.x, p.z);
  }

  build_ridge_pier(scene);
  build_ashcourt_market(scene);
  build_harbor_armored_depot(scene);
  build_harbor_loft(scene);
  build_north_quay(scene);

  // 2.1.0 denser streets — mid-block props, parked cars, neon, rooftop AC
  build_harbor_density(scene);
  build_ridge_density(scene);
  build_ashcourt_density(scene);

  // 3.7.0 wet-street puddles (Harbor + Ashcourt)
  place_street_puddles(scene);

  // 2.8.0 LOD stub — shared box proxy for some detail props (others skip beyond mid)
  auto* lod_box = scene.add_mesh(
      fury::make_box({0.85f, 0.85f, 0.85f}, Vec3{0.45f, 0.45f, 0.48f}));
  for (auto& e : scene.entities()) {
    if (!e.detail || e.lod_mesh) {
      continue;
    }
    if (e.name == "ParkedCarCabin") {
      e.lod_mesh = lod_box;
    }
  }
}

void draw_hud_bars(fury::Renderer& r, const fury::HeistController& heist,
                   const fury::HeatMeter& heat, bool in_vehicle, int win_w,
                   int win_h, const fury::MissionBoard& board,
                   const Vec3& player_pos, const Vec3& objective_pos,
                   int crew_nearby, bool buy_open, const fury::PlayerPerks& perks,
                   int save_slot, bool near_shop, float splash_t,
                   float banner_t, bool banner_success, int onboard_step,
                   float player_yaw, bool show_fps, float fps,
                   const fury::QuestJournal& journal, float banter_t,
                   const char* banter_line, bool alarm_active,
                   bool local_ready,
                   const std::vector<fury::net::CrewAssignment>& crew_roster,
                   const std::vector<fury::net::PlayerState>& remotes,
                   const std::vector<fury::net::ChatLine>& chat_log,
                   bool chat_open, const std::string& chat_buffer,
                   bool inv_open, int sell_selected, int pursuit_count,
                   bool in_safehouse, bool rep_open,
                   const fury::FactionReputations& reps, bool ending_banner,
                   bool cutscene_active, bool finale_locked, bool help_open,
                   bool door_enter_tip, const char* interior_tag,
                   bool skills_open, const fury::SkillTree& skills,
                   const fury::DailyContracts& daily, float run_peak_heat,
                   bool lobby_open, bool is_net_host,
                   bool nameplate_show, fury::DialogueRole nameplate_role,
                   float nameplate_fill, float dialogue_t, int dialogue_lines,
                   fury::DialogueRole dialogue_role, int radio_station,
                   bool map_open, int map_focus, float ft_cooldown,
                   bool can_fast_travel, float visibility,
                   bool crouching, bool breaker_tip,
                   bool craft_open, bool near_workbench,
                   const fury::CraftInventory& craft,
                   const fury::FenceUpgrades& fence_up) {
  const float W = static_cast<float>(win_w);
  const float H = static_cast<float>(win_h);

  // --- Title splash (first ~1.5s): stylized "VAULTLINE" bar plate -------------
  if (splash_t > 0.f) {
    const float a = std::clamp(splash_t / 0.35f, 0.f, 1.f);  // fade last 0.35s via remaining
    const float fade = splash_t > 0.35f ? 1.f : (splash_t / 0.35f);
    (void)a;
    const std::uint8_t alpha = static_cast<std::uint8_t>(200 * fade);
    r.draw_hud_rect(0.f, 0.f, W, H, Color{6, 10, 18, static_cast<std::uint8_t>(180 * fade)});
    const float cx = W * 0.5f;
    const float cy = H * 0.42f;
    // Outer plate
    r.draw_hud_rect(cx - 280.f, cy - 70.f, 560.f, 140.f, Color{12, 18, 28, alpha});
    r.draw_hud_rect(cx - 270.f, cy - 60.f, 540.f, 120.f, Color{20, 32, 48, alpha});
    // Accent bars spelling a geometric VAULTLINE title (9 letter slots)
    const float letter_w = 48.f;
    const float gap = 8.f;
    const float total = 9.f * letter_w + 8.f * gap;
    float x0 = cx - total * 0.5f;
    const Color gold{255, 200, 70, static_cast<std::uint8_t>(240 * fade)};
    const Color bar{230, 210, 140, static_cast<std::uint8_t>(220 * fade)};
    auto letter = [&](int i, bool top, bool mid, bool bot, bool left, bool right,
                      bool upright = false) {
      const float x = x0 + static_cast<float>(i) * (letter_w + gap);
      const float y = cy - 36.f;
      if (top) r.draw_hud_rect(x, y, letter_w, 10.f, gold);
      if (mid) r.draw_hud_rect(x + 4.f, y + 28.f, letter_w - 8.f, 8.f, bar);
      if (bot) r.draw_hud_rect(x, y + 56.f, letter_w, 10.f, gold);
      if (left) r.draw_hud_rect(x, y, 10.f, 66.f, gold);
      if (right) r.draw_hud_rect(x + letter_w - 10.f, y, 10.f, 66.f, gold);
      if (upright) r.draw_hud_rect(x + letter_w * 0.5f - 5.f, y, 10.f, 66.f, gold);
    };
    // V A U L T L I N E  (approximate block letters)
    letter(0, false, false, false, true, true);           // V-ish via sides
    r.draw_hud_rect(x0 + 10.f, cy + 20.f, letter_w - 20.f, 10.f, gold);  // V bottom tip bar
    letter(1, true, true, false, true, true);             // A
    letter(2, true, false, true, true, true);             // U
    letter(3, false, false, false, true, false);          // L
    r.draw_hud_rect(x0 + 3.f * (letter_w + gap), cy + 20.f, letter_w, 10.f, gold);
    letter(4, true, false, false, true, false);           // T
    r.draw_hud_rect(x0 + 4.f * (letter_w + gap) + letter_w * 0.5f - 5.f, cy - 36.f, 10.f, 66.f, gold);
    letter(5, false, false, false, true, false);          // L
    r.draw_hud_rect(x0 + 5.f * (letter_w + gap), cy + 20.f, letter_w, 10.f, gold);
    letter(6, false, false, false, false, false, true);   // I
    letter(7, true, false, false, true, true);            // N
    r.draw_hud_rect(x0 + 7.f * (letter_w + gap) + 8.f, cy - 26.f, 10.f, 50.f, bar);
    letter(8, true, true, true, true, false);             // E
    // Subtitle bars
    r.draw_hud_rect(cx - 120.f, cy + 90.f, 240.f, 8.f, Color{80, 180, 255, static_cast<std::uint8_t>(200 * fade)});
    r.draw_hud_rect(cx - 80.f, cy + 110.f, 160.f, 6.f, Color{60, 120, 180, static_cast<std::uint8_t>(160 * fade)});
  }

  // Panel background (taller for heat + visibility + crew stub)
  r.draw_hud_rect(16.f, 16.f, 340.f, 148.f, Color{12, 16, 24, 170});
  // Cash bar
  const float cash_t =
      (std::min)(1.f, static_cast<float>(heist.inventory().cash) / 50000.f);
  r.draw_hud_rect(28.f, 28.f, 316.f, 14.f, Color{40, 50, 60, 220});
  r.draw_hud_rect(28.f, 28.f, 316.f * cash_t, 14.f, Color{50, 200, 90, 230});

  // Loot progress
  const float loot_t = heist.loot_progress();
  r.draw_hud_rect(28.f, 50.f, 316.f, 14.f, Color{40, 50, 60, 220});
  Color loot_col{220, 180, 40, 230};
  if (heist.phase() == fury::HeistPhase::Escape) {
    loot_col = Color{80, 180, 255, 230};
  } else if (heist.phase() == fury::HeistPhase::Success) {
    loot_col = Color{90, 255, 140, 230};
  } else if (heist.phase() == fury::HeistPhase::Failed) {
    loot_col = Color{220, 60, 60, 230};
  }
  r.draw_hud_rect(28.f, 50.f, 316.f * (std::max)(loot_t, 0.02f), 14.f, loot_col);

  // Score stub bar
  const float score_t =
      (std::min)(1.f, static_cast<float>(heist.score().lifetime_cash) / 80000.f);
  r.draw_hud_rect(28.f, 72.f, 316.f, 14.f, Color{40, 50, 60, 220});
  r.draw_hud_rect(28.f, 72.f, 316.f * score_t, 14.f, Color{180, 120, 255, 230});

  // Heat / wanted bar
  const float heat_t = heat.normalized();
  r.draw_hud_rect(28.f, 94.f, 316.f, 14.f, Color{40, 50, 60, 220});
  Color heat_col{255, 160, 40, 230};
  if (heat_t > 0.66f) {
    heat_col = Color{255, 50, 50, 240};
  } else if (heat_t > 0.33f) {
    heat_col = Color{255, 120, 30, 230};
  }
  r.draw_hud_rect(28.f, 94.f, 316.f * (std::max)(heat_t, 0.02f), 14.f, heat_col);

  // Visibility / detection bar (guards + cameras)
  const float vis_t = std::clamp(visibility, 0.f, 1.f);
  r.draw_hud_rect(28.f, 116.f, 316.f, 12.f, Color{40, 50, 60, 220});
  Color vis_col{80, 200, 220, 230};
  if (vis_t > 0.66f) {
    vis_col = Color{255, 90, 160, 240};
  } else if (vis_t > 0.33f) {
    vis_col = Color{120, 220, 255, 230};
  }
  if (crouching) {
    vis_col = Color{60, 180, 140, 230};
  }
  r.draw_hud_rect(28.f, 116.f, 316.f * (std::max)(vis_t, 0.02f), 12.f, vis_col);

  // Crew nearby indicator (short bars)
  r.draw_hud_rect(28.f, 134.f, 316.f, 10.f, Color{40, 50, 60, 220});
  if (crew_nearby > 0) {
    r.draw_hud_rect(28.f, 134.f, 158.f * static_cast<float>(crew_nearby), 10.f,
                    Color{90, 180, 255, 230});
  }

  if (in_vehicle) {
    r.draw_hud_rect(16.f, 152.f, 180.f, 22.f, Color{20, 40, 30, 180});
    r.draw_hud_rect(28.f, 158.f, 156.f, 10.f, Color{60, 200, 120, 220});
    // Radio stub pip (C cycles) — 3 station slots, active lit
    r.draw_hud_rect(16.f, 178.f, 180.f, 28.f, Color{18, 24, 36, 190});
    const int st = std::clamp(radio_station, 0, 2);
    for (int i = 0; i < 3; ++i) {
      const bool on = (i == st);
      r.draw_hud_rect(28.f + static_cast<float>(i) * 52.f, 186.f, 44.f, 12.f,
                      on ? Color{255, 180, 70, 240} : Color{50, 70, 95, 210});
    }
  }

  // Mission board (M) — list of jobs with payout tier bars (5th = finale; 6th = North Quay)
  // Anchored below status/ready so it does not cover the left strip.
  if (board.open) {
    r.draw_hud_rect(16.f, 210.f, 360.f, 222.f, Color{10, 14, 22, 210});
    for (int i = 0; i < static_cast<int>(fury::kMissionCount); ++i) {
      const fury::MissionJob& job = fury::mission_job(static_cast<std::size_t>(i));
      const float y = 222.f + static_cast<float>(i) * 34.f;
      const bool sel = (board.selected == i);
      const bool locked = (i == fury::kFinaleMissionIndex && finale_locked);
      r.draw_hud_rect(28.f, y, 336.f, 28.f,
                      locked ? Color{40, 28, 28, 210}
                             : (sel ? Color{40, 70, 110, 230} : Color{28, 34, 48, 210}));
      const float tier_t = (std::min)(1.f, static_cast<float>(job.payout_tier) / 4.f);
      Color tier_col{80, 200, 120, 230};
      if (job.payout_tier >= 4) {
        tier_col = Color{120, 220, 255, 240};
      } else if (job.payout_tier >= 3) {
        tier_col = Color{255, 200, 60, 230};
      } else if (job.payout_tier == 2) {
        tier_col = Color{180, 140, 255, 230};
      }
      if (locked) {
        tier_col = Color{90, 70, 70, 200};
      }
      r.draw_hud_rect(40.f, y + 9.f, 300.f * tier_t, 10.f, tier_col);
    }
  } else if (!buy_open) {
    const fury::MissionJob& job = board.current();
    const float tier_t = (std::min)(1.f, static_cast<float>(job.payout_tier) / 4.f);
    r.draw_hud_rect(16.f, 210.f, 200.f, 18.f, Color{12, 16, 24, 150});
    r.draw_hud_rect(28.f, 214.f, 176.f * tier_t, 10.f,
                    board.is_finale() ? Color{120, 220, 255, 220}
                                      : Color{255, 200, 80, 210});
  }


  // Quest journal (J) — mission list + completion flags (incl. finale)
  // Right column under minimap; mutually exclusive with rep/inv/help.
  if (journal.open) {
    r.draw_hud_rect(W - 390.f, 178.f, 370.f, 246.f, Color{10, 14, 22, 220});
    for (int i = 0; i < static_cast<int>(fury::kMissionCount); ++i) {
      const fury::MissionJob& job = fury::mission_job(static_cast<std::size_t>(i));
      const float y = 190.f + static_cast<float>(i) * 36.f;
      const bool done = journal.complete[i] != 0;
      const bool locked = (i == fury::kFinaleMissionIndex && finale_locked && !done);
      r.draw_hud_rect(W - 378.f, y, 346.f, 30.f,
                      locked ? Color{48, 28, 28, 220}
                             : (done ? Color{28, 55, 40, 230} : Color{28, 34, 48, 220}));
      // completion pip
      r.draw_hud_rect(W - 368.f, y + 8.f, 14.f, 14.f,
                      done ? Color{90, 255, 140, 240}
                           : (locked ? Color{120, 50, 50, 220} : Color{60, 70, 90, 220}));
      const float tier_t = (std::min)(1.f, static_cast<float>(job.payout_tier) / 4.f);
      r.draw_hud_rect(W - 340.f, y + 10.f, 300.f * tier_t, 10.f,
                      done ? Color{90, 220, 140, 230}
                           : (i == fury::kFinaleMissionIndex ? Color{120, 220, 255, 210}
                                                             : Color{255, 200, 80, 210}));
    }
  }
  // Buy/sell menu (B) — Ashcourt fence perks + permanent upgrades + sell chips
  if (buy_open) {
    r.draw_hud_rect(16.f, 210.f, 360.f, 320.f, Color{8, 18, 14, 220});
    const float levels[3] = {
        static_cast<float>(perks.crew),
        static_cast<float>(perks.heat_damp),
        static_cast<float>(perks.loot_speed)};
    const Color cols[3] = {Color{90, 200, 140, 230}, Color{255, 160, 60, 230},
                           Color{120, 180, 255, 230}};
    for (int i = 0; i < 3; ++i) {
      const float y = 222.f + static_cast<float>(i) * 30.f;
      r.draw_hud_rect(28.f, y, 336.f, 26.f,
                      near_shop ? Color{30, 55, 40, 230} : Color{40, 35, 30, 210});
      const float t = (std::min)(1.f, levels[i] / 3.f);
      r.draw_hud_rect(40.f, y + 8.f, 300.f * (std::max)(t, 0.04f), 10.f, cols[i]);
    }
    // Permanent fence unlocks (4 Better Payouts / 5 Quieter Tools)
    {
      const bool ups[2] = {fence_up.better_payouts, fence_up.quieter_tools};
      const Color ucols[2] = {Color{255, 210, 90, 230}, Color{140, 220, 255, 230}};
      for (int i = 0; i < 2; ++i) {
        const float y = 316.f + static_cast<float>(i) * 28.f;
        r.draw_hud_rect(28.f, y, 336.f, 24.f,
                        ups[i] ? Color{40, 60, 40, 230}
                               : (near_shop ? Color{35, 40, 55, 230}
                                            : Color{35, 32, 30, 210}));
        r.draw_hud_rect(40.f, y + 7.f, 300.f * (ups[i] ? 1.f : 0.12f), 10.f, ucols[i]);
      }
    }
    // Sell rows — highlight selected chip; S sells one when near shop
    const Color chip_cols[3] = {Color{220, 200, 90, 230}, Color{80, 160, 255, 230},
                                Color{180, 120, 255, 230}};
    for (int i = 0; i < 3; ++i) {
      const float y = 380.f + static_cast<float>(i) * 30.f;
      const bool sel = (i == sell_selected);
      const int count = heist.inventory().chip_count(static_cast<fury::LootChip>(i));
      r.draw_hud_rect(28.f, y, 336.f, 28.f,
                      sel ? (near_shop ? Color{50, 70, 40, 240} : Color{50, 45, 35, 220})
                          : Color{24, 32, 28, 210});
      const float fill =
          (std::min)(1.f, static_cast<float>(count) / 8.f);
      r.draw_hud_rect(40.f, y + 9.f, 300.f * (std::max)(fill, count > 0 ? 0.08f : 0.04f),
                      10.f, chip_cols[i]);
    }
  }

  // Inventory panel (I) — cash + named chips (right column mid; exclusive w/ chat focus)
  if (inv_open) {
    r.draw_hud_rect(W - 390.f, 400.f, 370.f, 150.f, Color{10, 16, 22, 220});
    // Cash bar
    const float cash_fill =
        (std::min)(1.f, static_cast<float>(heist.inventory().cash) / 50000.f);
    r.draw_hud_rect(W - 378.f, 412.f, 346.f, 26.f, Color{28, 40, 32, 230});
    r.draw_hud_rect(W - 366.f, 420.f, 322.f * (std::max)(cash_fill, 0.04f), 10.f,
                    Color{50, 200, 90, 230});
    const Color chip_cols[3] = {Color{220, 200, 90, 230}, Color{80, 160, 255, 230},
                                Color{180, 120, 255, 230}};
    for (int i = 0; i < 3; ++i) {
      const float y = 448.f + static_cast<float>(i) * 30.f;
      const int count = heist.inventory().chip_count(static_cast<fury::LootChip>(i));
      r.draw_hud_rect(W - 378.f, y, 346.f, 26.f, Color{28, 34, 48, 220});
      const float fill =
          (std::min)(1.f, static_cast<float>(count) / 8.f);
      r.draw_hud_rect(W - 366.f, y + 8.f,
                      322.f * (std::max)(fill, count > 0 ? 0.08f : 0.04f), 10.f,
                      chip_cols[i]);
    }
  }

  // Faction reputation panel (U) — Pierline / Metro Watch / Syndicate (-100..100)
  if (rep_open) {
    r.draw_hud_rect(W - 390.f, 178.f, 370.f, 148.f, Color{14, 12, 22, 220});
    const int vals[3] = {reps.pierline, reps.metro_watch, reps.syndicate};
    const Color cols[3] = {Color{90, 200, 140, 230}, Color{80, 140, 255, 230},
                           Color{220, 120, 80, 230}};
    for (int i = 0; i < 3; ++i) {
      const float y = 192.f + static_cast<float>(i) * 40.f;
      r.draw_hud_rect(W - 378.f, y, 346.f, 32.f, Color{28, 30, 44, 220});
      // Center-zero bar: left = negative, right = positive
      const float mid = W - 378.f + 173.f;
      r.draw_hud_rect(mid - 1.f, y + 6.f, 2.f, 20.f, Color{70, 80, 100, 220});
      const float t = static_cast<float>(vals[i]) / 100.f;  // -1..1
      if (t >= 0.f) {
        r.draw_hud_rect(mid, y + 10.f, 160.f * (std::max)(t, 0.04f), 12.f, cols[i]);
      } else {
        const float w = 160.f * (std::max)(-t, 0.04f);
        r.draw_hud_rect(mid - w, y + 10.f, w, 12.f, cols[i]);
      }
    }
  }

  // Skill tree panel (N) — XP + 3 nodes (Silent Entry / Fast Hands / Cool Under Heat)
  if (skills_open) {
    r.draw_hud_rect(16.f, 210.f, 360.f, 200.f, Color{12, 10, 22, 220});
    const float xp_t =
        (std::min)(1.f, static_cast<float>(skills.xp) / 400.f);
    r.draw_hud_rect(28.f, 222.f, 336.f, 22.f, Color{28, 30, 48, 230});
    r.draw_hud_rect(40.f, 228.f, 300.f * (std::max)(xp_t, 0.04f), 10.f,
                    Color{180, 140, 255, 230});
    const Color skill_cols[3] = {Color{90, 220, 200, 230}, Color{255, 190, 90, 230},
                                 Color{120, 180, 255, 230}};
    for (int i = 0; i < 3; ++i) {
      const float y = 256.f + static_cast<float>(i) * 40.f;
      const bool on = skills.ranks[i] > 0;
      const bool can = skills.can_unlock(static_cast<fury::SkillId>(i));
      r.draw_hud_rect(28.f, y, 336.f, 32.f,
                      on ? Color{28, 50, 44, 230}
                         : (can ? Color{40, 36, 60, 230} : Color{24, 26, 36, 210}));
      r.draw_hud_rect(40.f, y + 10.f, 14.f, 14.f,
                      on ? skill_cols[i]
                         : (can ? Color{160, 140, 220, 220} : Color{60, 65, 80, 220}));
      r.draw_hud_rect(64.f, y + 12.f, 280.f * (on ? 1.f : (can ? 0.35f : 0.08f)), 10.f,
                      skill_cols[i]);
    }
  }

  // Craft panel (G near loft workbench) — SignalJammer / SmokePellet
  if (craft_open) {
    r.draw_hud_rect(16.f, 210.f, 360.f, 170.f, Color{10, 16, 24, 220});
    r.draw_hud_rect(28.f, 222.f, 336.f, 18.f,
                    near_workbench ? Color{40, 70, 90, 230} : Color{40, 35, 30, 210});
    // Recipe 1 — SignalJammer (owned = full bar)
    {
      const bool on = craft.signal_jammer > 0;
      r.draw_hud_rect(28.f, 250.f, 336.f, 36.f,
                      on ? Color{28, 55, 48, 230}
                         : (near_workbench ? Color{32, 40, 55, 230}
                                           : Color{28, 30, 36, 210}));
      r.draw_hud_rect(40.f, 260.f, 300.f * (on ? 1.f : 0.2f), 12.f,
                      Color{90, 220, 255, 230});
    }
    // Recipe 2 — SmokePellet stack fill
    {
      const float fill =
          (std::min)(1.f, static_cast<float>(craft.smoke_pellet) / 4.f);
      r.draw_hud_rect(28.f, 296.f, 336.f, 36.f,
                      near_workbench ? Color{40, 36, 28, 230} : Color{28, 30, 36, 210});
      r.draw_hud_rect(40.f, 306.f,
                      300.f * (std::max)(fill, craft.smoke_pellet > 0 ? 0.15f : 0.06f),
                      12.f, Color{255, 170, 80, 230});
    }
    if (craft.smoke_pellet > 0) {
      r.draw_hud_rect(28.f, 344.f, 80.f, 18.f, Color{255, 200, 80, 230});
      r.draw_hud_rect(116.f, 348.f, 200.f, 10.f, Color{200, 220, 255, 210});
    }
  }

  // Daily contract HUD pip (top-right under minimap area when not claimed)
  {
    const fury::DailyContractDef& d = daily.today();
    const bool done = daily.claimed_today();
    const float pip_x = W - 56.f;
    const float pip_y = 230.f;
    r.draw_hud_rect(pip_x, pip_y, 40.f, 40.f,
                    done ? Color{20, 48, 36, 200} : Color{36, 28, 18, 200});
    // Fill encodes objective heat cap; gold when open, teal when claimed
    const float fill = (std::min)(1.f, d.max_heat / 0.6f);
    r.draw_hud_rect(pip_x + 6.f, pip_y + 8.f, 28.f * (std::max)(fill, 0.2f), 10.f,
                    done ? Color{80, 220, 160, 230} : Color{255, 190, 80, 230});
    // Tiny peak-heat marker during active run (dim if idle)
    const float peak_t = (std::min)(1.f, run_peak_heat);
    r.draw_hud_rect(pip_x + 6.f, pip_y + 24.f, 28.f * (std::max)(peak_t, 0.04f), 8.f,
                    peak_t > d.max_heat ? Color{255, 70, 50, 230}
                                        : Color{120, 180, 255, 210});
    (void)d;
  }

  // Save slot stub
  {
    const float sx = 16.f;
    const float sy = H - 40.f;
    r.draw_hud_rect(sx, sy, 160.f, 24.f, Color{12, 16, 24, 180});
    for (int i = 0; i < kSaveSlotCount; ++i) {
      const bool sel = (i == save_slot);
      r.draw_hud_rect(sx + 12.f + static_cast<float>(i) * 48.f, sy + 6.f, 36.f,
                      12.f, sel ? Color{80, 200, 255, 240} : Color{50, 60, 80, 200});
    }
  }

  // Onboarding tip bar (bottom center) — step 0 board / 1 target / 2 escape
  // Hidden while chat/help open so bars do not stack on the input strip.
  if (onboard_step >= 0 && onboard_step < 3 && splash_t <= 0.f && !chat_open &&
      !help_open) {
    Color tip_bg{18, 28, 44, 200};
    if (onboard_step == 1) tip_bg = Color{44, 36, 18, 200};
    if (onboard_step == 2) tip_bg = Color{18, 44, 28, 200};
    r.draw_hud_rect(W * 0.5f - 220.f, H - 78.f, 440.f, 28.f, tip_bg);
    // Progress pips for onboarding stages
    for (int i = 0; i < 3; ++i) {
      const bool done = i < onboard_step;
      const bool cur = i == onboard_step;
      r.draw_hud_rect(W * 0.5f - 40.f + static_cast<float>(i) * 28.f, H - 42.f, 20.f,
                      8.f,
                      cur ? Color{255, 200, 80, 240}
                          : (done ? Color{80, 200, 120, 220} : Color{50, 60, 80, 180}));
    }
  }

  // Objective breadcrumb / compass marker (screen-space toward objective)
  if (splash_t <= 0.f) {
    const float dx = objective_pos.x - player_pos.x;
    const float dz = objective_pos.z - player_pos.z;
    const float ang = std::atan2(dx, dz) - player_yaw;
    const float sx = std::sin(ang);
    const float cx = std::cos(ang);
    // Bottom compass strip
    const float compass_x = W * 0.5f + sx * 120.f;
    const float compass_y = H - 110.f;
    r.draw_hud_rect(W * 0.5f - 130.f, compass_y - 4.f, 260.f, 18.f,
                    Color{10, 14, 22, 140});
    Color mark{255, 200, 60, 240};
    if (heist.phase() == fury::HeistPhase::Escape) {
      mark = Color{80, 255, 140, 240};
    } else if (onboard_step == 0) {
      mark = Color{120, 180, 255, 240};
    }
    r.draw_hud_rect(compass_x - 8.f, compass_y, 16.f, 10.f, mark);
    // Forward notch
    if (cx > 0.25f) {
      r.draw_hud_rect(compass_x - 3.f, compass_y - 8.f, 6.f, 6.f, mark);
    }
  }

  // Success / fail banner (+ finale ending "Pierline holds the Harbor")
  if (banner_t > 0.f && splash_t <= 0.f && !cutscene_active) {
    const float fade = std::clamp(banner_t / 0.4f, 0.f, 1.f);
    const std::uint8_t a = static_cast<std::uint8_t>(210 * fade);
    if (ending_banner && banner_success) {
      // Finale splash bars — geometric stand-in for "Pierline holds the Harbor"
      r.draw_hud_rect(0.f, H * 0.22f, W, 160.f, Color{8, 18, 28, static_cast<std::uint8_t>(200 * fade)});
      r.draw_hud_rect(W * 0.5f - 300.f, H * 0.26f, 600.f, 110.f, Color{12, 40, 36, a});
      const Color gold{255, 210, 90, a};
      const Color aqua{90, 220, 200, a};
      r.draw_hud_rect(W * 0.5f - 280.f, H * 0.28f, 560.f, 14.f, gold);
      r.draw_hud_rect(W * 0.5f - 250.f, H * 0.31f, 500.f, 18.f, aqua);
      r.draw_hud_rect(W * 0.5f - 220.f, H * 0.345f, 440.f, 12.f, gold);
      r.draw_hud_rect(W * 0.5f - 180.f, H * 0.375f, 360.f, 10.f,
                      Color{200, 255, 230, static_cast<std::uint8_t>(180 * fade)});
      // Side pips spelling a Pierline / Harbor motif
      for (int i = 0; i < 7; ++i) {
        r.draw_hud_rect(W * 0.5f - 270.f + static_cast<float>(i) * 80.f, H * 0.40f, 50.f, 8.f,
                        i % 2 == 0 ? gold : aqua);
      }
    } else {
      Color bg = banner_success ? Color{12, 48, 28, a} : Color{48, 14, 18, a};
      Color bar = banner_success ? Color{90, 255, 140, a} : Color{255, 70, 70, a};
      r.draw_hud_rect(W * 0.5f - 260.f, H * 0.28f, 520.f, 90.f, bg);
      r.draw_hud_rect(W * 0.5f - 240.f, H * 0.28f + 20.f, 480.f, 16.f, bar);
      r.draw_hud_rect(W * 0.5f - 200.f, H * 0.28f + 48.f, 400.f, 12.f, bar);
      r.draw_hud_rect(W * 0.5f - 160.f, H * 0.28f + 68.f, 320.f, 8.f,
                      Color{255, 255, 255, static_cast<std::uint8_t>(160 * fade)});
    }
  }

  // Cutscene skip hint
  if (cutscene_active && splash_t <= 0.f) {
    r.draw_hud_rect(W * 0.5f - 140.f, H - 56.f, 280.f, 22.f, Color{10, 14, 22, 180});
    r.draw_hud_rect(W * 0.5f - 120.f, H - 50.f, 240.f, 10.f, Color{180, 200, 255, 220});
  }

  // Optional FPS readout (toggle P)
  if (show_fps) {
    const float t = (std::min)(1.f, fps / 120.f);
    r.draw_hud_rect(W - 130.f, H - 40.f, 114.f, 24.f, Color{12, 16, 24, 190});
    r.draw_hud_rect(W - 122.f, H - 34.f, 98.f * (std::max)(t, 0.05f), 12.f,
                    Color{80, 220, 160, 230});
  }


  // Crew banter tip (Rook/Sparrow) — short rotating line on phase change
  if (banter_t > 0.f && banter_line && banter_line[0] && splash_t <= 0.f) {
    const float fade = std::clamp(banter_t / 0.35f, 0.f, 1.f);
    const std::uint8_t a = static_cast<std::uint8_t>(200 * fade);
    r.draw_hud_rect(W * 0.5f - 260.f, H - 148.f, 520.f, 26.f, Color{20, 36, 48, a});
    r.draw_hud_rect(W * 0.5f - 248.f, H - 140.f, 496.f * (std::min)(1.f, banter_t / 3.2f), 10.f,
                    Color{90, 200, 255, a});
  }

  // Alarm active pip (heat high while looting)
  if (alarm_active && splash_t <= 0.f) {
    const float flash = 0.5f + 0.5f * std::sin(heat_t * 28.f + loot_t * 14.f);
    const std::uint8_t a = static_cast<std::uint8_t>(170 + 70 * flash);
    r.draw_hud_rect(W - 56.f, 178.f, 40.f, 40.f, Color{180, 20, 20, a});
    r.draw_hud_rect(W - 48.f, 186.f, 24.f, 24.f, Color{255, 60, 40, a});
  }

  // Pursuit pips — one pip per active patrol car
  if (pursuit_count > 0 && splash_t <= 0.f) {
    const float flash = 0.5f + 0.5f * std::sin(heat_t * 18.f);
    const std::uint8_t a = static_cast<std::uint8_t>(190 + 50 * flash);
    r.draw_hud_rect(W - 120.f, 178.f, 56.f, 40.f, Color{20, 28, 48, 180});
    for (int i = 0; i < pursuit_count; ++i) {
      r.draw_hud_rect(W - 112.f + static_cast<float>(i) * 22.f, 188.f, 16.f, 20.f,
                      Color{60, 120, 255, a});
    }
  }

  // Safehouse tip — save + Tab map / FT + G craft while cooling heat in Harbor loft
  if (in_safehouse && splash_t <= 0.f && !map_open && !craft_open) {
    r.draw_hud_rect(W * 0.5f - 200.f, H - 178.f, 400.f, 26.f, Color{18, 40, 36, 210});
    r.draw_hud_rect(W * 0.5f - 188.f, H - 170.f, 376.f, 10.f, Color{80, 220, 180, 230});
    // Short Tab pip for map / FT + G craft pip
    r.draw_hud_rect(W * 0.5f - 70.f, H - 148.f, 28.f, 12.f, Color{255, 210, 80, 230});
    r.draw_hud_rect(W * 0.5f - 36.f, H - 146.f, 60.f, 8.f, Color{120, 220, 255, 210});
    r.draw_hud_rect(W * 0.5f + 32.f, H - 148.f, 22.f, 12.f, Color{90, 220, 255, 230});
    r.draw_hud_rect(W * 0.5f + 58.f, H - 146.f, 50.f, 8.f, Color{255, 170, 80, 210});
  }

  // Breaker box tip — stand near + E to cut site cameras
  if (breaker_tip && splash_t <= 0.f && !door_enter_tip) {
    r.draw_hud_rect(W * 0.5f - 110.f, H - 118.f, 220.f, 34.f, Color{28, 36, 18, 220});
    r.draw_hud_rect(W * 0.5f - 90.f, H - 108.f, 40.f, 14.f, Color{255, 220, 70, 240});
    r.draw_hud_rect(W * 0.5f - 40.f, H - 108.f, 120.f, 14.f, Color{200, 255, 120, 230});
  }

  // Crouch pip (Ctrl walk)
  if (crouching && splash_t <= 0.f) {
    r.draw_hud_rect(16.f, 170.f, 120.f, 18.f, Color{18, 40, 32, 200});
    r.draw_hud_rect(28.f, 175.f, 96.f, 8.f, Color{80, 220, 160, 230});
  }

  // Door trigger — geometric "Enter" tip (press E to snap inside; walk-through still works)
  if (door_enter_tip && splash_t <= 0.f) {
    r.draw_hud_rect(W * 0.5f - 90.f, H - 118.f, 180.f, 34.f, Color{20, 32, 48, 220});
    r.draw_hud_rect(W * 0.5f - 70.f, H - 108.f, 40.f, 14.f, Color{255, 210, 80, 240});  // E
    r.draw_hud_rect(W * 0.5f - 20.f, H - 108.f, 90.f, 14.f, Color{180, 220, 255, 230}); // Enter
  }

  // Interior zone pip (warm strip when inside bank/jewelry/loft/depot)
  if (interior_tag && interior_tag[0] && splash_t <= 0.f && !door_enter_tip) {
    r.draw_hud_rect(W * 0.5f - 60.f, H - 112.f, 120.f, 18.f, Color{36, 28, 18, 200});
    r.draw_hud_rect(W * 0.5f - 48.f, H - 106.f, 96.f, 6.f, Color{255, 190, 90, 220});
  }

  // Pre-heist lobby panel (L / auto when all ready) — remotes + mission; host Enter starts
  if (lobby_open && splash_t <= 0.f) {
    const float lx = W * 0.5f - 220.f;
    const float ly = H * 0.28f;
    r.draw_hud_rect(lx, ly, 440.f, 210.f, Color{8, 12, 22, 230});
    r.draw_hud_rect(lx + 12.f, ly + 12.f, 416.f, 18.f, Color{255, 200, 80, 240});
    // Mission name as tier bar
    {
      const fury::MissionJob& job = board.current();
      const float tier_t = (std::min)(1.f, static_cast<float>(job.payout_tier) / 4.f);
      r.draw_hud_rect(lx + 24.f, ly + 44.f, 392.f, 22.f, Color{28, 36, 52, 220});
      r.draw_hud_rect(lx + 32.f, ly + 50.f, 376.f * tier_t, 10.f,
                      board.is_finale() ? Color{120, 220, 255, 240}
                                        : Color{255, 200, 80, 230});
    }
    // Connected remotes as name-slot bars
    float ry = ly + 80.f;
    r.draw_hud_rect(lx + 24.f, ry, 392.f, 16.f, Color{40, 55, 75, 200});
    int shown = 0;
    for (const auto& rp : remotes) {
      if (shown >= 4) break;
      const float y = ry + 22.f + static_cast<float>(shown) * 22.f;
      r.draw_hud_rect(lx + 24.f, y, 392.f, 18.f, Color{24, 32, 48, 220});
      r.draw_hud_rect(lx + 32.f, y + 4.f, 12.f, 10.f,
                      rp.ready ? Color{90, 255, 140, 240} : Color{60, 70, 90, 220});
      r.draw_hud_rect(lx + 52.f, y + 5.f, 200.f + 40.f * static_cast<float>(shown % 3), 8.f,
                      Color{80, 200, 255, 210});
      ++shown;
    }
    if (shown == 0) {
      r.draw_hud_rect(lx + 24.f, ry + 22.f, 392.f, 18.f, Color{24, 32, 48, 180});
      r.draw_hud_rect(lx + 52.f, ry + 27.f, 160.f, 8.f, Color{90, 100, 120, 200});
    }
    // Footer: host Start hint vs joiner wait
    r.draw_hud_rect(lx + 24.f, ly + 178.f, 392.f, 20.f,
                    is_net_host ? Color{40, 90, 60, 230} : Color{40, 50, 70, 220});
    r.draw_hud_rect(lx + 40.f, ly + 184.f, is_net_host ? 280.f : 200.f, 8.f,
                    is_net_host ? Color{90, 255, 140, 240} : Color{120, 180, 220, 220});
  }

  // Ready-check pips — local + crew + remotes (tucked under status; clear of board)
  {
    const float rx = 16.f;
    float ry = 148.f;
    if (in_vehicle) ry = 180.f;
    // When left panels open, keep ready strip under the status plate only
    if (board.open || buy_open || skills_open || craft_open) {
      ry = in_vehicle ? 180.f : 148.f;
    }
    r.draw_hud_rect(rx, ry, 220.f, 22.f, Color{12, 16, 24, 170});
    r.draw_hud_rect(rx + 10.f, ry + 5.f, 12.f, 12.f,
                    local_ready ? Color{90, 255, 140, 240} : Color{60, 70, 90, 220});
    float px = rx + 30.f;
    for (const auto& c : crew_roster) {
      r.draw_hud_rect(px, ry + 5.f, 12.f, 12.f,
                      c.ready ? Color{90, 255, 140, 240} : Color{60, 70, 90, 220});
      px += 18.f;
    }
    for (const auto& rp : remotes) {
      r.draw_hud_rect(px, ry + 5.f, 12.f, 12.f,
                      rp.ready ? Color{90, 220, 255, 240} : Color{60, 70, 90, 220});
      px += 18.f;
    }
  }

  // Chat log — last 4 messages as HUD bars + input buffer (above save slots)
  {
    const float cx0 = 16.f;
    // Keep clear of save-slot strip (H-40) and onboarding (hidden while chat_open)
    const float cy0 = H - 196.f;
    const int n = static_cast<int>(chat_log.size());
    for (int i = 0; i < n; ++i) {
      const float y = cy0 + static_cast<float>(i) * 18.f;
      const float t = static_cast<float>(i + 1) / 4.f;
      r.draw_hud_rect(cx0, y, 320.f, 14.f, Color{10, 18, 28, static_cast<std::uint8_t>(140 + 20 * i)});
      r.draw_hud_rect(cx0 + 8.f, y + 4.f, 280.f * (std::min)(1.f, 0.35f + t * 0.5f), 6.f,
                      Color{80, 200, 255, 200});
    }
    if (chat_open) {
      r.draw_hud_rect(cx0, cy0 + 76.f, 340.f, 22.f, Color{20, 36, 52, 230});
      const float fill =
          (std::min)(1.f, static_cast<float>(chat_buffer.size()) / 64.f);
      r.draw_hud_rect(cx0 + 8.f, cy0 + 82.f, 320.f * (std::max)(0.04f, fill), 10.f,
                      Color{120, 220, 255, 240});
    }
  }

  // Controls help overlay (H) — full binding legend as geometric rows
  if (help_open && splash_t <= 0.f) {
    r.draw_hud_rect(W * 0.5f - 320.f, 80.f, 640.f, H - 160.f, Color{8, 12, 20, 230});
    r.draw_hud_rect(W * 0.5f - 300.f, 96.f, 600.f, 18.f, Color{255, 200, 80, 240});
    // Row groups: move / heist / panels / net / system
    const char* groups[] = {"move", "heist", "panels", "net", "system"};
    (void)groups;
    const int rows = 22;
    for (int i = 0; i < rows; ++i) {
      const float y = 124.f + static_cast<float>(i) * 24.f;
      const bool accent = (i % 4 == 0);
      r.draw_hud_rect(W * 0.5f - 290.f, y, 70.f, 18.f,
                      accent ? Color{255, 200, 80, 230} : Color{80, 180, 255, 220});
      r.draw_hud_rect(W * 0.5f - 210.f, y + 4.f, 480.f, 10.f,
                      Color{40, 55, 75, 220});
      // Fill length encodes "binding weight" so rows stay distinct without glyphs
      const float fill = 0.25f + 0.04f * static_cast<float>((i * 3) % 7);
      r.draw_hud_rect(W * 0.5f - 210.f, y + 4.f, 480.f * fill, 10.f,
                      accent ? Color{255, 210, 120, 230} : Color{120, 200, 255, 210});
    }
    // Footer hint bar (H closes)
    r.draw_hud_rect(W * 0.5f - 140.f, H - 70.f, 280.f, 16.f, Color{90, 220, 160, 230});
  }


  // NPC nameplate stub — small HUD bar when looking near a named NPC
  if (nameplate_show && splash_t <= 0.f && !help_open) {
    Color plate{40, 55, 75, 210};
    Color fillc{180, 220, 255, 230};
    switch (nameplate_role) {
      case fury::DialogueRole::Guard:
        plate = Color{40, 48, 70, 220};
        fillc = Color{90, 140, 255, 240};
        break;
      case fury::DialogueRole::Fence:
        plate = Color{50, 36, 22, 220};
        fillc = Color{255, 170, 80, 240};
        break;
      case fury::DialogueRole::Crew:
        plate = Color{20, 48, 40, 220};
        fillc = Color{90, 255, 180, 240};
        break;
      default:
        break;
    }
    const float nw = 120.f + 80.f * std::clamp(nameplate_fill, 0.15f, 1.f);
    r.draw_hud_rect(W * 0.5f - nw * 0.5f, H * 0.38f, nw, 22.f, plate);
    r.draw_hud_rect(W * 0.5f - nw * 0.5f + 10.f, H * 0.38f + 6.f,
                    (nw - 20.f) * std::clamp(nameplate_fill, 0.2f, 1.f), 10.f, fillc);
    // Q talk hint pip under nameplate
    r.draw_hud_rect(W * 0.5f - 28.f, H * 0.38f + 26.f, 22.f, 12.f, Color{255, 210, 80, 230});
    r.draw_hud_rect(W * 0.5f - 2.f, H * 0.38f + 28.f, 40.f, 8.f, Color{180, 220, 255, 210});
  }

  // Bark dialogue panel (Q) — 1–3 geometric line bars + role accent
  if (dialogue_t > 0.f && dialogue_lines > 0 && splash_t <= 0.f && !help_open) {
    const float fade = std::clamp(dialogue_t / 0.35f, 0.f, 1.f);
    const std::uint8_t a = static_cast<std::uint8_t>(220 * fade);
    Color accent{180, 220, 255, a};
    Color panel{12, 18, 28, a};
    switch (dialogue_role) {
      case fury::DialogueRole::Guard:
        accent = Color{90, 140, 255, a};
        panel = Color{16, 22, 40, a};
        break;
      case fury::DialogueRole::Fence:
        accent = Color{255, 170, 80, a};
        panel = Color{28, 20, 12, a};
        break;
      case fury::DialogueRole::Crew:
        accent = Color{90, 255, 180, a};
        panel = Color{12, 28, 22, a};
        break;
      default:
        break;
    }
    const int n = (std::min)(3, (std::max)(1, dialogue_lines));
    const float ph = 28.f + static_cast<float>(n) * 26.f;
    const float py = H * 0.55f;
    r.draw_hud_rect(W * 0.5f - 260.f, py, 520.f, ph, panel);
    r.draw_hud_rect(W * 0.5f - 248.f, py + 8.f, 80.f, 12.f, accent);  // speaker/role pip
    for (int i = 0; i < n; ++i) {
      const float y = py + 28.f + static_cast<float>(i) * 26.f;
      const float fill = 0.45f + 0.15f * static_cast<float>(i);
      r.draw_hud_rect(W * 0.5f - 248.f, y, 496.f, 18.f, Color{28, 36, 48, a});
      r.draw_hud_rect(W * 0.5f - 238.f, y + 4.f, 476.f * fill, 10.f, accent);
    }
  }

  // Fullscreen-ish district map (Tab) — colored district rects + blips + focus
  if (map_open && splash_t <= 0.f) {
    r.draw_hud_rect(0.f, 0.f, W, H, Color{4, 8, 14, 210});
    const float mx = W * 0.08f;
    const float my = H * 0.08f;
    const float mw = W * 0.84f;
    const float mh = H * 0.72f;
    r.draw_hud_rect(mx - 8.f, my - 8.f, mw + 16.f, mh + 16.f, Color{10, 16, 26, 240});
    r.draw_hud_rect(mx, my, mw, mh, Color{18, 28, 40, 230});
    // Title bar
    r.draw_hud_rect(mx + 12.f, my + 10.f, 220.f, 14.f, Color{255, 200, 80, 240});
    constexpr float world_min_x = -120.f;
    constexpr float world_max_x = 130.f;
    constexpr float world_min_z = -60.f;
    constexpr float world_max_z = 120.f;
    auto world_to_big = [&](float wx, float wz, float& ox, float& oy) {
      const float u = (wx - world_min_x) / (world_max_x - world_min_x);
      const float v = (wz - world_min_z) / (world_max_z - world_min_z);
      ox = mx + 10.f + std::clamp(u, 0.f, 1.f) * (mw - 20.f);
      oy = my + 32.f + std::clamp(v, 0.f, 1.f) * (mh - 48.f);
    };
    auto world_size_to_big = [&](float sx, float sz, float& ow, float& oh) {
      ow = sx / (world_max_x - world_min_x) * (mw - 20.f);
      oh = sz / (world_max_z - world_min_z) * (mh - 48.f);
    };
    for (int i = 0; i < kDistrictCount; ++i) {
      const DistrictInfo& d = district_info(i);
      float cx = 0.f, cy = 0.f, rw = 0.f, rh = 0.f;
      world_to_big(d.center.x, d.center.z, cx, cy);
      world_size_to_big(d.half_extents.x * 2.f, d.half_extents.z * 2.f, rw, rh);
      const bool focused = (i == map_focus);
      Color fill = d.fill;
      if (focused) {
        fill.a = 230;
        r.draw_hud_rect(cx - rw * 0.5f - 3.f, cy - rh * 0.5f - 3.f, rw + 6.f, rh + 6.f,
                        Color{255, 220, 100, 240});
      }
      r.draw_hud_rect(cx - rw * 0.5f, cy - rh * 0.5f, rw, rh, fill);
      // Index pip 1..6
      r.draw_hud_rect(cx - rw * 0.5f + 4.f, cy - rh * 0.5f + 4.f, 18.f, 12.f,
                      focused ? Color{255, 220, 100, 255} : Color{20, 28, 40, 220});
    }
    // Objective blip
    float ox = 0.f, oy = 0.f;
    world_to_big(objective_pos.x, objective_pos.z, ox, oy);
    r.draw_hud_rect(ox - 5.f, oy - 5.f, 10.f, 10.f, Color{255, 200, 60, 250});
    // Loft hub marker (always)
    float lx = 0.f, ly = 0.f;
    world_to_big(kHarborLoftPos.x, kHarborLoftPos.z, lx, ly);
    r.draw_hud_rect(lx - 4.f, ly - 4.f, 8.f, 8.f, Color{90, 220, 180, 240});
    // Player blip
    float px = 0.f, py = 0.f;
    world_to_big(player_pos.x, player_pos.z, px, py);
    r.draw_hud_rect(px - 4.f, py - 4.f, 8.f, 8.f, Color{80, 220, 255, 255});
    // Focus legend + FT strip
    {
      const DistrictInfo& d = district_info(map_focus);
      r.draw_hud_rect(mx + 12.f, my + mh - 28.f, mw - 24.f, 18.f, Color{12, 20, 32, 230});
      r.draw_hud_rect(mx + 20.f, my + mh - 24.f, 120.f * (0.35f + 0.1f * static_cast<float>(map_focus)),
                      10.f, d.fill);
      const float tip_y = my + mh + 20.f;
      if (can_fast_travel && map_focus != 4) {
        r.draw_hud_rect(mx + 12.f, tip_y, mw - 24.f, 28.f, Color{18, 40, 36, 230});
        r.draw_hud_rect(mx + 24.f, tip_y + 8.f, 80.f, 12.f, Color{255, 210, 80, 240});  // Enter
        r.draw_hud_rect(mx + 116.f, tip_y + 8.f, 160.f, 12.f, Color{80, 220, 180, 230}); // FT
        // Cost pip length encodes $250 affordability-ish
        const float cash_t =
            (std::min)(1.f, static_cast<float>(heist.inventory().cash) / 50000.f);
        r.draw_hud_rect(mx + mw - 160.f, tip_y + 8.f, 120.f * (std::max)(0.08f, cash_t), 12.f,
                        Color{50, 200, 90, 230});
      } else if (in_safehouse && map_focus == 4) {
        r.draw_hud_rect(mx + 12.f, tip_y, mw - 24.f, 22.f, Color{18, 40, 36, 200});
        r.draw_hud_rect(mx + 24.f, tip_y + 6.f, 200.f, 10.f, Color{90, 220, 180, 210});
      } else if (!in_safehouse) {
        r.draw_hud_rect(mx + 12.f, tip_y, mw - 24.f, 22.f, Color{28, 20, 18, 210});
        r.draw_hud_rect(mx + 24.f, tip_y + 6.f, 240.f, 10.f, Color{200, 120, 80, 210});
      }
      if (ft_cooldown > 0.f) {
        const float ct = std::clamp(ft_cooldown / kFastTravelCooldown, 0.f, 1.f);
        r.draw_hud_rect(mx + mw - 140.f, my + 10.f, 120.f, 10.f, Color{40, 50, 60, 220});
        r.draw_hud_rect(mx + mw - 140.f, my + 10.f, 120.f * ct, 10.f, Color{255, 140, 60, 230});
      }
    }
  }

  // Minimap stub — top-right (hidden while fullscreen map open)
  if (!map_open) {
    const float map_s = 150.f;
    const float map_x = W - map_s - 16.f;
    const float map_y = 16.f;
    r.draw_hud_rect(map_x, map_y, map_s, map_s, Color{18, 24, 34, 190});
    r.draw_hud_rect(map_x + 2.f, map_y + 2.f, map_s - 4.f, map_s - 4.f,
                    Color{28, 40, 55, 160});
    constexpr float world_min_x = -120.f;
    constexpr float world_max_x = 130.f;
    constexpr float world_min_z = -60.f;
    constexpr float world_max_z = 120.f;
    auto world_to_map = [&](const Vec3& p, float& ox, float& oy) {
      const float u = (p.x - world_min_x) / (world_max_x - world_min_x);
      const float v = (p.z - world_min_z) / (world_max_z - world_min_z);
      ox = map_x + 6.f + std::clamp(u, 0.f, 1.f) * (map_s - 12.f);
      oy = map_y + 6.f + std::clamp(v, 0.f, 1.f) * (map_s - 12.f);
    };
    float px = 0.f, py = 0.f, ox = 0.f, oy = 0.f;
    world_to_map(player_pos, px, py);
    world_to_map(objective_pos, ox, oy);
    float sx = 0.f, sy = 0.f;
    world_to_map(kHarborLoftPos, sx, sy);
    r.draw_hud_rect(sx - 3.f, sy - 3.f, 6.f, 6.f, Color{90, 220, 180, 230});
    r.draw_hud_rect(ox - 4.f, oy - 4.f, 8.f, 8.f, Color{255, 200, 60, 240});
    r.draw_hud_rect(px - 3.f, py - 3.f, 6.f, 6.f, Color{80, 220, 255, 255});
  }
}


}  // namespace

int main(int argc, char** argv) {
  bool force_soft = false;
  bool smoke_mode = false;
  fury::net::NetMode net_mode = fury::net::NetMode::Embedded;
  std::string net_host = "127.0.0.1";
  std::uint16_t net_port = 7777;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i] ? argv[i] : "";
    if (a == "--soft" || a == "-soft") force_soft = true;
    if (a == "--smoke" || a == "-smoke") smoke_mode = true;
    if (a.rfind("--net=", 0) == 0) {
      const std::string v = a.substr(6);
      if (v == "host") net_mode = fury::net::NetMode::Host;
      else if (v == "join") net_mode = fury::net::NetMode::Join;
      else net_mode = fury::net::NetMode::Embedded;
    } else if (a == "--net" && i + 1 < argc) {
      const std::string v = argv[++i] ? argv[i] : "";
      if (v == "host") net_mode = fury::net::NetMode::Host;
      else if (v == "join") net_mode = fury::net::NetMode::Join;
      else net_mode = fury::net::NetMode::Embedded;
    } else if (a.rfind("--net-host=", 0) == 0) {
      net_host = a.substr(11);
    } else if (a == "--net-host" && i + 1 < argc) {
      net_host = argv[++i] ? argv[i] : "127.0.0.1";
    } else if (a.rfind("--net-port=", 0) == 0) {
      net_port = static_cast<std::uint16_t>(std::atoi(a.substr(11).c_str()));
    }
  }
  if (const char* env = std::getenv("FURY_SOFT")) {
    if (env[0] == '1' || env[0] == 't' || env[0] == 'T' || env[0] == 'y' ||
        env[0] == 'Y') {
      force_soft = true;
    }
  }
  if (const char* env = std::getenv("FURY_SMOKE")) {
    if (env[0] != '\0' && env[0] != '0' && env[0] != 'f' && env[0] != 'F') {
      smoke_mode = true;
    }
  }
  if (const char* env = std::getenv("FURY_NET")) {
    const std::string v = env;
    if (v == "host" || v == "HOST") net_mode = fury::net::NetMode::Host;
    else if (v == "join" || v == "JOIN") net_mode = fury::net::NetMode::Join;
    else if (v == "embedded" || v == "EMBEDDED" || v == "loopback")
      net_mode = fury::net::NetMode::Embedded;
  }
  if (const char* env = std::getenv("FURY_NET_HOST")) {
    if (env[0] != '\0') net_host = env;
  }
  if (const char* env = std::getenv("FURY_NET_PORT")) {
    if (env[0] != '\0') {
      const int p = std::atoi(env);
      if (p > 0 && p < 65536) net_port = static_cast<std::uint16_t>(p);
    }
  }
  // Smoke / CI stays on embedded loopback host+client.
  if (smoke_mode) {
    net_mode = fury::net::NetMode::Embedded;
  }

  bool unlock_all = false;
  if (const char* env = std::getenv("FURY_UNLOCK_ALL")) {
    if (env[0] == '1' || env[0] == 't' || env[0] == 'T' || env[0] == 'y' ||
        env[0] == 'Y') {
      unlock_all = true;
    }
  }

  bool perf_log = false;
  if (const char* env = std::getenv("FURY_PERF")) {
    if (env[0] == '1' || env[0] == 't' || env[0] == 'T' || env[0] == 'y' ||
        env[0] == 'Y') {
      perf_log = true;
    }
  }

  fury::QualityLevel quality_level = fury::QualityLevel::Med;
  if (const char* env = std::getenv("FURY_QUALITY")) {
    quality_level = fury::QualityPreset::parse_env(env);
  }
  fury::QualityPreset quality = fury::QualityPreset::make(quality_level);

  fury::AppConfig config;
  config.window.title = "Fury — Vaultline 3.7.0";
  config.window.width = 1280;
  config.window.height = 720;
  config.clear_color = {78, 118, 168, 255};
  config.log_fps = false;  // optional; toggle with P
  config.fps_log_interval = 1.0f;
  config.prefer_opengl = !force_soft;
  config.cull_distance = quality.cull_distance;
  config.lod_mid_distance = quality.cull_distance * 0.5f;
  config.capture_mouse = !smoke_mode;
  config.enable_collision = true;
  config.player_radius = 0.45f;

  fury::Application app(std::move(config));
  if (!app.init()) {
    fury::Log::error("Failed to initialize Vaultline");
    return EXIT_FAILURE;
  }

  fury::Lighting lit = app.renderer().lighting();
  lit.sun_direction = {-0.35f, -0.88f, -0.28f};
  lit.sun_color = {1.f, 0.96f, 0.88f};
  lit.sun_intensity = 1.15f;
  lit.ambient = {0.16f, 0.20f, 0.28f};
  lit.fog_color = {78.f / 255.f, 118.f / 255.f, 168.f / 255.f};
  lit.ao_strength = 0.55f;
  lit.enable_shadows = true;
  quality.apply_to_lighting(lit);
  app.renderer().set_lighting(lit);
  app.renderer().set_shadow_map_size(quality.shadow_map_size);

  build_harbor_metro(app.scene());

  const fury::InteriorCatalog interiors = fury::make_harbor_interiors();
  const char* active_interior_tag = "";
  bool door_enter_tip = false;
  bool door_tip_logged = false;

  app.camera().position = {0.f, 1.7f, 12.f};
  app.camera().yaw = -1.5707963f;
  app.camera().pitch = -0.08f;
  app.camera().fly_mode = false;
  app.camera().move_speed = 9.f;
  app.camera().far_plane = quality.camera_far;
  app.camera().snap_look();

  fury::DayNightCycle day_night;
  day_night.day_length = 160.f;
  day_night.time_of_day = 0.34f;
  fury::Lighting base_lit = lit;
  fury::WeatherStub weather;

  auto audio = fury::create_audio();
  audio->init();

  fury::NpcSystem npcs;

  auto spawn_npc = [&](fury::NpcAgent agent, const fury::Vec3& color) {
    fury::Entity e;
    e.name = agent.entity_name.empty() ? agent.name : agent.entity_name;
    agent.entity_name = e.name;
    // Unique humanoid mesh per agent so walk poses do not stomp each other.
    e.mesh = app.scene().add_mesh(
        fury::make_humanoid(agent.height, color, 0.f));
    e.transform.position = agent.position;
    e.material.albedo = color;
    e.material.roughness = 0.65f;
    e.material.metallic = 0.05f;
    e.solid = false;
    app.scene().add_entity(std::move(e));
    npcs.add(std::move(agent));
  };

  {
    fury::NpcAgent a;
    a.name = "CivA";
    a.display_name = "Mira Vale";
    a.entity_name = "NpcCivA";
    a.kind = fury::NpcKind::Civilian;
    a.height = 1.75f;
    a.position = {-12.f, 0.875f, 10.f};
    a.speed = 2.4f;
    a.waypoints = {{-12.f, 0.f, 10.f}, {12.f, 0.f, 10.f}, {12.f, 0.f, -18.f},
                   {-12.f, 0.f, -18.f}};
    spawn_npc(std::move(a), {0.55f, 0.72f, 0.85f});
  }
  {
    fury::NpcAgent a;
    a.name = "CivB";
    a.display_name = "Jon Keel";
    a.entity_name = "NpcCivB";
    a.kind = fury::NpcKind::Civilian;
    a.height = 1.7f;
    a.position = {18.f, 0.85f, 22.f};
    a.speed = 2.1f;
    a.waypoints = {{18.f, 0.f, 22.f}, {34.f, 0.f, 22.f}, {34.f, 0.f, 8.f},
                   {18.f, 0.f, 8.f}};
    spawn_npc(std::move(a), {0.85f, 0.62f, 0.45f});
  }
  {
    fury::NpcAgent a;
    a.name = "CivC";
    a.display_name = "Tessa Quill";
    a.entity_name = "NpcCivC";
    a.kind = fury::NpcKind::Civilian;
    a.height = 1.65f;
    a.position = {88.f, 0.825f, 8.f};
    a.speed = 2.0f;
    a.waypoints = {{88.f, 0.f, 8.f}, {102.f, 0.f, 8.f}, {102.f, 0.f, 18.f},
                   {88.f, 0.f, 18.f}, {70.f, 0.f, 6.f}};
    spawn_npc(std::move(a), {0.65f, 0.80f, 0.55f});
  }
  {
    fury::NpcAgent g;
    g.name = "BankGuard";
    g.display_name = "Sgt. Hale";
    g.entity_name = "NpcGuard";
    g.kind = fury::NpcKind::Guard;
    g.height = 1.85f;
    g.position = {4.f, 0.925f, -2.f};
    g.speed = 1.6f;
    g.chase_speed = 3.5f;
    g.waypoints = {{4.f, 0.f, -2.f}, {-4.f, 0.f, -2.f}, {-4.f, 0.f, 4.f},
                   {4.f, 0.f, 4.f}, {0.f, 0.f, -6.f}};
    spawn_npc(std::move(g), {0.25f, 0.35f, 0.55f});
  }
  // Ashcourt civilian
  {
    fury::NpcAgent a;
    a.name = "CivAsh";
    a.display_name = "Nell Ash";
    a.entity_name = "NpcCivAsh";
    a.kind = fury::NpcKind::Civilian;
    a.height = 1.75f;
    a.position = {-88.f, 0.875f, 42.f};
    a.speed = 1.9f;
    a.waypoints = {{-88.f, 0.f, 42.f}, {-80.f, 0.f, 42.f}, {-80.f, 0.f, 50.f},
                   {-92.f, 0.f, 50.f}, {-70.f, 0.f, 28.f}};
    spawn_npc(std::move(a), {0.72f, 0.58f, 0.40f});
  }
  // Ashcourt fence broker (near shop) — unique Q dialogue role
  {
    fury::NpcAgent f;
    f.name = "Fence";
    f.display_name = "Cass Vesper";
    f.entity_name = "NpcFence";
    f.kind = fury::NpcKind::Fence;
    f.height = 1.78f;
    f.position = {-84.f, 0.89f, 46.f};
    f.speed = 1.2f;
    f.waypoints = {{-84.f, 0.f, 46.f}, {-88.f, 0.f, 50.f}, {-82.f, 0.f, 50.f},
                   {-86.f, 0.f, 45.f}};
    spawn_npc(std::move(f), {0.90f, 0.55f, 0.28f});
  }

  // Police chase AI — box-mesh patrol cars (spawn on high heat / alarm)
  fury::PursuitSystem pursuit;
  auto* patrol_body = app.scene().add_mesh(
      fury::make_box({4.2f, 1.35f, 2.0f}, Vec3{0.12f, 0.22f, 0.55f}));
  auto* patrol_light = app.scene().add_mesh(
      fury::make_box({0.55f, 0.25f, 1.4f}, Vec3{0.9f, 0.15f, 0.12f}));
  Material patrol_mat;
  patrol_mat.metallic = 0.55f;
  patrol_mat.roughness = 0.4f;
  patrol_mat.albedo = {0.2f, 0.35f, 0.75f};
  Material patrol_light_mat;
  patrol_light_mat.albedo = {1.0f, 0.25f, 0.2f};
  patrol_light_mat.emissive = 0.4f;
  patrol_light_mat.roughness = 0.85f;
  std::vector<fury::PatrolCar> patrol_slots;
  for (int i = 0; i < 2; ++i) {
    const std::string body_name = std::string("PatrolCar") + std::to_string(i);
    const std::string light_name = std::string("PatrolLight") + std::to_string(i);
    {
      fury::Entity e;
      e.name = body_name;
      e.tag = "patrol";
      e.mesh = patrol_body;
      e.transform.position = {0.f, -40.f, 0.f};
      e.material = patrol_mat;
      e.solid = false;
      e.visible = false;
      app.scene().add_entity(std::move(e));
    }
    {
      fury::Entity e;
      e.name = light_name;
      e.tag = "patrol_light";
      e.mesh = patrol_light;
      e.transform.position = {0.f, -40.f, 0.f};
      e.material = patrol_light_mat;
      e.solid = false;
      e.visible = false;
      app.scene().add_entity(std::move(e));
    }
    fury::PatrolCar car;
    car.entity_name = body_name;
    car.spawn_slot = i;
    car.speed = (i == 0) ? 11.5f : 10.2f;
    patrol_slots.push_back(std::move(car));
  }
  pursuit.configure(std::move(patrol_slots));

  // 2.3.0 civilian traffic AI — looping street cars (not pursuit); slow near player
  fury::TrafficSystem traffic;
  auto* traffic_body = app.scene().add_mesh(
      fury::make_box({4.0f, 1.2f, 1.9f}, Vec3{0.55f, 0.55f, 0.58f}));
  auto* traffic_cabin = app.scene().add_mesh(
      fury::make_box({2.2f, 0.7f, 1.7f}, Vec3{0.35f, 0.45f, 0.55f}));
  const Vec3 traffic_colors[] = {
      {0.72f, 0.22f, 0.18f}, {0.20f, 0.45f, 0.75f}, {0.85f, 0.75f, 0.25f},
      {0.25f, 0.55f, 0.35f}, {0.55f, 0.55f, 0.58f}, {0.40f, 0.30f, 0.55f},
  };
  // Street loops across Harbor / bridge / Ashcourt / North Quay approach
  const std::vector<std::vector<Vec3>> traffic_routes = {
      {{-20.f, 0.f, 10.f}, {20.f, 0.f, 10.f}, {20.f, 0.f, -18.f},
       {-20.f, 0.f, -18.f}},
      {{12.f, 0.f, 8.f}, {42.f, 0.f, 8.f}, {55.f, 0.f, 6.f}, {70.f, 0.f, 6.f},
       {55.f, 0.f, 6.f}, {42.f, 0.f, 8.f}},
      {{-48.f, 0.f, 10.f}, {-70.f, 0.f, 26.f}, {-88.f, 0.f, 42.f},
       {-70.f, 0.f, 28.f}, {-48.f, 0.f, 12.f}},
      {{0.f, 0.f, 28.f}, {18.f, 0.f, 48.f}, {18.f, 0.f, 72.f}, {18.f, 0.f, 90.f},
       {8.f, 0.f, 96.f}, {18.f, 0.f, 72.f}, {18.f, 0.f, 48.f}},
      {{34.f, 0.f, 22.f}, {34.f, 0.f, -10.f}, {14.f, 0.f, -20.f},
       {-10.f, 0.f, -10.f}, {-10.f, 0.f, 22.f}},
      {{88.f, 0.f, 8.f}, {102.f, 0.f, 8.f}, {102.f, 0.f, 18.f}, {88.f, 0.f, 18.f},
       {70.f, 0.f, 6.f}},
  };
  std::vector<fury::TrafficCar> traffic_slots;
  const int traffic_count = 6;
  for (int i = 0; i < traffic_count; ++i) {
    const std::string body_name = std::string("TrafficCar") + std::to_string(i);
    const std::string cab_name = std::string("TrafficCabin") + std::to_string(i);
    Material body_mat;
    body_mat.albedo = traffic_colors[i % 6];
    body_mat.metallic = 0.45f;
    body_mat.roughness = 0.42f;
    Material cab_mat;
    cab_mat.albedo = {0.25f, 0.35f, 0.45f};
    cab_mat.metallic = 0.2f;
    cab_mat.roughness = 0.35f;
    {
      fury::Entity e;
      e.name = body_name;
      e.tag = "traffic";
      e.mesh = traffic_body;
      e.transform.position = traffic_routes[static_cast<std::size_t>(i)][0];
      e.transform.position.y = 0.85f;
      e.material = body_mat;
      e.solid = false;
      e.visible = true;
      app.scene().add_entity(std::move(e));
    }
    {
      fury::Entity e;
      e.name = cab_name;
      e.tag = "traffic";
      e.mesh = traffic_cabin;
      e.transform.position = traffic_routes[static_cast<std::size_t>(i)][0];
      e.transform.position.y = 1.55f;
      e.material = cab_mat;
      e.solid = false;
      e.visible = true;
      app.scene().add_entity(std::move(e));
    }
    fury::TrafficCar car;
    car.entity_name = body_name;
    car.waypoints = traffic_routes[static_cast<std::size_t>(i)];
    car.cruise_speed = 6.5f + 0.35f * static_cast<float>(i);
    traffic_slots.push_back(std::move(car));
  }
  traffic.configure(std::move(traffic_slots));

  fury::HeistController heist;
  heist.vault_position = {0.f, 0.f, -15.2f};
  heist.escape_position = {34.f, 0.f, 30.f};
  heist.approach_radius = 5.5f;
  heist.interact_radius = 3.8f;
  heist.breach_duration = 1.8f;
  heist.loot_duration = 4.5f;
  heist.escape_radius = 5.f;
  heist.escape_timeout = 60.f;
  heist.loot_fail_timeout = 28.f;
  heist.base_payout = 9000;
  heist.jewelry_bonus = 0;

  // Active target: 0 Meridian, 1 Crown, 2 ATM, 3 Depot, 4 Night Vault, 5 North Quay Yard
  fury::MissionBoard mission_board;
  fury::QuestJournal quest_journal;
  const Vec3 meridian_vault{0.f, 0.f, -15.2f};
  const Vec3 jewel_vault{-22.f, 0.f, 4.8f};
  const Vec3 ashcourt_atm{-90.f, 0.f, 28.55f};  // AshcourtAtm alcove face
  const Vec3 harbor_depot{58.f, 0.f, -51.2f};   // HarborDepotCage face
  const Vec3 north_quay_yard{8.f, 0.f, 112.f};  // NorthQuaySealedContainer face
  const Vec3 vault_positions[6] = {meridian_vault, jewel_vault, ashcourt_atm,
                                   harbor_depot, meridian_vault, north_quay_yard};

  fury::HeatMeter heat;
  fury::VisibilityMeter visibility;
  fury::SecurityNet security;
  bool breaker_tip = false;
  bool breaker_tip_logged = false;
  const float base_escape_timeout = heist.escape_timeout;

  // Driveable vehicles: getaway van + Ashcourt civilian sedan (3.3.0)
  DriveableSlot driveables[2] = {
      {{34.f, 1.2f, 33.5f}, 0.f, DriveKind::Van, "getaway van"},
      {{-82.f, 0.85f, 38.f}, 1.5707963f, DriveKind::CivSedan, "Ashcourt sedan"},
  };
  constexpr float kVehicleEnterRadius = 4.2f;
  int seated_vehicle = -1;  // index into driveables, or -1 on foot
  // in_vehicle bool kept in sync with seated_vehicle for existing call sites
  bool in_vehicle = false;
  int radio_station = 0;
  bool c_was_down = false;

  // AI crew stubs (follow during heist) — low-poly humanoids
  fury::CrewSystem crew;
  {
    fury::CrewMember c;
    c.name = "Crew-Rook";
    c.display_name = "Rook";
    c.entity_name = "CrewRook";
    c.height = 1.7f;
    c.follow_offset = {-1.8f, 0.f, -1.4f};
    c.position = {-2.f, 0.85f, 14.f};
    crew.add(std::move(c));
    fury::Entity e;
    e.name = "CrewRook";
    e.mesh = app.scene().add_mesh(
        fury::make_humanoid(1.7f, fury::Vec3{0.35f, 0.75f, 0.55f}, 0.f));
    e.transform.position = {-2.f, 0.85f, 14.f};
    e.material.albedo = {0.35f, 0.75f, 0.55f};
    e.material.roughness = 0.6f;
    app.scene().add_entity(std::move(e));
  }
  {
    fury::CrewMember c;
    c.name = "Crew-Sparrow";
    c.display_name = "Sparrow";
    c.entity_name = "CrewSparrow";
    c.height = 1.72f;
    c.follow_offset = {1.8f, 0.f, -1.2f};
    c.position = {2.f, 0.86f, 14.f};
    crew.add(std::move(c));
    fury::Entity e;
    e.name = "CrewSparrow";
    e.mesh = app.scene().add_mesh(
        fury::make_humanoid(1.72f, fury::Vec3{0.75f, 0.45f, 0.35f}, 0.f));
    e.transform.position = {2.f, 0.86f, 14.f};
    e.material.albedo = {0.75f, 0.45f, 0.35f};
    e.material.roughness = 0.6f;
    app.scene().add_entity(std::move(e));
  }

  // 2.7.0 replay ghost trail markers (hidden until F10 scrub)
  auto* replay_ghost_mesh = app.scene().add_mesh(
      fury::make_box({0.22f, 0.22f, 0.22f}, Vec3{0.25f, 0.90f, 1.0f}));
  {
    Material ghost_mat;
    ghost_mat.albedo = {0.25f, 0.90f, 1.0f};
    ghost_mat.emissive = 1.6f;
    ghost_mat.roughness = 0.85f;
    for (int gi = 0; gi < 24; ++gi) {
      Entity ge;
      ge.name = "ReplayGhost";
      ge.tag = "replay_ghost";
      ge.mesh = replay_ghost_mesh;
      ge.material = ghost_mat;
      ge.visible = false;
      ge.transform.position = {0.f, -50.f, 0.f};
      app.scene().add_entity(std::move(ge));
    }
  }

  // Optional third-person player body (V toggle; hidden in fly-cam / first-person)
  constexpr float kPlayerBodyHeight = 1.75f;
  const Vec3 kPlayerBodyColor{0.32f, 0.58f, 0.88f};
  float player_anim_phase = 0.f;
  {
    fury::Entity e;
    e.name = "PlayerBody";
    e.mesh = app.scene().add_mesh(
        fury::make_humanoid(kPlayerBodyHeight, kPlayerBodyColor, 0.f));
    e.transform.position = {0.f, kPlayerBodyHeight * 0.5f, 12.f};
    e.material.albedo = kPlayerBodyColor;
    e.material.roughness = 0.62f;
    e.material.metallic = 0.05f;
    e.visible = false;
    e.solid = false;
    app.scene().add_entity(std::move(e));
  }

  // Lamp positions for dynamic point lights (filled once from scene tags)
  std::vector<Vec3> lamp_positions;
  for (const auto& ent : app.scene().entities()) {
    if (ent.tag == "lamp") {
      lamp_positions.push_back(ent.transform.position);
    }
  }

  fury::ParticleSystem particles;
  auto* fx_quad = app.scene().add_mesh(
      fury::make_box({1.f, 1.f, 1.f}, Vec3{1.f, 0.85f, 0.25f}));

  // Tag remaining asphalt-textured props; cache dry materials for wet tint
  struct AsphaltDry {
    std::string name;
    Vec3 albedo;
    float roughness;
    float metallic;
  };
  std::vector<AsphaltDry> asphalt_dry;
  for (auto& ent : app.scene().entities()) {
    if (ent.material.texture == TextureSlot::Asphalt) {
      if (ent.tag.empty()) {
        ent.tag = "asphalt";
      }
      asphalt_dry.push_back(
          {ent.name, ent.material.albedo, ent.material.roughness,
           ent.material.metallic});
    }
  }

  auto net_client = fury::net::create_loopback_client();
  {
    fury::net::ConnectOptions net_opts;
    std::string connect_host = "127.0.0.1";
    if (net_mode == fury::net::NetMode::Host) {
      net_opts.ensure_embedded_host = true;
      net_opts.host_bind = fury::net::ListenBind::Any;
      connect_host = "127.0.0.1";
    } else if (net_mode == fury::net::NetMode::Join) {
      net_opts.ensure_embedded_host = false;
      connect_host = net_host.empty() ? "127.0.0.1" : net_host;
    } else {
      net_opts.ensure_embedded_host = true;
      net_opts.host_bind = fury::net::ListenBind::Loopback;
      connect_host = "127.0.0.1";
    }
    fury::Log::info(std::string("Net mode: ") + fury::net::net_mode_name(net_mode) +
                    " -> " + connect_host + ":" + std::to_string(net_port));
    if (!net_client->connect(connect_host, net_port, net_opts)) {
      fury::Log::warn("Net connect failed — continuing offline stubs");
    }
  }
  // Session crew roles (net stub)
  net_client->assign_crew_role(10, "Crew-Rook", fury::net::CrewRole::Muscle);
  net_client->assign_crew_role(11, "Crew-Sparrow", fury::net::CrewRole::Lookout);

  fury::SessionSnapshot session;
  fury::PlayerPerks perks;
  fury::FactionReputations factions;
  BuyMenu buy_menu;
  InventoryPanel inv_panel;
  RepPanel rep_panel;
  HelpPanel help_panel;
  MapPanel map_panel;
  float fast_travel_cd = 0.f;
  bool map_mouse_was_down = false;
  bool tab_was_down = false;
  fury::SkillPanel skill_panel;
  fury::SkillTree skills;
  fury::CraftPanel craft_panel;
  fury::CraftInventory craft;
  fury::FenceUpgrades fence_up;
  fury::DailyContracts daily;
  float run_peak_heat = 0.f;
  int active_slot = 0;
  {
    std::ostringstream sid;
    sid << "vl-" << net_client->session().session_id;
    session.session_id = sid.str();
  }
  session.world = "Harbor Metro / Ridge Pier / Ashcourt / Depot / North Quay";
  session.player_name = "Operator";

  auto apply_session_to_play = [&]() {
    heist.inventory().cash = session.cash;
    heist.score().successes = session.successes;
    heist.score().failures = session.failures;
    heist.score().lifetime_cash = session.lifetime_score;
    mission_board.selected = std::clamp(session.heist_target_index, 0,
                                       static_cast<int>(fury::kMissionCount) - 1);
    perks.crew = (std::max)(0, session.perk_crew);
    perks.heat_damp = (std::max)(0, session.perk_heat_damp);
    perks.loot_speed = (std::max)(0, session.perk_loot_speed);
    for (int i = 0; i < static_cast<int>(fury::kMissionCount); ++i) {
      quest_journal.complete[i] = session.mission_complete[i] ? 1 : 0;
    }
    heist.inventory().chips[0] = (std::max)(0, session.item_bearer_bond);
    heist.inventory().chips[1] = (std::max)(0, session.item_sapphire);
    heist.inventory().chips[2] = (std::max)(0, session.item_ledger_drive);
    factions.pierline = fury::FactionReputations::clamp_rep(session.rep_pierline);
    factions.metro_watch =
        fury::FactionReputations::clamp_rep(session.rep_metro_watch);
    factions.syndicate = fury::FactionReputations::clamp_rep(session.rep_syndicate);
    skills.xp = (std::max)(0, session.skill_xp);
    skills.ranks[0] = session.skill_silent_entry ? 1 : 0;
    skills.ranks[1] = session.skill_fast_hands ? 1 : 0;
    skills.ranks[2] = session.skill_cool_under_heat ? 1 : 0;
    daily.claim_ymd = (std::max)(0, session.daily_claim_ymd);
    craft.signal_jammer = session.item_signal_jammer ? 1 : 0;
    craft.smoke_pellet = (std::max)(0, session.item_smoke_pellet);
    fence_up.better_payouts = session.upgrade_better_payouts != 0;
    fence_up.quieter_tools = session.upgrade_quieter_tools != 0;
  };

  auto fill_session_from_play = [&]() {
    session.cash = heist.inventory().cash;
    session.successes = heist.score().successes;
    session.failures = heist.score().failures;
    session.lifetime_score = heist.score().lifetime_cash;
    session.heist_target_index = mission_board.selected;
    session.perk_crew = perks.crew;
    session.perk_heat_damp = perks.heat_damp;
    session.perk_loot_speed = perks.loot_speed;
    session.save_slot = active_slot;
    for (int i = 0; i < static_cast<int>(fury::kMissionCount); ++i) {
      session.mission_complete[i] = quest_journal.complete[i] ? 1 : 0;
    }
    session.item_bearer_bond = heist.inventory().chips[0];
    session.item_sapphire = heist.inventory().chips[1];
    session.item_ledger_drive = heist.inventory().chips[2];
    session.rep_pierline = factions.pierline;
    session.rep_metro_watch = factions.metro_watch;
    session.rep_syndicate = factions.syndicate;
    session.skill_xp = skills.xp;
    session.skill_silent_entry = skills.ranks[0] ? 1 : 0;
    session.skill_fast_hands = skills.ranks[1] ? 1 : 0;
    session.skill_cool_under_heat = skills.ranks[2] ? 1 : 0;
    session.daily_claim_ymd = daily.claim_ymd;
    session.item_signal_jammer = craft.signal_jammer ? 1 : 0;
    session.item_smoke_pellet = craft.smoke_pellet;
    session.upgrade_better_payouts = fence_up.better_payouts ? 1 : 0;
    session.upgrade_quieter_tools = fence_up.quieter_tools ? 1 : 0;
  };

  auto autosave_slot = [&]() {
    fill_session_from_play();
    fury::save_session_json(fury::session_slot_path(active_slot), session);
  };

  auto load_slot = [&](int slot) {
    slot = std::clamp(slot, 0, kSaveSlotCount - 1);
    active_slot = slot;
    fury::SessionSnapshot loaded = session;
    if (fury::load_session_json(fury::session_slot_path(slot), loaded)) {
      session = loaded;
    } else {
      // Fresh slot — keep identity, reset progress
      session.cash = 0;
      session.successes = 0;
      session.failures = 0;
      session.lifetime_score = 0;
      session.heist_target_index = 0;
      session.perk_crew = 0;
      session.perk_heat_damp = 0;
      session.perk_loot_speed = 0;
      for (int i = 0; i < static_cast<int>(fury::kMissionCount); ++i) {
        session.mission_complete[i] = 0;
      }
      session.item_bearer_bond = 0;
      session.item_sapphire = 0;
      session.item_ledger_drive = 0;
      session.rep_pierline = 0;
      session.rep_metro_watch = 0;
      session.rep_syndicate = 0;
      session.skill_xp = 0;
      session.skill_silent_entry = 0;
      session.skill_fast_hands = 0;
      session.skill_cool_under_heat = 0;
      session.daily_claim_ymd = 0;
      session.item_signal_jammer = 0;
      session.item_smoke_pellet = 0;
      session.upgrade_better_payouts = 0;
      session.upgrade_quieter_tools = 0;
    }
    session.save_slot = active_slot;
    apply_session_to_play();
    heist.reset();
    heat.reset();
    visibility.reset();
    fury::Log::info(std::string("Save slot ") + std::to_string(active_slot) +
                    " active ($" + std::to_string(heist.inventory().cash) + ")");
  };

  // Prefer slot 0; migrate legacy vaultline_session.json if present
  if (!fury::load_session_json(fury::session_slot_path(0), session)) {
    if (fury::load_session_json("vaultline_session.json", session)) {
      session.save_slot = 0;
      fury::save_session_json(fury::session_slot_path(0), session);
      fury::Log::info("Migrated legacy vaultline_session.json -> slot 0");
    }
  }
  active_slot = std::clamp(session.save_slot, 0, kSaveSlotCount - 1);
  apply_session_to_play();

  // Stability: save/load roundtrip self-check (temp file)
  {
    fury::SessionSnapshot probe = session;
    probe.cash = 4242;
    probe.successes = 7;
    probe.perk_crew = 2;
    probe.save_slot = 1;
    probe.mission_complete[0] = 1;
    probe.mission_complete[2] = 1;
    probe.mission_complete[3] = 1;
    probe.mission_complete[4] = 1;
    probe.item_bearer_bond = 3;
    probe.item_sapphire = 2;
    probe.item_ledger_drive = 1;
    probe.rep_pierline = 42;
    probe.rep_metro_watch = -35;
    probe.rep_syndicate = 12;
    probe.skill_xp = 175;
    probe.skill_silent_entry = 1;
    probe.skill_fast_hands = 0;
    probe.skill_cool_under_heat = 1;
    probe.daily_claim_ymd = 20260907;
    probe.item_signal_jammer = 1;
    probe.item_smoke_pellet = 2;
    probe.upgrade_better_payouts = 1;
    probe.upgrade_quieter_tools = 1;
    const std::string rt_path = "vaultline_roundtrip_tmp.json";
    if (fury::save_session_json(rt_path, probe)) {
      fury::SessionSnapshot back{};
      if (fury::load_session_json(rt_path, back) && back.cash == 4242 &&
          back.successes == 7 && back.perk_crew == 2 && back.save_slot == 1 &&
          back.mission_complete[0] == 1 && back.mission_complete[2] == 1 &&
          back.mission_complete[3] == 1 && back.mission_complete[4] == 1 &&
          back.item_bearer_bond == 3 &&
          back.item_sapphire == 2 && back.item_ledger_drive == 1 &&
          back.rep_pierline == 42 && back.rep_metro_watch == -35 &&
          back.rep_syndicate == 12 && back.skill_xp == 175 &&
          back.skill_silent_entry == 1 && back.skill_fast_hands == 0 &&
          back.skill_cool_under_heat == 1 && back.daily_claim_ymd == 20260907 &&
          back.item_signal_jammer == 1 && back.item_smoke_pellet == 2 &&
          back.upgrade_better_payouts == 1 && back.upgrade_quieter_tools == 1) {
        fury::Log::info("Session save/load roundtrip OK");
      } else {
        fury::Log::warn("Session save/load roundtrip MISMATCH");
      }
      std::remove(rt_path.c_str());
    }
  }

  auto apply_target = [&]() {
    const int idx = mission_board.selected;
    if (idx == fury::kFinaleMissionIndex &&
        !quest_journal.finale_unlocked(unlock_all)) {
      fury::Log::info(
          "Meridian Night Vault locked — complete other Harbor jobs first "
          "(or set FURY_UNLOCK_ALL=1)");
      mission_board.selected = 0;
    }
    const int use_idx = mission_board.selected;
    const fury::MissionJob& job = mission_board.current();
    heist.vault_position =
        vault_positions[static_cast<std::size_t>(use_idx) %
                       (sizeof(vault_positions) / sizeof(vault_positions[0]))];
    heist.base_payout = static_cast<int>(
        static_cast<float>(job.base_payout) * fence_up.payout_mul() + 0.5f);
    heist.jewelry_bonus = static_cast<int>(
        static_cast<float>(job.jewelry_bonus) * fence_up.payout_mul() + 0.5f);
    heist.breach_duration =
        job.breach_duration * skills.breach_duration_mul() *
        fence_up.quieter_breach_mul(skills.unlocked(fury::SkillId::SilentEntry));
    heist.loot_duration = job.loot_duration;
    // Finale: harder heat + force night lighting cue
    if (mission_board.is_finale()) {
      heist.escape_timeout = base_escape_timeout * 0.85f;
      day_night.time_of_day = 0.92f;  // deep night
      fury::Log::info(
          "Finale lighting cue: night forced — harder heat, bigger payout");
    } else {
      heist.escape_timeout = base_escape_timeout;
    }
    fury::Log::info(std::string("Mission selected: ") + job.title +
                    " (tier " + std::to_string(job.payout_tier) + ", $" +
                    std::to_string(job.base_payout + job.jewelry_bonus) + ")");
    heist.reset();
    heat.reset();
    visibility.reset();
  };
  apply_target();


  // 3.5.0 security net — cameras + breakers at bank / jewelry / depot
  {
    auto add_cam = [&](const char* body, const char* lens, float yaw, int site) {
      fury::SecurityCamera c;
      if (auto* e = app.scene().find_by_name(body)) {
        c.position = e->transform.position;
      }
      c.yaw = yaw;
      c.site_id = site;
      c.entity_name = body;
      c.lens_name = lens;
      security.add_camera(std::move(c));
    };
    auto add_brk = [&](const char* name, int site) {
      fury::BreakerBox b;
      if (auto* e = app.scene().find_by_name(name)) {
        b.position = e->transform.position;
      }
      b.site_id = site;
      b.entity_name = name;
      security.add_breaker(std::move(b));
    };
    add_cam("BankCamL", "BankCamLLens", -0.35f, 0);
    add_cam("BankCamR", "BankCamRLens", 3.49f, 0);
    add_cam("BankCamVault", "BankCamVaultLens", 1.5708f, 0);
    add_brk("BankBreaker", 0);
    add_cam("JewelCamFront", "JewelCamFrontLens", -1.5708f, 1);
    add_cam("JewelCamSide", "JewelCamSideLens", 0.2f, 1);
    add_brk("JewelBreaker", 1);
    add_cam("DepotCamHall", "DepotCamHallLens", -1.5708f, 2);
    add_cam("DepotCamBay", "DepotCamBayLens", -1.8f, 2);
    add_brk("DepotBreaker", 2);
    fury::Log::info("Security: cameras at Meridian / Crown & Cutler / Depot; E near breaker cuts site cams");
  }

  fury::Log::info("=== Vaultline 3.7.0 — lightning + puddles + storm ===");
  fury::Log::info("Original bank-heist open-world MMO prototype — no Rockstar/GTA IP.");
  fury::Log::info("WASD move (accel/decel), mouse look (smoothed), Space/Ctrl up/down (fly), Ctrl crouch (walk), F walk/fly, V first/third, Shift sprint");
  fury::Log::info("E near vault/safe/ATM/depot/container to breach → loot → green pad to extract");
  fury::Log::info("F/E near getaway van or Ashcourt sedan to enter/exit (steal); WASD drive; C cycles radio");
  fury::Log::info("M opens mission board; 1/2/3/4/5/6 select job (or T cycles); 5=finale when unlocked; 6=North Quay yard");
  fury::Log::info("J opens quest journal (missions + completion flags in save)");
  fury::Log::info("Q near a named NPC opens 1-3 line bark dialogue (unique fence/guard/crew lines); nameplate when looking near");
  fury::Log::info("B opens Ashcourt fence buy/sell (near shop): 1-3 buy perks; 4 Better Payouts; 5 Quieter Tools; Left/Right chip; S sell");
  fury::Log::info("G near loft workbench opens craft UI: 1 SignalJammer (Bond+Drive); 2 SmokePellet (Sapphire+Bond); X uses SmokePellet");
  fury::Log::info("I toggles inventory panel (cash + BearerBond / Sapphire / LedgerDrive)");
  fury::Log::info("U toggles faction reputation panel (Pierline / Metro Watch / Syndicate)");
  fury::Log::info("N toggles skill tree (XP from heists; 1/2/3 unlock Silent Entry / Fast Hands / Cool Under Heat)");
  fury::Log::info(daily.status_line());
  fury::Log::info("Heist success raises Pierline, lowers Metro Watch; fence sell raises Syndicate tension");
  fury::Log::info("Low Metro Watch → faster pursuits; high Pierline → Ashcourt shop discount");
  fury::Log::info("Successful extract rolls per-mission loot table (cash + named chips)");
  fury::Log::info("[ ] cycle save slots (vaultline_session_slotN.json); autosaves active slot + items");
  fury::Log::info("P toggles FPS overlay/log; R cycles weather; F6 cycles quality (low/med/high); F8 mutes audio; F9 photo; F10 replay");
  fury::Log::info("H toggles full controls help overlay");
  fury::Log::info("Title splash → Harbor fly-over cutscene (Esc skip) → onboarding; footstep/impact cues");
  fury::Log::info("TIP: Press M to open the mission board, then head to the gold objective");
  fury::Log::info("Crew stubs follow during heist and boost loot speed nearby");
  fury::Log::info("Net: UDP syncs pose/heat/phase/mission/loot/cash/ready; L lobby; Enter/Y chat; K ready");
  fury::Log::info("Net modes: default embedded | FURY_NET=host listen | FURY_NET=join + FURY_NET_HOST");
  fury::Log::info("Meridian Mutual heist tuned for ~2–5 min including travel");
  fury::Log::info("Day/night + NPCs + Ridge Pier + Ashcourt + Harbor Armored Depot + North Quay");
  fury::Log::info("Crew banter on phase changes; siren flashes when heat high while looting");
  fury::Log::info("High heat/alarm spawns patrol cars — lose by distance, van, or Harbor loft");
  fury::Log::info("Harbor loft safehouse (waterfront) clears heat; G craft at workbench; save tip while inside ([/])");
  fury::Log::info("Tab opens district map (1-6 / click focus); from loft Enter fast-travels to hubs ($250, cooldown)");
  fury::Log::info("Interior zones: bank/jewelry/loft/depot boost ambient + fill lights; door volumes show Enter (E snap)");
  fury::Log::info("Weather stub: clear/rain/storm/auto-drizzle; denser fog + rain streaks + wet asphalt; storm lightning + puddles");
  fury::Log::info("3.7.0: storm weather (R); lightning flash + thunder cue + ambient spike; Harbor/Ashcourt puddles when wet; heavier storm rain");
  fury::Log::info("3.6.0: loft workbench craft (G) SignalJammer/SmokePellet; fence Better Payouts + Quieter Tools (Silent Entry synergy); craft/upgrades in save");
  fury::Log::info("3.5.0: Ctrl crouch (walk) + visibility meter; security cams (bank/depot/jewelry) + breaker E cut");
  fury::Log::info("3.2.0: NPC display names + look-near nameplate HUD; Q bark dialogue (fence/guard/crew unique); approach log");
  fury::Log::info("3.1.0: water wave normals + shore foam + better fresnel; 2-cascade shadows on high (single med/low; off soft/llvmpipe)");
  fury::Log::info("3.0.0: major prototype milestone — docs/help/net/districts tour of 2.x; still not AAA/GTA");
  fury::Log::info("2.9.0: co-op mission+phase+loot UDP sync (joiner mirrors host); pre-heist lobby (L / auto when ready; host Enter starts)");
  fury::Log::info("2.8.0: LOD stub (detail props skip/proxy beyond mid); AABB behind-plane cull; deep-indoor sector hide; draw sort by material");
  fury::Log::info("2.7.0: photo mode (F9 freeze/free-cam/hide HUD, Esc exit); replay ring buffer scrub (F10, A/D, ghost path)");
  fury::Log::info("2.6.0: skill tree stub (N) + XP; daily rotating contract (hash of date) + HUD pip + cash bonus; skills/daily in save");
  fury::Log::info("2.5.0: interior lighting zones (bank/jewelry/loft/depot) + door Enter tips / optional snap; open doorways kept");
  fury::Log::info("2.4.0: low-poly humanoid NPC/crew/player meshes; procedural limb swing; V first/third (body when not fly)");
  fury::Log::info("2.3.0: North Quay industrial district + bridge; civilian traffic AI (stop/slow); container yard job");
  fury::Log::info("2.2.0: optional SDL_mixer procedural beeps; day/night/rain ambience hooks; F8 mute");
  fury::Log::info(std::string("2.1.0: denser Harbor/Ridge/Ashcourt props; FURY_QUALITY=") +
                    quality.name() + " (F6 cycles low/med/high); fog/cull/shadow/bloom/reflect");
  fury::Log::info("2.0.0: HUD/UX polish, H help, cull 90m, far-NPC skip, FURY_PERF=1, CHANGELOG");
  fury::Log::info("1.9.0: intro cutscene fly-over (Esc skip); Meridian Night Vault finale; ending banner");
  fury::Log::info("Finale unlock: complete jobs 1-4 or FURY_UNLOCK_ALL=1; night-forced + harder heat");
  fury::Log::info("Materials: brick/metal/glass textures; water waves/foam + fresnel; bloom-lite; CSM stub (high)");
  fury::Log::info(std::string("Audio backend: ") + audio->backend_name());
  fury::Log::info("Esc releases mouse, Esc again quits — session autosaves on success/fail");

  {
    auto* ghost_mesh = app.scene().add_mesh(
        fury::make_box(fury::Vec3{0.8f, 1.8f, 0.8f},
                       fury::Vec3{0.3f, 0.7f, 0.9f}));
    fury::Entity ghost;
    ghost.name = "GhostLoop";
    ghost.mesh = ghost_mesh;
    ghost.transform.position = {8.f, 0.9f, 10.f};
    ghost.material.metallic = 0.2f;
    ghost.material.roughness = 0.5f;
    app.scene().add_entity(std::move(ghost));
  }

  fury::HeistPhase last_phase = heist.phase();
  float status_timer = 0.f;
  bool t_was_down = false;
  bool m_was_down = false;
  bool j_was_down = false;
  bool b_was_down = false;
  bool i_was_down = false;
  bool u_was_down = false;
  bool n_was_down = false;
  bool g_was_down = false;
  bool x_was_down = false;
  bool s_was_down = false;
  bool left_was = false;
  bool right_was = false;
  bool bracket_l_was = false;
  bool bracket_r_was = false;
  bool digit_was_down[7] = {false, false, false, false, false, false, false};
  float ghost_cash_flash = 0.f;
  float ghost_last_cash = -1.f;

  // Presentation + onboarding + cutscene / finale / help 2.0.0 / pursuit / factions / safehouse
  float splash_remaining = smoke_mode ? 0.f : 1.5f;
  float banner_timer = 0.f;
  bool banner_success = false;
  bool ending_banner = false;
  bool show_fps = false;
  bool p_was_down = false;
  bool f6_was_down = false;
  bool f8_was_down = false;
  float quality_tip_timer = 0.f;
  float mute_tip_timer = 0.f;
  bool f9_was_down = false;
  bool f10_was_down = false;
  fury::PhotoMode photo_mode;
  fury::ReplayBuffer replay;
  float photo_tip_timer = 0.f;
  float replay_tip_timer = 0.f;
  float siren_cue_accum = 0.f;
  bool h_was_down = false;
  float perf_log_timer = 0.f;
  int perf_npc_updated = 0;
  int perf_npc_total = 0;
  // 0 = open board, 1 = go to target, 2 = escape, 3 = done
  int onboard_step = (session.successes > 0) ? 3 : 0;
  int onboard_tip_logged = -1;
  float smoke_elapsed = 0.f;
  fury::CutsceneStub intro_cutscene;
  bool cutscene_pending = !smoke_mode;  // play once after splash (skip cutscene+chat in CI smoke)
  const Vec3 gameplay_spawn{0.f, 1.7f, 12.f};
  const float gameplay_yaw = -1.5707963f;
  const float gameplay_pitch = -0.08f;
  fury::CrewBanter crew_banter;
  float banter_timer = 0.f;
  const char* banter_line = "";
  fury::DialogueBarks dialogue_barks;
  float dialogue_timer = 0.f;
  int dialogue_line_count = 0;
  fury::DialogueRole dialogue_role = fury::DialogueRole::Civilian;
  std::string dialogue_speaker;
  std::vector<std::string> dialogue_lines;
  std::string approach_logged_id;  // last NPC we logged approach for
  bool nameplate_show = false;
  fury::DialogueRole nameplate_role = fury::DialogueRole::Civilian;
  float nameplate_fill = 0.5f;
  std::string focus_npc_id;
  std::string focus_npc_label;
  fury::DialogueRole focus_role = fury::DialogueRole::Civilian;
  bool q_was_down = false;
  float alarm_time = 0.f;
  bool alarm_active = false;
  bool in_safehouse = false;
  bool safehouse_tip_logged = false;
  int pursuit_count = 0;
  bool r_was_down = false;
  float footstep_accum = 0.f;
  Vec3 foot_last_pos = app.camera().position;
  float rain_emit_accum = 0.f;
  float lightning_cd = 2.5f;
  float lightning_flash = 0.f;
  std::uint32_t weather_rng = 0xA5F17E37u;
  bool chat_open = false;
  std::string chat_buffer;
  bool local_ready = false;
  bool lobby_open = false;
  bool lobby_auto_armed = true;  // re-arm when not all ready
  bool l_was_down = false;
  int mirrored_mission = -1;
  std::uint8_t mirrored_phase = 255;

  auto dist_xz = [](const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
  };

  auto nearest_driveable = [&](const Vec3& from) -> int {
    int best = -1;
    float best_d = kVehicleEnterRadius + 1.f;
    for (int i = 0; i < 2; ++i) {
      const float d = dist_xz(from, driveables[i].pos);
      if (d <= kVehicleEnterRadius && d < best_d) {
        best_d = d;
        best = i;
      }
    }
    return best;
  };

  auto place_part = [&](const char* name, const DriveableSlot& slot, float lx,
                        float ly, float lz, bool hide) {
    if (auto* ent = app.scene().find_by_name(name)) {
      const Vec3 off = vehicle_local_offset(slot.yaw, lx, ly, lz);
      ent->transform.position = {slot.pos.x + off.x, slot.pos.y + off.y,
                                 slot.pos.z + off.z};
      ent->transform.rotation_euler = {0.f, slot.yaw, 0.f};
      ent->visible = !hide;
    }
  };

  auto sync_vehicle_entity = [&]() {
    // Van parts (index 0)
    {
      const DriveableSlot& slot = driveables[0];
      const bool hide = (seated_vehicle == 0);
      place_part("GetawayVanBed", slot, -0.55f, -0.05f, 0.f, hide);
      place_part("GetawayVanCab", slot, 1.55f, 0.05f, 0.f, hide);
      place_part("GetawayVanGlass", slot, 2.35f, 0.35f, 0.f, hide);
      place_part("GetawayVanHeadL", slot, 2.45f, -0.25f, -0.72f, hide);
      place_part("GetawayVanHeadR", slot, 2.45f, -0.25f, 0.72f, hide);
    }
    // Civ sedan (index 1)
    {
      const DriveableSlot& slot = driveables[1];
      const bool hide = (seated_vehicle == 1);
      place_part("CivSedanBody", slot, 0.f, 0.f, 0.f, hide);
      place_part("CivSedanCabin", slot, 0.15f, 0.55f, 0.f, hide);
      place_part("CivSedanHeadL", slot, 1.75f, -0.15f, -0.62f, hide);
      place_part("CivSedanHeadR", slot, 1.75f, -0.15f, 0.62f, hide);
    }
  };

  auto try_toggle_vehicle = [&](bool pressed) -> bool {
    if (!pressed) {
      return false;
    }
    if (seated_vehicle >= 0) {
      DriveableSlot& slot = driveables[seated_vehicle];
      const std::string label = slot.label;
      const Vec3 side = vehicle_local_offset(slot.yaw, 0.f, 0.f, -3.2f);
      seated_vehicle = -1;
      in_vehicle = false;
      app.camera().vehicle_seated = false;
      app.camera().fly_mode = false;
      app.camera().velocity = {};
      app.camera().position = {slot.pos.x + side.x, 1.7f, slot.pos.z + side.z};
      app.camera().snap_look();
      sync_vehicle_entity();
      fury::Log::info(std::string("Exited ") + label);
      return true;
    }
    const int near_i = nearest_driveable(app.camera().position);
    if (near_i < 0) {
      return false;
    }
    DriveableSlot& slot = driveables[near_i];
    seated_vehicle = near_i;
    in_vehicle = true;
    app.camera().vehicle_seated = true;
    app.camera().fly_mode = false;
    app.camera().velocity = {};
    app.camera().yaw = slot.yaw;
    app.camera().yaw_target = slot.yaw;
    app.camera().position = {slot.pos.x, 1.55f, slot.pos.z};
    app.camera().snap_look();
    sync_vehicle_entity();
    if (slot.kind == DriveKind::CivSedan) {
      fury::Log::info(
          "Stole Ashcourt sedan — WASD drive, F/E exit, C cycle radio");
    } else {
      fury::Log::info(
          "Entered getaway van — WASD drive, F/E exit, C cycle radio");
    }
    return true;
  };

  sync_vehicle_entity();  // initial cab/bed/headlight layout

  app.on_pre_update = [&](float /*dt*/, const fury::InputState& input) {
    // Photo / replay: consume F/V so fly/third toggles do not fight free-cam
    if (photo_mode.active || replay.scrubbing) {
      return true;
    }
    // Consume F when used for vehicle enter/exit (near any driveable or seated)
    const bool near =
        in_vehicle || nearest_driveable(app.camera().position) >= 0;
    if (input.key_f && near) {
      try_toggle_vehicle(true);
      return true;
    }
    return false;
  };

  app.on_update = [&](float dt, const fury::InputState& input) {
    if (fast_travel_cd > 0.f && !map_panel.open) {
      fast_travel_cd = (std::max)(0.f, fast_travel_cd - dt);
    }
    if (splash_remaining > 0.f) {
      splash_remaining = (std::max)(0.f, splash_remaining - dt);
      if (splash_remaining <= 0.f && cutscene_pending && !intro_cutscene.finished) {
        cutscene_pending = false;
        intro_cutscene.begin();
        app.input().set_cinematic(true);
        fury::Log::info("Cutscene: Harbor Metro fly-over (Esc to skip)");
      }
    }
    if (banner_timer > 0.f) {
      banner_timer = (std::max)(0.f, banner_timer - dt);
      if (banner_timer <= 0.f) {
        ending_banner = false;
      }
    }
    if (banter_timer > 0.f) {
      banter_timer = (std::max)(0.f, banter_timer - dt);
    }
    if (dialogue_timer > 0.f) {
      dialogue_timer = (std::max)(0.f, dialogue_timer - dt);
      if (dialogue_timer <= 0.f) {
        dialogue_line_count = 0;
        dialogue_lines.clear();
      }
    }

    alarm_time += dt;
    if (smoke_mode) {
      smoke_elapsed += dt;
      if (smoke_elapsed >= 2.7f) {
        app.request_quit();
      }
    }

    // Intro cutscene — keyframe lerp; Esc skips
    if (intro_cutscene.active) {
      if (input.escape_pressed) {
        intro_cutscene.skip();
        fury::Log::info("Cutscene skipped");
      } else {
        intro_cutscene.update(dt, app.camera());
      }
      if (!intro_cutscene.active) {
        app.input().set_cinematic(false);
        app.camera().position = gameplay_spawn;
        app.camera().yaw = gameplay_yaw;
        app.camera().pitch = gameplay_pitch;
        app.camera().fly_mode = false;
        app.camera().velocity = {};
        app.camera().snap_look();
        fury::Log::info("Cutscene complete — Harbor Metro");
      }
      // Still drive day/night visuals during fly-over
      day_night.update(dt);
      fury::Lighting framed_cs = day_night.apply(base_lit);
      app.renderer().set_lighting(framed_cs);
      app.config().clear_color = day_night.sky_clear();
      return;
    }

    const bool is_net_host = (net_mode != fury::net::NetMode::Join);

    // Pre-heist lobby — Esc closes; host Enter starts (commit mission + clear ready)
    if (lobby_open && !chat_open) {
      if (input.escape_pressed) {
        lobby_open = false;
        lobby_auto_armed = false;
        if (!help_panel.open) app.input().set_cinematic(false);
        fury::Log::info("Lobby closed (Esc)");
      } else if (input.key_enter && is_net_host && !smoke_mode) {
        apply_target();
        local_ready = false;
        net_client->set_crew_ready(10, false);
        net_client->set_crew_ready(11, false);
        lobby_open = false;
        lobby_auto_armed = false;
        if (!help_panel.open) app.input().set_cinematic(false);
        if (onboard_step == 0) onboard_step = 1;
        fury::Log::info(std::string("LOBBY START — ") + mission_board.current().title +
                        " (host Enter); head to objective");
      }
    }

    // Chat stub — Enter / Y open buffer; Esc cancels; Enter sends Chat UDP
    // (Enter in lobby is Start for host — do not open chat)
    if (chat_open) {
      if (!input.text_chars.empty()) {
        for (char ch : input.text_chars) {
          if (chat_buffer.size() >= 64) break;
          if (ch >= 32 && ch < 127) chat_buffer.push_back(ch);
        }
      }
      if (input.key_backspace && !chat_buffer.empty()) {
        chat_buffer.pop_back();
      }
      if (input.escape_pressed) {
        chat_open = false;
        chat_buffer.clear();
        app.input().set_text_entry(false);
        fury::Log::info("Chat cancelled");
      } else if (input.key_enter) {
        if (!chat_buffer.empty()) {
          net_client->send_chat(chat_buffer);
        }
        chat_buffer.clear();
        chat_open = false;
        app.input().set_text_entry(false);
      }
    } else if (!smoke_mode && !lobby_open && (input.key_enter || input.key_y)) {
      chat_open = true;
      chat_buffer.clear();
      app.input().set_text_entry(true);
      buy_menu.open = false;
      mission_board.open = false;
      quest_journal.open = false;
      inv_panel.open = false;
      rep_panel.open = false;
      help_panel.open = false;
      skill_panel.open = false;
      craft_panel.open = false;
      map_panel.open = false;
      fury::Log::info("Chat open — type message, Enter to send, Esc to cancel");
    }

    // Tab — fullscreen district map (M stays mission board; Esc/Tab closes)
    {
      const Uint8* keys_tab = SDL_GetKeyboardState(nullptr);
      const bool tab_down = keys_tab[SDL_SCANCODE_TAB] != 0;
      if (!chat_open && !smoke_mode && !help_panel.open && tab_down && !tab_was_down) {
        map_panel.open = !map_panel.open;
        if (map_panel.open) {
          buy_menu.open = false;
          mission_board.open = false;
          quest_journal.open = false;
          inv_panel.open = false;
          rep_panel.open = false;
          skill_panel.open = false;
          craft_panel.open = false;
          lobby_open = false;
          app.input().set_mouse_captured(false);
          app.input().set_cinematic(true);
          fury::Log::info(std::string("MAP OPEN (Tab) — focus ") +
                          district_info(map_panel.focus).name +
                          " | 1-6 or click district" +
                          (in_safehouse ? " | loft: Enter fast travel ($250)" : " | FT from Harbor loft only"));
        } else {
          if (!help_panel.open && !lobby_open) app.input().set_cinematic(false);
          fury::Log::info("Map closed");
        }
      }
      tab_was_down = tab_down;
    }
    if (map_panel.open && input.escape_pressed) {
      map_panel.open = false;
      if (!help_panel.open && !lobby_open) app.input().set_cinematic(false);
      fury::Log::info("Map closed");
    }

    // H — toggle full controls help overlay (closes other panels; Esc/H closes)
    {
      const Uint8* keys_h = SDL_GetKeyboardState(nullptr);
      const bool h_down = keys_h[SDL_SCANCODE_H] != 0;
      if (!chat_open && !smoke_mode && h_down && !h_was_down) {
        help_panel.open = !help_panel.open;
        if (help_panel.open) {
          buy_menu.open = false;
          mission_board.open = false;
          quest_journal.open = false;
          inv_panel.open = false;
          rep_panel.open = false;
          skill_panel.open = false;
          craft_panel.open = false;
          map_panel.open = false;
          lobby_open = false;
          app.input().set_cinematic(true);  // Esc closes help without quitting
          fury::Log::info("HELP (H) — WASD move | Mouse look | Space/Ctrl fly up/down | Ctrl crouch (walk) | Shift sprint | F fly/van/steal sedan | V 1st/3rd | C radio (in vehicle)");
          fury::Log::info("HELP — E breach / door snap / vehicle / breaker | Q talk | Tab map | M board | J journal | B fence | G craft (loft) | I inv | U rep | N skills | X smoke");
          fury::Log::info("HELP — 1-6 jobs/map focus (B:1-3 buy) | loft map Enter=FT | Left/Right+S sell | T cycle | [ ] saves | R weather | P FPS");
          fury::Log::info("HELP — F6 quality | F8 mute | F9 photo | F10 replay (A/D scrub) | L lobby | Enter/Y chat | host Enter start | K ready");
          fury::Log::info("HELP — Esc/H closes this overlay (also exits photo/replay)");
        } else {
          app.input().set_cinematic(false);
          fury::Log::info("Help closed");
        }
      }
      h_was_down = h_down;
    }
    if (help_panel.open && input.escape_pressed) {
      help_panel.open = false;
      app.input().set_cinematic(false);
      fury::Log::info("Help closed");
    }

    // Modal help — freeze gameplay sim (lighting still ticks for readability)
    if (help_panel.open) {
      day_night.update(dt);
      fury::Lighting framed_help = day_night.apply(base_lit);
      app.renderer().set_lighting(framed_help);
      app.config().clear_color = day_night.sky_clear();
      return;
    }

    // Modal map — select district focus; loft-only fast travel confirm
    if (map_panel.open) {
      if (fast_travel_cd > 0.f) {
        fast_travel_cd = (std::max)(0.f, fast_travel_cd - dt);
      }
      const Uint8* keys_map = SDL_GetKeyboardState(nullptr);
      const SDL_Scancode digit_scans_map[6] = {
          SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
          SDL_SCANCODE_5, SDL_SCANCODE_6};
      for (int i = 0; i < 6; ++i) {
        const bool down = keys_map[digit_scans_map[i]] != 0;
        if (down && !digit_was_down[i + 1]) {
          map_panel.focus = i;
          fury::Log::info(std::string("Map focus: ") + district_info(i).name);
        }
        digit_was_down[i + 1] = down;
      }
      // Click district rects (cursor free while cinematic)
      {
        int mx = 0, my = 0;
        const Uint32 buttons = SDL_GetMouseState(&mx, &my);
        const bool md = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        if (md && !map_mouse_was_down) {
          const float W = static_cast<float>(app.window().width());
          const float H = static_cast<float>(app.window().height());
          const float panel_x = W * 0.08f;
          const float panel_y = H * 0.08f;
          const float panel_w = W * 0.84f;
          const float panel_h = H * 0.72f;
          constexpr float world_min_x = -120.f;
          constexpr float world_max_x = 130.f;
          constexpr float world_min_z = -60.f;
          constexpr float world_max_z = 120.f;
          const float fx = static_cast<float>(mx);
          const float fy = static_cast<float>(my);
          for (int i = 0; i < kDistrictCount; ++i) {
            const DistrictInfo& d = district_info(i);
            const float u = (d.center.x - world_min_x) / (world_max_x - world_min_x);
            const float v = (d.center.z - world_min_z) / (world_max_z - world_min_z);
            const float cx = panel_x + 10.f + std::clamp(u, 0.f, 1.f) * (panel_w - 20.f);
            const float cy = panel_y + 32.f + std::clamp(v, 0.f, 1.f) * (panel_h - 48.f);
            const float rw = d.half_extents.x * 2.f / (world_max_x - world_min_x) * (panel_w - 20.f);
            const float rh = d.half_extents.z * 2.f / (world_max_z - world_min_z) * (panel_h - 48.f);
            if (fx >= cx - rw * 0.5f && fx <= cx + rw * 0.5f &&
                fy >= cy - rh * 0.5f && fy <= cy + rh * 0.5f) {
              map_panel.focus = i;
              fury::Log::info(std::string("Map focus (click): ") + d.name);
              break;
            }
          }
        }
        map_mouse_was_down = md;
      }
      // Enter — confirm fast travel from loft to focused hub
      if (input.key_enter) {
        const int fi = map_panel.focus;
        if (fi == 4) {
          fury::Log::info("Already at Harbor loft — pick another district to travel");
        } else if (!in_safehouse) {
          fury::Log::info("Fast travel only from Harbor loft safehouse");
        } else if (fast_travel_cd > 0.f) {
          fury::Log::info(std::string("Fast travel cooling down (") +
                          std::to_string(static_cast<int>(fast_travel_cd + 0.99f)) +
                          "s)");
        } else if (heist.inventory().cash < kFastTravelCost) {
          fury::Log::info(std::string("Need $") + std::to_string(kFastTravelCost) +
                          " for fast travel (have $" +
                          std::to_string(heist.inventory().cash) + ")");
        } else {
          const DistrictInfo& d = district_info(fi);
          heist.inventory().cash -= kFastTravelCost;
          if (in_vehicle) {
            in_vehicle = false;
            seated_vehicle = -1;
            app.camera().vehicle_seated = false;
            sync_vehicle_entity();
          }
          app.camera().position = d.hub;
          app.camera().fly_mode = false;
          fast_travel_cd = kFastTravelCooldown;
          map_panel.open = false;
          if (!help_panel.open && !lobby_open) app.input().set_cinematic(false);
          autosave_slot();
          fury::Log::info(std::string("FAST TRAVEL → ") + d.name + " (-$" +
                          std::to_string(kFastTravelCost) + ")");
        }
      }
      day_night.update(dt);
      fury::Lighting framed_map = day_night.apply(base_lit);
      app.renderer().set_lighting(framed_map);
      app.config().clear_color = day_night.sky_clear();
      return;
    }

    // F9 photo mode / F10 replay scrub (2.7.0) — edge triggers; mutual exclusion
    {
      const Uint8* keys_pr = SDL_GetKeyboardState(nullptr);
      const bool f9_down = keys_pr[SDL_SCANCODE_F9] != 0;
      const bool f10_down = keys_pr[SDL_SCANCODE_F10] != 0;
      if (!chat_open && !smoke_mode && f9_down && !f9_was_down) {
        if (replay.scrubbing) {
          replay.end_scrub(app.camera());
          app.input().set_cinematic(false);
          for (auto& ent : app.scene().entities()) {
            if (ent.tag == "replay_ghost") ent.visible = false;
          }
        }
        if (photo_mode.active) {
          photo_mode.exit(app.camera());
          app.input().set_escape_modal(false);
          if (in_vehicle) {
            app.camera().vehicle_seated = true;
            app.camera().fly_mode = false;
          }
          fury::Log::info("Photo mode OFF (F9)");
        } else {
          help_panel.open = false;
          buy_menu.open = false;
          mission_board.open = false;
          quest_journal.open = false;
          inv_panel.open = false;
          rep_panel.open = false;
          skill_panel.open = false;
          craft_panel.open = false;
          map_panel.open = false;
          photo_mode.enter(app.camera());
          app.input().set_escape_modal(true);
          photo_tip_timer = 2.5f;
          fury::Log::info("Photo mode ON (F9) — sim frozen, free cam WASD+look, HUD hidden, Esc exits");
        }
      }
      if (!chat_open && !smoke_mode && f10_down && !f10_was_down) {
        if (photo_mode.active) {
          photo_mode.exit(app.camera());
          app.input().set_escape_modal(false);
          if (in_vehicle) {
            app.camera().vehicle_seated = true;
            app.camera().fly_mode = false;
          }
        }
        if (replay.scrubbing) {
          replay.end_scrub(app.camera());
          app.input().set_cinematic(false);
          app.input().set_escape_modal(false);
          for (auto& ent : app.scene().entities()) {
            if (ent.tag == "replay_ghost") ent.visible = false;
          }
          fury::Log::info("Replay scrub OFF (F10)");
        } else if (replay.count > 0) {
          help_panel.open = false;
          buy_menu.open = false;
          mission_board.open = false;
          quest_journal.open = false;
          inv_panel.open = false;
          rep_panel.open = false;
          skill_panel.open = false;
          craft_panel.open = false;
          map_panel.open = false;
          replay.begin_scrub(app.camera());
          app.input().set_cinematic(true);
          app.input().set_escape_modal(true);
          replay_tip_timer = 2.5f;
          fury::Log::info("Replay scrub ON (F10) — A/D scrub path, ghost trail, Esc exits");
        } else {
          fury::Log::info("Replay buffer empty — move around first");
        }
      }
      f9_was_down = f9_down;
      f10_was_down = f10_down;
    }
    if (photo_tip_timer > 0.f) photo_tip_timer -= dt;
    if (replay_tip_timer > 0.f) replay_tip_timer -= dt;

    // Photo mode — freeze sim; free camera already driven by Application (fly)
    if (photo_mode.active) {
      if (input.escape_pressed) {
        photo_mode.exit(app.camera());
        app.input().set_escape_modal(false);
        if (in_vehicle) {
          app.camera().vehicle_seated = true;
          app.camera().fly_mode = false;
        }
        fury::Log::info("Photo mode OFF (Esc)");
        return;
      }
      app.camera().fly_mode = true;
      app.camera().vehicle_seated = false;
      day_night.update(dt);
      fury::Lighting framed_photo = day_night.apply(base_lit);
      app.renderer().set_lighting(framed_photo);
      app.config().clear_color = day_night.sky_clear();
      return;
    }

    // Replay scrub — freeze sim; A/D rewind camera along ring buffer + ghost path
    if (replay.scrubbing) {
      if (input.escape_pressed) {
        replay.end_scrub(app.camera());
        app.input().set_cinematic(false);
        app.input().set_escape_modal(false);
        for (auto& ent : app.scene().entities()) {
          if (ent.tag == "replay_ghost") ent.visible = false;
        }
        fury::Log::info("Replay scrub OFF (Esc)");
        return;
      }
      {
        const Uint8* keys_sc = SDL_GetKeyboardState(nullptr);
        float du = 0.f;
        if (keys_sc[SDL_SCANCODE_A] || keys_sc[SDL_SCANCODE_LEFT]) du -= 0.35f * dt;
        if (keys_sc[SDL_SCANCODE_D] || keys_sc[SDL_SCANCODE_RIGHT]) du += 0.35f * dt;
        if (du != 0.f) {
          replay.scrub(du);
        }
      }
      replay.apply_to_camera(app.camera());
      // Place ghost trail markers along recorded path
      {
        const std::size_t stride = replay.ghost_stride();
        int gi = 0;
        for (auto& ent : app.scene().entities()) {
          if (ent.tag != "replay_ghost") continue;
          const std::size_t chrono = static_cast<std::size_t>(gi) * stride;
          if (chrono < replay.count) {
            const fury::ReplaySample s = replay.at_chrono(chrono);
            ent.transform.position = s.position;
            ent.visible = true;
            // Highlight nearest-to-scrub sample
            const float u_g = (replay.count <= 1)
                                  ? 1.f
                                  : static_cast<float>(chrono) /
                                        static_cast<float>(replay.count - 1);
            const bool near = std::fabs(u_g - replay.scrub_u) < 0.06f;
            ent.material.emissive = near ? 3.2f : 1.2f;
            ent.transform.scale = near ? Vec3{1.6f, 1.6f, 1.6f}
                                       : Vec3{1.f, 1.f, 1.f};
          } else {
            ent.visible = false;
          }
          ++gi;
        }
      }
      day_night.update(dt);
      fury::Lighting framed_rp = day_night.apply(base_lit);
      app.renderer().set_lighting(framed_rp);
      app.config().clear_color = day_night.sky_clear();
      return;
    }

    // K — toggle local ready (synced via PlayerState flags + crew pips)
    if (!chat_open && !lobby_open && input.key_k) {
      local_ready = !local_ready;
      net_client->set_crew_ready(10, local_ready);
      net_client->set_crew_ready(11, local_ready);
      fury::Log::info(local_ready ? "Ready ON (K)" : "Ready OFF (K)");
    }

    // L — toggle pre-heist lobby panel (also auto-opens when all ready)
    {
      const Uint8* keys_l = SDL_GetKeyboardState(nullptr);
      const bool l_down = keys_l[SDL_SCANCODE_L] != 0;
      if (!chat_open && !smoke_mode && !help_panel.open && l_down && !l_was_down) {
        lobby_open = !lobby_open;
        if (lobby_open) {
          buy_menu.open = false;
          mission_board.open = false;
          quest_journal.open = false;
          inv_panel.open = false;
          rep_panel.open = false;
          skill_panel.open = false;
          craft_panel.open = false;
          help_panel.open = false;
          map_panel.open = false;
          app.input().set_cinematic(true);
          fury::Log::info(std::string("Lobby OPEN — mission: ") +
                          mission_board.current().title +
                          (is_net_host ? " | host: Enter to Start" : " | waiting on host"));
        } else {
          lobby_auto_armed = false;
          if (!help_panel.open) app.input().set_cinematic(false);
          fury::Log::info("Lobby closed (L)");
        }
      }
      l_was_down = l_down;
    }

    // Auto-open lobby when local + remotes + crew all ready (pre-heist)
    {
      bool all_ready = local_ready;
      for (const auto& rp : net_client->remote_players()) {
        if (!rp.ready) all_ready = false;
      }
      for (const auto& c : net_client->crew_roster()) {
        if (!c.ready) all_ready = false;
      }
      if (!all_ready) {
        lobby_auto_armed = true;
      } else if (lobby_auto_armed && !lobby_open && !chat_open && !smoke_mode &&
                 heist.phase() == fury::HeistPhase::Idle) {
        lobby_open = true;
        lobby_auto_armed = false;
        buy_menu.open = false;
        mission_board.open = false;
        quest_journal.open = false;
        inv_panel.open = false;
        rep_panel.open = false;
        skill_panel.open = false;
        craft_panel.open = false;
        help_panel.open = false;
        map_panel.open = false;
        app.input().set_cinematic(true);
        fury::Log::info(std::string("Lobby AUTO — all ready | ") +
                        mission_board.current().title +
                        (is_net_host ? " | Enter to Start" : " | waiting on host"));
      }
    }

    // P — toggle FPS overlay + log
    if (!chat_open) {
      const Uint8* keys_fps = SDL_GetKeyboardState(nullptr);
      const bool p_down = keys_fps[SDL_SCANCODE_P] != 0;
      if (p_down && !p_was_down) {
        show_fps = !show_fps;
        app.config().log_fps = show_fps;
        fury::Log::info(show_fps ? "FPS overlay ON (P)" : "FPS overlay OFF (P)");
      }
      p_was_down = p_down;

      // F6 — cycle graphics quality (low/med/high); [ ] reserved for save slots
      const bool f6_down = keys_fps[SDL_SCANCODE_F6] != 0;
      if (f6_down && !f6_was_down) {
        quality.cycle();
        quality.apply_to_lighting(base_lit);
        app.config().cull_distance = quality.cull_distance;
        app.config().lod_mid_distance = quality.cull_distance * 0.5f;
        app.camera().far_plane = quality.camera_far;
        app.renderer().set_shadow_map_size(quality.shadow_map_size);
        // Re-apply current framed lighting path on next frame via base_lit
        quality_tip_timer = 2.5f;
        fury::Log::info(std::string("Quality -> ") + quality.name() +
                        " (cull=" + std::to_string(static_cast<int>(quality.cull_distance)) +
                        "m shadow=" + std::to_string(quality.shadow_map_size) +
                        "x" + std::to_string(quality.shadow_cascade_count) +
                        " bloom=" + (quality.enable_bloom ? "on" : "off") +
                        " reflect=" + (quality.enable_reflections ? "on" : "off") +
                        " fog=" + std::to_string(static_cast<int>(quality.fog_start)) +
                        "-" + std::to_string(static_cast<int>(quality.fog_end)) + ")");
      }
      f6_was_down = f6_down;

      // F8 — toggle audio mute (ambience hooks still update)
      const bool f8_down = keys_fps[SDL_SCANCODE_F8] != 0;
      if (f8_down && !f8_was_down) {
        audio->toggle_mute();
        mute_tip_timer = 2.0f;
      }
      f8_was_down = f8_down;
    }
    if (quality_tip_timer > 0.f) {
      quality_tip_timer -= dt;
    }
    if (mute_tip_timer > 0.f) {
      mute_tip_timer -= dt;
    }

    // Onboarding tip log lines (once per step)
    if (onboard_step != onboard_tip_logged && splash_remaining <= 0.f) {
      onboard_tip_logged = onboard_step;
      if (onboard_step == 0) {
        fury::Log::info("TIP: Press M — open the mission board and pick a job (1/2/3/4/5/6)");
      } else if (onboard_step == 1) {
        fury::Log::info("TIP: Follow the gold compass/minimap blip to the target — press E to breach");
      } else if (onboard_step == 2) {
        fury::Log::info("TIP: Reach the green extraction pad (or drive the getaway van with F/E)");
      } else if (onboard_step == 3 && session.successes > 0) {
        fury::Log::info("TIP: Slice complete — press E to reset, B near Ashcourt fence to spend cash");
      }
    }

    // Finale night-forced lighting cue (keep TOD near midnight while selected)
    if (mission_board.is_finale()) {
      const float night_target = 0.92f;
      const float blend = (std::min)(1.f, dt * 0.85f);
      day_night.time_of_day += (night_target - day_night.time_of_day) * blend;
      if (day_night.time_of_day < 0.f) day_night.time_of_day += 1.f;
      if (day_night.time_of_day >= 1.f) day_night.time_of_day -= 1.f;
    }
    day_night.update(dt);
    fury::Lighting framed = day_night.apply(base_lit);

    // R — cycle weather stub (clear / rain / storm / auto-drizzle)
    {
      const Uint8* keys_w = SDL_GetKeyboardState(nullptr);
      const bool r_down = keys_w[SDL_SCANCODE_R] != 0;
      if (!chat_open && r_down && !r_was_down) {
        weather.cycle();
        fury::Log::info(std::string("Weather -> ") + weather.mode_name());
      }
      r_was_down = r_down;
    }
    const float rain = weather.intensity(day_night.time_of_day);
    const float rain01 = std::clamp(rain, 0.f, 1.f);
    framed = weather.apply(framed, rain);
    {
      const float night = day_night.night_factor();
      audio->set_ambience(1.f - night, night, rain01);
    }

    // Wetter asphalt tint
    for (const auto& dry : asphalt_dry) {
      if (auto* ent = app.scene().find_by_name(dry.name)) {
        weather.tint_asphalt(ent->material.albedo, ent->material.roughness,
                             ent->material.metallic, ent->material.wetness,
                             dry.albedo, dry.roughness, dry.metallic, rain01);
      }
    }

    // 3.7.0 puddles — dark reflective patches when wet
    {
      const bool wet = rain01 > 0.08f;
      for (Entity& ent : app.scene().entities()) {
        if (ent.tag != "puddle") {
          continue;
        }
        ent.visible = wet;
        if (wet) {
          ent.material.wetness = rain01;
          ent.material.roughness = 0.08f + 0.10f * (1.f - rain01);
          ent.material.metallic = 0.55f + 0.30f * rain01;
          ent.material.albedo = {0.06f + 0.04f * (1.f - rain01),
                                 0.08f + 0.04f * (1.f - rain01),
                                 0.12f + 0.05f * (1.f - rain01)};
        }
      }
    }

    // Rain particle streaks near camera (storm = heavier)
    if (rain > 0.05f) {
      const float rate = 18.f + 55.f * rain + (weather.is_storm() ? 35.f : 0.f);
      rain_emit_accum += dt * rate;
      const int n = static_cast<int>(rain_emit_accum);
      if (n > 0) {
        rain_emit_accum -= static_cast<float>(n);
        particles.emit_rain_streaks(app.camera().position, n,
                                    16.f + 6.f * rain01 +
                                        (weather.is_storm() ? 4.f : 0.f));
      }
    } else {
      rain_emit_accum = 0.f;
    }

    // 3.7.0 lightning — occasional screen flash + thunder + ambient spike
    if (lightning_flash > 0.f) {
      lightning_flash = (std::max)(0.f, lightning_flash - dt * 4.2f);
    }
    {
      const float mean = weather.lightning_interval_mean();
      if (mean > 0.f && rain > 0.12f) {
        lightning_cd -= dt;
        if (lightning_cd <= 0.f) {
          weather_rng = weather_rng * 1664525u + 1013904223u;
          const float u =
              static_cast<float>((weather_rng >> 8) & 0xffffffu) / 16777215.f;
          const float jitter = 0.55f + u * 1.1f;
          lightning_cd = mean * jitter;
          lightning_flash = weather.is_storm() ? 1.f : 0.72f;
          audio->play_cue("thunder");
        }
      } else {
        lightning_cd = (std::max)(lightning_cd, 1.5f);
      }
    }
    if (lightning_flash > 0.01f) {
      const float f = std::clamp(lightning_flash, 0.f, 1.f);
      framed.ambient =
          framed.ambient + Vec3{0.55f, 0.62f, 0.85f} * (0.85f * f);
      framed.sun_intensity += 1.15f * f;
      framed.sun_color =
          framed.sun_color * (1.f - 0.35f * f) + Vec3{0.85f, 0.90f, 1.05f} * (0.35f * f);
    }

    // Pick up to 3 nearest street lamps as dynamic point lights (night readable)
    {
      struct Cand { float d2; Vec3 pos; };
      std::vector<Cand> cands;
      cands.reserve(lamp_positions.size());
      const Vec3 cam = app.camera().position;
      for (const Vec3& lp : lamp_positions) {
        const float dx = lp.x - cam.x;
        const float dz = lp.z - cam.z;
        cands.push_back({dx * dx + dz * dz, lp});
      }
      std::sort(cands.begin(), cands.end(),
                [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
      const int n = (std::min)(3, static_cast<int>(cands.size()));
      framed.point_light_count = n;
      const float night = day_night.night_factor();
      for (int i = 0; i < n; ++i) {
        fury::PointLight pl;
        pl.position = cands[static_cast<std::size_t>(i)].pos;
        pl.color = {1.f, 0.92f, 0.62f};
        pl.intensity = 0.55f + 1.55f * night;
        pl.radius = 16.f + 6.f * night;
        framed.point_lights[i] = pl;
      }
    }

    // 2.5.0 interior lighting zones — boost ambient, enable extra fills, dim exterior
    // 2.8.0 occlusion-lite sector hide when deep indoors (not near a door)
    active_interior_tag = "";
    app.config().sector_hide = false;
    if (const fury::InteriorZone* iz = interiors.zone_at(app.camera().position)) {
      active_interior_tag = iz->tag;
      fury::InteriorCatalog::apply_zone_lighting(framed, *iz);

      bool near_door = false;
      const Vec3& p = app.camera().position;
      for (const auto& d : interiors.doors) {
        if (std::strcmp(d.zone_tag, iz->tag) != 0) {
          continue;
        }
        if (std::fabs(p.x - d.center.x) <= d.half_extents.x + 1.6f &&
            std::fabs(p.y - d.center.y) <= d.half_extents.y + 1.0f &&
            std::fabs(p.z - d.center.z) <= d.half_extents.z + 1.6f) {
          near_door = true;
          break;
        }
      }
      const bool deep_core =
          std::fabs(p.x - iz->center.x) <= iz->half_extents.x * 0.72f &&
          std::fabs(p.y - iz->center.y) <= iz->half_extents.y * 0.85f &&
          std::fabs(p.z - iz->center.z) <= iz->half_extents.z * 0.72f;
      if (deep_core && !near_door) {
        app.config().sector_hide = true;
        app.config().sector_focus = Aabb{
            iz->center,
            {iz->half_extents.x * 1.1f, iz->half_extents.y * 1.15f,
             iz->half_extents.z * 1.1f}};
      }
    }
    app.renderer().set_lighting(framed);
    app.config().clear_color = day_night.sky_clear();

    const float lamp_mul = day_night.lamp_emissive_mul();
    const float night = day_night.night_factor();
    for (auto& ent : app.scene().entities()) {
      if (ent.tag == "lamp") {
        ent.material.emissive = lamp_mul;
      } else if (ent.tag == "window") {
        ent.material.emissive = 0.08f + 2.4f * night;
      }
    }

    // E also enters/exits vehicle when close (without starting a vault breach if seated)
    if (in_vehicle) {
      try_toggle_vehicle(input.interact_pressed);
    } else if (nearest_driveable(app.camera().position) >= 0 &&
               input.interact_pressed) {
      try_toggle_vehicle(true);
    }

    if (seated_vehicle >= 0) {
      DriveableSlot& slot = driveables[seated_vehicle];
      slot.pos = {app.camera().position.x,
                  slot.kind == DriveKind::Van ? 1.2f : 0.85f,
                  app.camera().position.z};
      slot.yaw = app.camera().yaw;
      sync_vehicle_entity();
    }

    // Headlights emissive at night while driving that vehicle
    {
      const float night = day_night.night_factor();
      const bool lit = in_vehicle && night > 0.35f;
      const float glow = lit ? (1.2f + 3.8f * night) : 0.05f;
      auto set_heads = [&](const char* a, const char* b, bool active) {
        const float e = active ? glow : 0.05f;
        if (auto* L = app.scene().find_by_name(a)) L->material.emissive = e;
        if (auto* R = app.scene().find_by_name(b)) R->material.emissive = e;
      };
      set_heads("GetawayVanHeadL", "GetawayVanHeadR",
                lit && seated_vehicle == 0);
      set_heads("CivSedanHeadL", "CivSedanHeadR",
                lit && seated_vehicle == 1);
    }

    // Radio stub — C cycles stations while seated (log + HUD pip + optional beep)
    {
      const Uint8* keys_c = SDL_GetKeyboardState(nullptr);
      const bool c_down = keys_c[SDL_SCANCODE_C] != 0;
      if (in_vehicle && !chat_open && !help_panel.open && !smoke_mode && c_down &&
          !c_was_down) {
        radio_station = (radio_station + 1) % 3;
        fury::Log::info(std::string("Radio: ") + kRadioStations[radio_station] +
                        " (C to cycle)");
        audio->play_cue("radio_tick");
      }
      c_was_down = c_down;
    }

    // Footstep audio hooks (silent backend OK) — walk cadence only
    {
      const Vec3 pos = app.camera().position;
      const float dx = pos.x - foot_last_pos.x;
      const float dz = pos.z - foot_last_pos.z;
      const float dist = std::sqrt(dx * dx + dz * dz);
      foot_last_pos = pos;
      const bool walking = !in_vehicle && !app.camera().fly_mode &&
                           !app.camera().vehicle_seated;
      if (walking && dist > 1e-4f) {
        footstep_accum += dist;
        const float stride = app.camera().crouching
                                 ? 1.85f
                                 : (input.key_shift ? 1.05f : 1.35f);
        while (footstep_accum >= stride) {
          footstep_accum -= stride;
          audio->play_cue("footstep");
        }
      } else if (!walking) {
        footstep_accum = 0.f;
      }
    }

    // Guard chase when heat is elevated
    Vec3 guard_pos{4.f, 0.f, -2.f};
    for (auto& agent : npcs.agents()) {
      if (agent.kind == fury::NpcKind::Guard) {
        agent.chasing = heat.normalized() >= 0.45f;
        agent.chase_target = app.camera().position;
        guard_pos = agent.position;
      }
    }

    // Skip far NPC sim (non-chasing) — tighter than render cull for CPU
    constexpr float kNpcUpdateDist = 70.f;
    perf_npc_total = static_cast<int>(npcs.agents().size());
    perf_npc_updated = 0;
    if (perf_log) {
      const Vec3 focus = app.camera().position;
      const float max2 = kNpcUpdateDist * kNpcUpdateDist;
      for (const auto& agent : npcs.agents()) {
        if (agent.chasing && agent.kind == fury::NpcKind::Guard) {
          ++perf_npc_updated;
          continue;
        }
        const float dx = agent.position.x - focus.x;
        const float dz = agent.position.z - focus.z;
        if (dx * dx + dz * dz <= max2) ++perf_npc_updated;
      }
    }
    npcs.update(dt, app.camera().position, kNpcUpdateDist);
    for (const auto& agent : npcs.agents()) {
      if (auto* ent = app.scene().find_by_name(agent.entity_name)) {
        ent->transform.position = agent.position;
        ent->transform.rotation_euler.y = agent.yaw;
        if (ent->mesh) {
          fury::pose_humanoid(*ent->mesh, agent.height, ent->material.albedo,
                              agent.anim_phase);
        }
      }
    }
    for (const auto& agent : npcs.agents()) {
      if (agent.kind == fury::NpcKind::Guard) {
        guard_pos = agent.position;
      }
    }

    // M = mission board; B = buy menu; 1/2/3 select/buy; T = cycle; [ ] slots
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    const bool can_retarget =
        heist.phase() == fury::HeistPhase::Idle ||
        heist.phase() == fury::HeistPhase::Success ||
        heist.phase() == fury::HeistPhase::Failed;

    // --- NPC nameplates / approach log / Q dialogue (3.2.0) -----------------
    nameplate_show = false;
    focus_npc_id.clear();
    focus_npc_label.clear();
    {
      const Vec3& cam = app.camera().position;
      const float yaw = app.camera().yaw;
      const float fx = std::sin(yaw);
      const float fz = std::cos(yaw);
      constexpr float kTalkRadius = 5.5f;
      constexpr float kLookDot = 0.55f;
      float best_score = -1.f;
      auto consider = [&](const char* id, const char* label, fury::DialogueRole role,
                          const Vec3& pos) {
        const float dx = pos.x - cam.x;
        const float dz = pos.z - cam.z;
        const float d = std::sqrt(dx * dx + dz * dz);
        if (d > kTalkRadius || d < 1e-3f) {
          return;
        }
        const float inv = 1.f / d;
        const float look = dx * inv * fx + dz * inv * fz;
        if (look < kLookDot) {
          return;
        }
        const float score = look * 2.f + (1.f - d / kTalkRadius);
        if (score > best_score) {
          best_score = score;
          focus_npc_id = id;
          focus_npc_label = label;
          focus_role = role;
          nameplate_show = true;
          nameplate_role = role;
          nameplate_fill = 0.35f + 0.55f * std::clamp(look, 0.f, 1.f);
        }
      };
      for (const auto& agent : npcs.agents()) {
        fury::DialogueRole role = fury::DialogueRole::Civilian;
        if (agent.kind == fury::NpcKind::Guard) {
          role = fury::DialogueRole::Guard;
        } else if (agent.kind == fury::NpcKind::Fence) {
          role = fury::DialogueRole::Fence;
        }
        consider(agent.entity_name.c_str(), agent.label(), role, agent.position);
      }
      for (const auto& cm : crew.members()) {
        if (!cm.active) continue;
        consider(cm.entity_name.c_str(), cm.label(), fury::DialogueRole::Crew,
                 cm.position);
      }
      // Approach log — once when a named NPC newly enters focus
      if (!focus_npc_id.empty() && focus_npc_id != approach_logged_id) {
        approach_logged_id = focus_npc_id;
        fury::Log::info(std::string("[NPC] ") + focus_npc_label + " (" +
                        fury::DialogueBarks::role_label(focus_role) +
                        ") nearby — press Q to talk");
      }
      if (focus_npc_id.empty()) {
        approach_logged_id.clear();
      }
    }
    // Q — bark dialogue with focused named NPC
    {
      const Uint8* keys_q = SDL_GetKeyboardState(nullptr);
      const bool q_down = keys_q[SDL_SCANCODE_Q] != 0;
      if (!chat_open && !help_panel.open && !smoke_mode && !lobby_open &&
          !photo_mode.active && !replay.scrubbing && q_down && !q_was_down) {
        if (!focus_npc_id.empty()) {
          dialogue_barks.pick(focus_role, dialogue_lines);
          dialogue_line_count = static_cast<int>(dialogue_lines.size());
          dialogue_role = focus_role;
          dialogue_speaker = focus_npc_label;
          dialogue_timer = 4.2f;
          fury::Log::info(std::string("[TALK] ") + dialogue_speaker + " (" +
                          fury::DialogueBarks::role_label(dialogue_role) + "):");
          for (const auto& line : dialogue_lines) {
            fury::Log::info(std::string("  ") + line);
          }
        } else {
          fury::Log::info("No named NPC in view — look near civilians / guard / fence / crew, then Q");
        }
      }
      q_was_down = q_down;
    }

    const bool near_shop =
        dist_xz(app.camera().position, kAshcourtShopPos) <= kShopRadius;

    const bool m_down = keys[SDL_SCANCODE_M] != 0;
    if (!chat_open && !help_panel.open && m_down && !m_was_down) {
      mission_board.toggle();
      if (mission_board.open) {
        buy_menu.open = false;
        quest_journal.open = false;
        inv_panel.open = false;
        rep_panel.open = false;
        help_panel.open = false;
        skill_panel.open = false;
        craft_panel.open = false;
        map_panel.open = false;
      }
      fury::Log::info(mission_board.open ? "Mission board OPEN (1/2/3/4/5 to select)"
                                         : "Mission board closed");
      fury::Log::info(mission_board.status_line());
      if (mission_board.open && onboard_step == 0) {
        onboard_step = 1;
      }
    }
    m_was_down = m_down;

    const bool b_down = keys[SDL_SCANCODE_B] != 0;
    if (!chat_open && !help_panel.open && b_down && !b_was_down) {
      buy_menu.open = !buy_menu.open;
      if (buy_menu.open) {
        mission_board.open = false;
        quest_journal.open = false;
        inv_panel.open = false;
        rep_panel.open = false;
        help_panel.open = false;
        skill_panel.open = false;
        craft_panel.open = false;
        map_panel.open = false;
      }
      fury::Log::info(buy_menu.open
                          ? (near_shop
                                 ? "Fence OPEN — 1-3 perks; 4 Better Payouts; 5 Quieter Tools; L/R chip; S sell"
                                 : "Fence OPEN — approach Ashcourt shop to buy/sell")
                          : "Fence menu closed");
    }
    b_was_down = b_down;

    const bool i_down = keys[SDL_SCANCODE_I] != 0;
    if (!chat_open && !help_panel.open && i_down && !i_was_down) {
      inv_panel.open = !inv_panel.open;
      if (inv_panel.open) {
        mission_board.open = false;
        buy_menu.open = false;
        quest_journal.open = false;
        rep_panel.open = false;
        help_panel.open = false;
        skill_panel.open = false;
        craft_panel.open = false;
        map_panel.open = false;
      }
      if (inv_panel.open) {
        std::ostringstream inv_oss;
        inv_oss << "Inventory OPEN — cash=$" << heist.inventory().cash
                << " Bond=" << heist.inventory().chips[0]
                << " Sapphire=" << heist.inventory().chips[1]
                << " Drive=" << heist.inventory().chips[2];
        fury::Log::info(inv_oss.str());
      } else {
        fury::Log::info("Inventory closed");
      }
    }
    i_was_down = i_down;

    const bool u_down = keys[SDL_SCANCODE_U] != 0;
    if (!chat_open && !help_panel.open && u_down && !u_was_down) {
      rep_panel.open = !rep_panel.open;
      if (rep_panel.open) {
        mission_board.open = false;
        buy_menu.open = false;
        quest_journal.open = false;
        inv_panel.open = false;
        help_panel.open = false;
        skill_panel.open = false;
        craft_panel.open = false;
        map_panel.open = false;
        fury::Log::info(std::string("Reputation OPEN (U) — ") +
                        factions.status_line());
      } else {
        fury::Log::info("Reputation closed");
      }
    }
    u_was_down = u_down;

    const bool j_down = keys[SDL_SCANCODE_J] != 0;
    if (!chat_open && !help_panel.open && j_down && !j_was_down) {
      quest_journal.toggle();
      if (quest_journal.open) {
        mission_board.open = false;
        buy_menu.open = false;
        inv_panel.open = false;
        rep_panel.open = false;
        help_panel.open = false;
        skill_panel.open = false;
        craft_panel.open = false;
        map_panel.open = false;
      }
      fury::Log::info(quest_journal.open ? "Quest journal OPEN (J)"
                                         : "Quest journal closed");
      fury::Log::info(quest_journal.status_line());
    }
    j_was_down = j_down;

    const bool n_down = keys[SDL_SCANCODE_N] != 0;
    if (!chat_open && !help_panel.open && n_down && !n_was_down) {
      skill_panel.open = !skill_panel.open;
      if (skill_panel.open) {
        mission_board.open = false;
        buy_menu.open = false;
        quest_journal.open = false;
        inv_panel.open = false;
        rep_panel.open = false;
        help_panel.open = false;
        craft_panel.open = false;
        map_panel.open = false;
        fury::Log::info(std::string("Skills OPEN (N) — ") + skills.status_line());
        fury::Log::info("1/2/3 unlock Silent Entry / Fast Hands / Cool Under Heat (100 XP each)");
      } else {
        fury::Log::info("Skills closed");
      }
    }
    n_was_down = n_down;

    // G — loft workbench craft panel (near workbench / in loft)
    const bool near_workbench =
        dist_xz(app.camera().position, kLoftWorkbenchPos) <= kWorkbenchRadius;
    const bool g_down = keys[SDL_SCANCODE_G] != 0;
    if (!chat_open && !help_panel.open && g_down && !g_was_down) {
      if (!near_workbench && !in_safehouse) {
        fury::Log::info("Craft bench is at Harbor loft — enter loft and press G");
      } else {
        craft_panel.open = !craft_panel.open;
        if (craft_panel.open) {
          mission_board.open = false;
          buy_menu.open = false;
          quest_journal.open = false;
          inv_panel.open = false;
          rep_panel.open = false;
          help_panel.open = false;
          skill_panel.open = false;
          map_panel.open = false;
          fury::Log::info(std::string("Craft OPEN (G) — ") + craft.status_line());
          fury::Log::info(
              "1 SignalJammer (BearerBond+LedgerDrive); 2 SmokePellet (Sapphire+BearerBond); "
              "X uses SmokePellet anywhere");
        } else {
          fury::Log::info("Craft closed");
        }
      }
    }
    g_was_down = g_down;

    // X — use SmokePellet (instant heat drop once)
    const bool x_down = keys[SDL_SCANCODE_X] != 0;
    if (!chat_open && !help_panel.open && x_down && !x_was_down) {
      if (craft.try_use_smoke(heat.value)) {
        visibility.value = (std::max)(0.f, visibility.value - 0.35f);
        particles.emit_burst(app.camera().position + Vec3{0.f, 1.0f, 0.f}, 28, 6.f);
        audio->play_cue("impact");
        fury::Log::info(std::string("SmokePellet used — heat dump (remaining ") +
                        std::to_string(craft.smoke_pellet) + ")");
        autosave_slot();
      } else {
        fury::Log::info("No SmokePellet — craft at loft workbench (G, recipe 2)");
      }
    }
    x_was_down = x_down;

    const bool bl = keys[SDL_SCANCODE_LEFTBRACKET] != 0;
    const bool br = keys[SDL_SCANCODE_RIGHTBRACKET] != 0;
    if (!chat_open && bl && !bracket_l_was) {
      autosave_slot();
      load_slot((active_slot + kSaveSlotCount - 1) % kSaveSlotCount);
      apply_target();
    }
    if (!chat_open && br && !bracket_r_was) {
      autosave_slot();
      load_slot((active_slot + 1) % kSaveSlotCount);
      apply_target();
    }
    bracket_l_was = bl;
    bracket_r_was = br;

    const SDL_Scancode digit_scans[6] = {
        SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
        SDL_SCANCODE_5, SDL_SCANCODE_6};
    const int perk_costs[3] = {3500, 4500, 4000};  // affordable after one Meridian
    for (int i = 0; i < 6; ++i) {
      const bool down = keys[digit_scans[i]] != 0;
      if (!chat_open && down && !digit_was_down[i + 1]) {
        if (craft_panel.open) {
          if (i == 0) {
            if (!near_workbench && !in_safehouse) {
              fury::Log::info("Too far from loft workbench");
            } else if (craft.signal_jammer > 0) {
              fury::Log::info("SignalJammer already owned");
            } else if (!craft.can_craft_jammer(heist.inventory())) {
              fury::Log::info("Need BearerBond + LedgerDrive for SignalJammer");
            } else if (craft.try_craft_jammer(heist.inventory())) {
              fury::Log::info("Crafted SignalJammer — camera heat reduced while owned");
              autosave_slot();
            }
          } else if (i == 1) {
            if (!near_workbench && !in_safehouse) {
              fury::Log::info("Too far from loft workbench");
            } else if (!craft.can_craft_smoke(heist.inventory())) {
              fury::Log::info("Need Sapphire + BearerBond for SmokePellet");
            } else if (craft.try_craft_smoke(heist.inventory())) {
              fury::Log::info(std::string("Crafted SmokePellet x") +
                              std::to_string(craft.smoke_pellet) +
                              " — press X to dump heat");
              autosave_slot();
            }
          }
        } else if (skill_panel.open) {
          if (i < 3) {
            const auto sid = static_cast<fury::SkillId>(i);
            if (skills.unlocked(sid)) {
              fury::Log::info(std::string(fury::skill_name(sid)) + " already unlocked");
            } else if (skills.xp < fury::SkillTree::kUnlockCost) {
              fury::Log::info(std::string("Need ") +
                              std::to_string(fury::SkillTree::kUnlockCost) +
                              " XP for " + fury::skill_name(sid) + " (have " +
                              std::to_string(skills.xp) + ")");
            } else if (skills.try_unlock(sid)) {
              fury::Log::info(std::string("Unlocked ") + fury::skill_name(sid) +
                              " (-" + std::to_string(fury::SkillTree::kUnlockCost) +
                              " XP) — " + fury::skill_blurb(sid));
              // Refresh breach duration if Silent Entry just unlocked mid-idle
              if (sid == fury::SkillId::SilentEntry &&
                  (heist.phase() == fury::HeistPhase::Idle ||
                   heist.phase() == fury::HeistPhase::Success ||
                   heist.phase() == fury::HeistPhase::Failed)) {
                apply_target();
              }
              autosave_slot();
            }
          }
        } else if (buy_menu.open) {
          if (i < 3) {
            if (!near_shop) {
              fury::Log::info("Too far from Ashcourt fence shop");
            } else {
              int* lvl = (i == 0)   ? &perks.crew
                         : (i == 1) ? &perks.heat_damp
                                    : &perks.loot_speed;
              const float price_mul = factions.shop_price_mul();
              const int cost = static_cast<int>(
                  static_cast<float>(perk_costs[i] * (*lvl + 1)) * price_mul +
                  0.5f);
              if (*lvl >= 3) {
                fury::Log::info("Perk already maxed (3)");
              } else if (heist.inventory().cash < cost) {
                fury::Log::info(std::string("Need $") + std::to_string(cost) +
                                " for perk");
              } else {
                heist.inventory().cash -= cost;
                ++(*lvl);
                const char* names[3] = {"Crew perk", "Heat dampener", "Loot speed"};
                std::ostringstream buy_oss;
                buy_oss << "Purchased " << names[i] << " L" << *lvl << " (-$"
                        << cost << ")";
                if (price_mul < 0.999f) {
                  buy_oss << " [Pierline discount x" << price_mul << "]";
                }
                fury::Log::info(buy_oss.str());
                autosave_slot();
              }
            }
          } else if (i == 3 || i == 4) {
            // Permanent fence upgrades: 4 Better Payouts, 5 Quieter Tools
            if (!near_shop) {
              fury::Log::info("Too far from Ashcourt fence shop");
            } else {
              const float price_mul = factions.shop_price_mul();
              if (i == 3) {
                if (fence_up.better_payouts) {
                  fury::Log::info("Better Payouts already unlocked");
                } else {
                  const int cost = static_cast<int>(
                      static_cast<float>(fury::FenceUpgrades::kBetterPayoutsCost) *
                          price_mul +
                      0.5f);
                  if (heist.inventory().cash < cost) {
                    fury::Log::info(std::string("Need $") + std::to_string(cost) +
                                    " for Better Payouts (+10%)");
                  } else {
                    heist.inventory().cash -= cost;
                    fence_up.better_payouts = true;
                    apply_target();
                    fury::Log::info(std::string("Unlocked Better Payouts (+10%) (-$") +
                                    std::to_string(cost) + ")");
                    autosave_slot();
                  }
                }
              } else {
                if (fence_up.quieter_tools) {
                  fury::Log::info("Quieter Tools already unlocked");
                } else {
                  const int cost = static_cast<int>(
                      static_cast<float>(fury::FenceUpgrades::kQuieterToolsCost) *
                          price_mul +
                      0.5f);
                  if (heist.inventory().cash < cost) {
                    fury::Log::info(std::string("Need $") + std::to_string(cost) +
                                    " for Quieter Tools");
                  } else {
                    heist.inventory().cash -= cost;
                    fence_up.quieter_tools = true;
                    apply_target();
                    fury::Log::info(
                        std::string("Unlocked Quieter Tools (-$") +
                        std::to_string(cost) +
                        ") — synergy with Silent Entry shortens breach");
                    autosave_slot();
                  }
                }
              }
            }
          }
        } else if (can_retarget) {
          if (mission_board.select(i)) {
            apply_target();
          } else {
            fury::Log::info(mission_board.status_line());
          }
        }
      }
      digit_was_down[i + 1] = down;
    }

    // Fence sell: Left/Right select chip type; S sells one when near shop
    const bool left_down = keys[SDL_SCANCODE_LEFT] != 0;
    const bool right_down = keys[SDL_SCANCODE_RIGHT] != 0;
    if (!chat_open && buy_menu.open && left_down && !left_was) {
      buy_menu.sell_selected =
          (buy_menu.sell_selected + 2) % static_cast<int>(fury::LootChip::Count);
      fury::Log::info(std::string("Sell select: ") +
                      fury::loot_chip_name(static_cast<fury::LootChip>(
                          buy_menu.sell_selected)));
    }
    if (!chat_open && buy_menu.open && right_down && !right_was) {
      buy_menu.sell_selected =
          (buy_menu.sell_selected + 1) % static_cast<int>(fury::LootChip::Count);
      fury::Log::info(std::string("Sell select: ") +
                      fury::loot_chip_name(static_cast<fury::LootChip>(
                          buy_menu.sell_selected)));
    }
    left_was = left_down;
    right_was = right_down;

    const bool s_down = keys[SDL_SCANCODE_S] != 0;
    if (!chat_open && buy_menu.open && s_down && !s_was_down) {
      if (!near_shop) {
        fury::Log::info("Too far from Ashcourt fence shop to sell");
      } else {
        const auto chip = static_cast<fury::LootChip>(buy_menu.sell_selected);
        const int price = fury::loot_chip_sell_price(chip);
        if (heist.inventory().take_chip(chip, 1) == 1) {
          heist.inventory().cash += price;
          factions.on_fence_sell();
          fury::Log::info(std::string("Sold ") + fury::loot_chip_name(chip) +
                          " +$" + std::to_string(price) +
                          " (Syndicate tension " +
                          std::to_string(factions.syndicate) + ")");
          autosave_slot();
        } else {
          fury::Log::info(std::string("No ") + fury::loot_chip_name(chip) +
                          " to sell");
        }
      }
    }
    s_was_down = s_down;

    const bool t_down = keys[SDL_SCANCODE_T] != 0;
    if (!chat_open && t_down && !t_was_down && can_retarget && !buy_menu.open) {
      int next = (mission_board.selected + 1) % static_cast<int>(fury::kMissionCount);
      if (next == fury::kFinaleMissionIndex &&
          !quest_journal.finale_unlocked(unlock_all)) {
        next = 0;  // wrap past locked finale
        fury::Log::info("Finale locked — cycling past Meridian Night Vault");
      }
      mission_board.selected = next;
      apply_target();
    }
    t_was_down = t_down;

    // Crew follows during active heist phases
    const bool crew_follow =
        heist.phase() == fury::HeistPhase::Breach ||
        heist.phase() == fury::HeistPhase::Looting ||
        heist.phase() == fury::HeistPhase::Escape ||
        heist.phase() == fury::HeistPhase::Approach;
    crew.update(dt, app.camera().position, app.camera().yaw, crew_follow);
    for (const auto& cm : crew.members()) {
      if (auto* ent = app.scene().find_by_name(cm.entity_name)) {
        ent->transform.position = cm.position;
        ent->transform.rotation_euler.y = cm.yaw;
        ent->visible = true;
        if (ent->mesh) {
          fury::pose_humanoid(*ent->mesh, cm.height, ent->material.albedo,
                              cm.anim_phase);
        }
      }
    }

    // Player third-person body + walk limb swing (hidden in fly / first-person / van)
    {
      const bool show_body = app.camera().third_person && !app.camera().fly_mode &&
                             !in_vehicle;
      if (auto* body = app.scene().find_by_name("PlayerBody")) {
        body->visible = show_body;
        if (show_body) {
          const float spd = std::sqrt(
              app.camera().velocity.x * app.camera().velocity.x +
              app.camera().velocity.z * app.camera().velocity.z);
          if (spd > 0.2f) {
            player_anim_phase += spd * dt * 3.2f;
          }
          const float body_h = app.camera().crouching
                                    ? kPlayerBodyHeight * 0.62f
                                    : kPlayerBodyHeight;
          body->transform.position = {
              app.camera().position.x, body_h * 0.5f, app.camera().position.z};
          body->transform.rotation_euler.y = app.camera().yaw;
          if (body->mesh) {
            fury::pose_humanoid(*body->mesh, body_h, kPlayerBodyColor,
                                player_anim_phase);
          }
        }
      }
    }
    heist.loot_speed_mul =
        crew.loot_speed_boost(app.camera().position, 5.5f) * perks.crew_mul() *
        perks.loot_mul() * skills.loot_speed_mul();

    // Harder escape when heat is high
    if (heist.phase() == fury::HeistPhase::Escape) {
      const float heat_t = heat.normalized();
      heist.escape_timeout = base_escape_timeout * (1.f - 0.45f * heat_t);
      if (heat_t >= 0.999f) {
        // Max heat during escape: fail the extract
        // Force fail by shrinking timeout below elapsed — handled via heat fail below
      }
    } else {
      heist.escape_timeout = base_escape_timeout;
    }

    // 2.5.0 door triggers — Enter tip + optional snap (before heist E so snap wins at doors)
    door_enter_tip = false;
    bool door_consumed_interact = false;
    if (!in_vehicle && !chat_open && !help_panel.open) {
      if (const fury::DoorTrigger* door = interiors.door_at(app.camera().position)) {
        const fury::InteriorZone* iz = interiors.zone_at(app.camera().position);
        const bool already_inside =
            iz && std::strcmp(iz->tag, door->zone_tag) == 0;
        if (!already_inside) {
          door_enter_tip = true;
          if (!door_tip_logged) {
            door_tip_logged = true;
            fury::Log::info(std::string("TIP: Enter ") + door->label +
                            " — press E to snap inside (or walk through doorway)");
          }
          if (input.interact_pressed && door->snap_on_interact) {
            app.camera().position = door->interior_spawn;
            app.camera().snap_look();
            fury::Log::info(std::string("Entered ") + door->label +
                            " interior (door snap)");
            door_enter_tip = false;
            door_consumed_interact = true;
          }
        }
      } else {
        door_tip_logged = false;
      }
    }

    // 3.5.0 breaker — E near box cuts that site's cameras (before heist E)
    breaker_tip = false;
    if (!in_vehicle && !chat_open && !help_panel.open && !lobby_open &&
        !smoke_mode) {
      if (security.near_live_breaker(app.camera().position)) {
        breaker_tip = true;
        if (!breaker_tip_logged) {
          breaker_tip_logged = true;
          fury::Log::info(
              "TIP: Breaker box — press E to cut security cameras for this site");
        }
        if (input.interact_pressed && !door_consumed_interact) {
          const int sid = security.try_trip_breaker(app.camera().position);
          if (sid >= 0) {
            door_consumed_interact = true;
            fury::Log::info(std::string("Breaker tripped — cameras offline at ") +
                            fury::SecurityNet::site_name(sid));
            audio->play_cue("impact");
            for (const auto& cam : security.cameras()) {
              if (cam.site_id == sid) {
                if (auto* lens = app.scene().find_by_name(cam.lens_name)) {
                  lens->material.emissive = 0.05f;
                  lens->material.albedo = {0.2f, 0.25f, 0.28f};
                }
              }
            }
            for (const auto& b : security.breakers()) {
              if (b.site_id == sid && b.tripped) {
                if (auto* be = app.scene().find_by_name(b.entity_name)) {
                  be->material.emissive = 0.08f;
                  be->material.albedo = {0.35f, 0.35f, 0.32f};
                }
              }
            }
          }
        }
      } else {
        breaker_tip_logged = false;
      }
    }

        const bool interact_for_heist =
        input.interact_pressed && !door_consumed_interact && !in_vehicle &&
        !chat_open && !help_panel.open && !lobby_open &&
        nearest_driveable(app.camera().position) < 0;
    // Join clients mirror host heist phase/loot — skip local sim to avoid desync payouts
    if (net_mode != fury::net::NetMode::Join || !net_client->connected()) {
      heist.update(app.camera().position, interact_for_heist, dt);
    }


    // Harbor loft safehouse — loft interior zone clears heat over time
    {
      const fury::InteriorZone* iz = interiors.zone_at(app.camera().position);
      in_safehouse = !in_vehicle && iz && std::strcmp(iz->tag, "loft") == 0;
      if (in_safehouse && !safehouse_tip_logged) {
        safehouse_tip_logged = true;
        fury::Log::info(
            "TIP: Harbor loft — heat cooling. G craft at workbench. Tab map / Enter FT ($250). "
            "[ / ] save slots (autosaves on extract/quit)");
      }
      if (!in_safehouse) {
        safehouse_tip_logged = false;
      }
    }

    // Police chase AI — spawn/pursue on high heat or alarm; contact raises heat
    {
      pursuit.spawn_interval = factions.pursuit_spawn_interval();
      const float heat_bump = pursuit.update(
          dt, app.camera().position, heat.normalized(), alarm_active, in_vehicle,
          in_safehouse);
      if (heat_bump > 0.f) {
        heat.value = (std::min)(1.f, heat.value + heat_bump);
        fury::Log::info("Patrol contact — heat up");
      }
      pursuit_count = pursuit.active_count();
      for (const auto& car : pursuit.cars()) {
        if (auto* body = app.scene().find_by_name(car.entity_name)) {
          body->transform.position = car.position;
          body->transform.rotation_euler.y = car.yaw;
          body->visible = car.active;
        }
        const std::string light_name =
            std::string("PatrolLight") +
            car.entity_name.substr(std::string("PatrolCar").size());
        if (auto* light = app.scene().find_by_name(light_name)) {
          light->transform.position = {
              car.position.x, car.position.y + 0.85f, car.position.z};
          light->transform.rotation_euler.y = car.yaw;
          light->visible = car.active;
          if (car.active) {
            const float flash =
                0.45f + 0.55f * std::sin(alarm_time * 16.f +
                                         static_cast<float>(car.spawn_slot));
            light->material.emissive = 1.0f + 3.5f * flash;
          }
        }
      }
    }

    // Civilian traffic — waypoint loops; stop/slow near player
    {
      traffic.update(dt, app.camera().position);
      for (const auto& car : traffic.cars()) {
        if (auto* body = app.scene().find_by_name(car.entity_name)) {
          body->transform.position = car.position;
          body->transform.rotation_euler.y = car.yaw;
          body->visible = car.active;
        }
        const std::string cab_name =
            std::string("TrafficCabin") +
            car.entity_name.substr(std::string("TrafficCar").size());
        if (auto* cab = app.scene().find_by_name(cab_name)) {
          cab->transform.position = {
              car.position.x, car.position.y + 0.7f, car.position.z};
          cab->transform.rotation_euler.y = car.yaw;
          cab->visible = car.active;
        }
      }
    }

    const bool hidden =
        in_vehicle || in_safehouse;  // van / loft count as cover for heat decay
    const bool crouching = app.camera().crouching;
    const float d_guard = dist_xz(app.camera().position, guard_pos);
    const bool near_guard = d_guard <= heat.guard_radius;

    // Visibility meter + camera heat (standing in cone)
    const float cam_heat = security.update(
        dt, app.camera().position, crouching, hidden, near_guard, d_guard,
        visibility);
    if (cam_heat > 0.f) {
      heat.value = (std::min)(
          1.f, heat.value + cam_heat * craft.camera_heat_mul());
    }

    const float base_rise = heat.rise_rate;
    const float finale_heat_mul = mission_board.is_finale() ? 1.65f : 1.f;
    float crouch_heat_mul = crouching ? 0.35f : 1.f;  // quieter heat while crouched
    heat.rise_rate =
        base_rise * perks.heat_rise_mul() * skills.heat_rise_mul() *
        finale_heat_mul * crouch_heat_mul;
    // Extra loft decay while inside (on top of player_hidden multiplier)
    if (in_safehouse) {
      heat.value = (std::max)(0.f, heat.value - heat.decay_rate * 1.25f * dt);
    }
    const bool heat_fail =
        heat.update(dt, heist.phase(), app.camera().position, guard_pos, hidden);
    heat.rise_rate = base_rise;
    if (heat_fail ||
        (heat.is_max() && heist.phase() == fury::HeistPhase::Escape)) {
      if (heist.phase() != fury::HeistPhase::Failed &&
          heist.phase() != fury::HeistPhase::Success &&
          heist.phase() != fury::HeistPhase::Idle) {
        fury::Log::info("Heat max — job burned");
        heist.force_fail();
      }
    }

    // Optional siren visual: flash emissive beacons when heat is high during loot
    // Peak heat this run (for daily contract checks)
    if (heist.phase() == fury::HeistPhase::Approach ||
        heist.phase() == fury::HeistPhase::Breach ||
        heist.phase() == fury::HeistPhase::Looting ||
        heist.phase() == fury::HeistPhase::Escape) {
      run_peak_heat = (std::max)(run_peak_heat, heat.normalized());
    } else if (heist.phase() == fury::HeistPhase::Idle) {
      run_peak_heat = 0.f;
    }

    alarm_active = heist.phase() == fury::HeistPhase::Looting &&
                   heat.normalized() >= 0.55f;
    if (alarm_active) {
      siren_cue_accum += dt;
      if (siren_cue_accum >= 1.15f) {
        siren_cue_accum = 0.f;
        audio->play_cue("siren");
      }
    } else {
      siren_cue_accum = 0.f;
    }
    for (auto& ent : app.scene().entities()) {
      if (ent.tag != "siren") {
        continue;
      }
      if (alarm_active) {
        const float flash =
            0.45f + 0.55f * std::sin(alarm_time * 14.f +
                                     ent.transform.position.x * 0.1f);
        ent.material.emissive = 1.2f + 3.8f * flash;
        ent.material.albedo = {1.0f, 0.12f + 0.25f * flash, 0.08f};
      } else {
        ent.material.emissive = 0.18f;
        ent.material.albedo = {0.95f, 0.18f, 0.12f};
      }
    }

    if (heist.phase() != last_phase) {
      fury::Log::info(std::string("Heist state -> ") + heist.phase_name());
      {
        const char* line = crew_banter.next_line(heist.phase());
        if (line && line[0]) {
          banter_line = line;
          banter_timer = 3.4f;
          fury::Log::info(std::string("[CREW] ") + line);
        }
      }
      if (heist.phase() == fury::HeistPhase::Approach) {
        run_peak_heat = 0.f;
      }
      if (heist.phase() == fury::HeistPhase::Approach ||
          heist.phase() == fury::HeistPhase::Breach) {
        if (onboard_step < 1) onboard_step = 1;
      }
      if (heist.phase() == fury::HeistPhase::Breach) {
        audio->play_cue("heist_start");
        audio->play_cue("heist_breach");
        audio->play_cue("impact");
      } else if (heist.phase() == fury::HeistPhase::Escape) {
        if (onboard_step < 2) onboard_step = 2;
      } else if (heist.phase() == fury::HeistPhase::Success) {
        audio->play_cue("heist_success");
        heat.reset();
        visibility.reset();
        particles.emit_burst(app.camera().position + Vec3{0.f, 1.2f, 0.f}, 48, 8.f);
        banner_timer = mission_board.is_finale() ? 4.0f : 2.2f;
        banner_success = true;
        ending_banner = mission_board.is_finale();
        onboard_step = 3;
        quest_journal.mark_complete(mission_board.selected);
        factions.on_heist_success();
        fury::Log::info(std::string("Reputation: ") + factions.status_line());
        {
          const int gained = fury::SkillTree::xp_for_tier(
              mission_board.current().payout_tier);
          skills.add_xp(gained);
          fury::Log::info(std::string("Skills: +") + std::to_string(gained) +
                          " XP (total " + std::to_string(skills.xp) + ") — " +
                          skills.status_line());
        }
        {
          const int daily_cash = daily.try_claim_on_success(
              mission_board.selected, run_peak_heat);
          if (daily_cash > 0) {
            heist.inventory().cash += daily_cash;
            heist.score().lifetime_cash += daily_cash;
            fury::Log::info(std::string("Daily contract complete: ") +
                            daily.today().title + " +$" +
                            std::to_string(daily_cash) +
                            " (peak heat " + std::to_string(run_peak_heat) + ")");
          } else if (!daily.claimed_today() &&
                     mission_board.selected == daily.today().mission_index) {
            fury::Log::info(std::string("Daily not met — need peak heat <= ") +
                            std::to_string(daily.today().max_heat) +
                            " (had " + std::to_string(run_peak_heat) + ")");
          }
        }
        {
          const int bonus = fury::roll_mission_loot(
              static_cast<std::size_t>(mission_board.selected), heist.inventory());
          fury::Log::info(std::string("Loot table rolled (+$") +
                          std::to_string(bonus) + " cash drops; chips Bond=" +
                          std::to_string(heist.inventory().chips[0]) +
                          " Sapphire=" +
                          std::to_string(heist.inventory().chips[1]) +
                          " Drive=" +
                          std::to_string(heist.inventory().chips[2]) + ")");
        }
        if (mission_board.is_finale()) {
          constexpr int kFinaleCashBonus = 25000;
          heist.inventory().cash += kFinaleCashBonus;
          heist.score().lifetime_cash += kFinaleCashBonus;
          particles.emit_burst(app.camera().position + Vec3{0.f, 2.0f, 0.f}, 72, 11.f);
          fury::Log::info(
              std::string("ENDING: Pierline holds the Harbor — finale cash bonus +$") +
              std::to_string(kFinaleCashBonus));
        }
        fury::Log::info(std::string("Journal: marked complete — ") +
                        mission_board.current().title);
      } else if (heist.phase() == fury::HeistPhase::Failed) {
        heat.value = (std::min)(1.f, heat.value + 0.25f);
        banner_timer = 2.2f;
        banner_success = false;
        ending_banner = false;
      }
      if (heist.phase() == fury::HeistPhase::Success ||
          heist.phase() == fury::HeistPhase::Failed) {
        autosave_slot();
      }
      last_phase = heist.phase();
    }

    fury::net::PlayerState local;
    local.id = net_client->local_player_id();
    local.display_name = "Operator";
    local.position = app.camera().position;
    local.yaw = app.camera().yaw;
    local.heat = heat.normalized();
    local.heist_phase = static_cast<std::uint8_t>(heist.phase());
    local.in_heist = heist.phase() == fury::HeistPhase::Breach ||
                     heist.phase() == fury::HeistPhase::Looting ||
                     heist.phase() == fury::HeistPhase::Escape;
    local.cash = static_cast<float>(heist.inventory().cash);
    local.ready = local_ready;
    local.mission_index =
        static_cast<std::uint8_t>(std::clamp(mission_board.selected, 0, 255));
    local.loot_progress = heist.loot_progress();
    net_client->send_player_state(local);
    net_client->poll();

    // Co-op heist sync — joiner mirrors host mission index + phase + loot progress
    if (net_mode == fury::net::NetMode::Join && net_client->connected()) {
      const fury::net::PlayerState* host_ps = nullptr;
      for (const auto& rp : net_client->remote_players()) {
        if (rp.id == 1 || host_ps == nullptr) {
          host_ps = &rp;
          if (rp.id == 1) break;
        }
      }
      if (host_ps != nullptr) {
        const int host_mission = static_cast<int>(host_ps->mission_index);
        if (host_mission >= 0 &&
            host_mission < static_cast<int>(fury::kMissionCount) &&
            (host_mission != mission_board.selected ||
             host_mission != mirrored_mission)) {
          mission_board.selected = host_mission;
          apply_target();
          mirrored_mission = host_mission;
          fury::Log::info(std::string("Joiner mirrored host mission: ") +
                          mission_board.current().title);
        }
        const auto host_phase =
            static_cast<fury::HeistPhase>(host_ps->heist_phase);
        if (host_ps->heist_phase != mirrored_phase ||
            host_ps->in_heist ||
            host_phase == fury::HeistPhase::Looting ||
            host_phase == fury::HeistPhase::Escape ||
            host_phase == fury::HeistPhase::Breach) {
          heist.apply_net_sync(host_phase, host_ps->loot_progress);
          mirrored_phase = host_ps->heist_phase;
        } else if (host_phase == fury::HeistPhase::Idle ||
                   host_phase == fury::HeistPhase::Success ||
                   host_phase == fury::HeistPhase::Failed) {
          if (heist.phase() != host_phase) {
            heist.apply_net_sync(host_phase, host_ps->loot_progress);
          }
          mirrored_phase = host_ps->heist_phase;
        }
        // Host started from lobby: clear joiner ready when host drops ready mid-lobby
        if (lobby_open && !host_ps->ready && local_ready &&
            host_phase == fury::HeistPhase::Idle) {
          // keep lobby until host advances; no force-close
        }
      }
    }

    particles.update(dt);
    particles.sync_scene(app.scene(), fx_quad);

    if (auto* remote_ent = app.scene().find_by_name("GhostLoop")) {
      if (!net_client->remote_players().empty()) {
        const auto& rp = net_client->remote_players().front();
        remote_ent->transform.position = {rp.position.x, 0.9f, rp.position.z};
        remote_ent->transform.rotation_euler.y = rp.yaw;
        remote_ent->visible = true;
        // Synced cash flash when Ghost wallet jumps
        if (ghost_last_cash >= 0.f && rp.cash > ghost_last_cash + 0.5f) {
          ghost_cash_flash = 0.85f;
        }
        ghost_last_cash = rp.cash;
        ghost_cash_flash = (std::max)(0.f, ghost_cash_flash - dt);
        const float ht = std::clamp(rp.heat, 0.f, 1.f);
        const float flash = ghost_cash_flash;
        remote_ent->material.albedo = {
            0.3f + 0.7f * ht + 0.6f * flash,
            0.7f - 0.4f * ht + 0.5f * flash,
            0.9f - 0.6f * ht * (1.f - flash)};
        remote_ent->material.emissive =
            (rp.in_heist ? 0.45f : 0.05f) + 1.8f * flash;
      }
    }

    if (perf_log) {
      perf_log_timer += dt;
      if (perf_log_timer >= 1.0f) {
        std::ostringstream poss;
        poss << "[perf] fps=" << std::fixed << std::setprecision(1) << app.timer().fps()
             << " cull=" << app.config().cull_distance
             << " npc_upd=" << perf_npc_updated << "/" << perf_npc_total
             << " phase=" << static_cast<int>(heist.phase());
        fury::Log::info(poss.str());
        perf_log_timer = 0.f;
      }
    }

    // 2.7.0 — keep last N seconds of player transform for F10 scrub
    replay.push(app.camera(), dt);

    status_timer += dt;
    if (status_timer >= 2.0f) {
      std::ostringstream oss;
      oss << heist.status_line();
      oss << " | heat=" << heat.normalized()
          << (in_vehicle ? " [van]" : "")
          << (in_safehouse ? " [loft]" : "")
          << (active_interior_tag[0] && !in_safehouse
                  ? (std::string(" [") + active_interior_tag + "]")
                  : std::string())
          << " vis=" << visibility.normalized()
          << (app.camera().crouching ? " [crouch]" : "")
          << " pursuit=" << pursuit_count
          << " | tod=" << day_night.time_of_day
          << " night=" << day_night.night_factor()
          << " wx=" << weather.mode_name() << "/" << rain
          << " npcs=" << npcs.agents().size()
          << " crew=" << crew.nearby_count(app.camera().position, 5.5f)
          << " lootx=" << heist.loot_speed_mul
          << " lights=" << framed.point_light_count
          << " slot=" << active_slot
          << " perks=c" << perks.crew << "/h" << perks.heat_damp << "/l"
          << perks.loot_speed
          << " up=" << (fence_up.better_payouts ? "P" : "-")
          << (fence_up.quieter_tools ? "Q" : "-")
          << " craft=j" << craft.signal_jammer << "/s" << craft.smoke_pellet
          << " chips=b" << heist.inventory().chips[0] << "/s"
          << heist.inventory().chips[1] << "/d" << heist.inventory().chips[2]
          << " " << factions.status_line()
          << " | " << mission_board.status_line();
      if (net_client->connected()) {
        oss << " | session=" << net_client->session().session_id
            << " remotes=" << net_client->remote_players().size()
            << " crew_roles=" << net_client->crew_roster().size();
      }
      fury::Log::info(oss.str());
      status_timer = 0.f;
    }
  };

  app.on_hud = [&]() {
    // Photo mode — hide gameplay HUD (tiny PHOTO pip only)
    if (photo_mode.active) {
      const float W = static_cast<float>(app.window().width());
      const float H = static_cast<float>(app.window().height());
      app.renderer().draw_hud_rect(W * 0.5f - 50.f, 18.f, 100.f, 18.f,
                                   Color{12, 18, 28, 160});
      app.renderer().draw_hud_rect(W * 0.5f - 36.f, 22.f, 72.f, 10.f,
                                   Color{255, 210, 90, 230});
      if (photo_tip_timer > 0.f) {
        const float fade = std::clamp(photo_tip_timer / 0.35f, 0.f, 1.f);
        const std::uint8_t a = static_cast<std::uint8_t>(210 * fade);
        app.renderer().draw_hud_rect(W * 0.5f - 120.f, H - 56.f, 240.f, 20.f,
                                     Color{12, 18, 28, a});
        app.renderer().draw_hud_rect(W * 0.5f - 100.f, H - 50.f, 200.f, 8.f,
                                     Color{255, 200, 80, a});
      }
      return;
    }

    const int crew_n = crew.nearby_count(app.camera().position, 5.5f);
    const bool near_shop =
        dist_xz(app.camera().position, kAshcourtShopPos) <= kShopRadius;
    // Breadcrumb objective: board cue near spawn, vault during job, escape on extract
    Vec3 objective = heist.vault_position;
    if (onboard_step == 0) {
      objective = {0.f, 0.f, 8.f};  // plaza / board cue near start
    } else if (heist.phase() == fury::HeistPhase::Escape) {
      objective = heist.escape_position;
    }
    const bool finale_locked =
        !quest_journal.finale_unlocked(unlock_all);
    draw_hud_bars(app.renderer(), heist, heat, in_vehicle, app.window().width(),
                  app.window().height(), mission_board, app.camera().position,
                  objective, crew_n, buy_menu.open, perks, active_slot, near_shop,
                  splash_remaining, banner_timer, banner_success, onboard_step,
                  app.camera().yaw, show_fps, app.timer().fps(), quest_journal,
                  banter_timer, banter_line, alarm_active, local_ready,
                  net_client->crew_roster(), net_client->remote_players(),
                  net_client->chat_log(), chat_open, chat_buffer, inv_panel.open,
                  buy_menu.sell_selected, pursuit_count, in_safehouse,
                  rep_panel.open, factions, ending_banner, intro_cutscene.active,
                  finale_locked, help_panel.open, door_enter_tip,
                  active_interior_tag, skill_panel.open, skills, daily,
                  run_peak_heat, lobby_open,
                  net_mode != fury::net::NetMode::Join,
                  nameplate_show, nameplate_role, nameplate_fill,
                  dialogue_timer, dialogue_line_count, dialogue_role,
                  radio_station, map_panel.open, map_panel.focus, fast_travel_cd,
                  in_safehouse && fast_travel_cd <= 0.f &&
                      heist.inventory().cash >= kFastTravelCost,
                  visibility.normalized(), app.camera().crouching, breaker_tip,
                  craft_panel.open,
                  dist_xz(app.camera().position, kLoftWorkbenchPos) <=
                      kWorkbenchRadius,
                  craft, fence_up);
    // 3.7.0 lightning screen flash
    if (lightning_flash > 0.01f) {
      const float W = static_cast<float>(app.window().width());
      const float H = static_cast<float>(app.window().height());
      const float f = std::clamp(lightning_flash, 0.f, 1.f);
      const std::uint8_t a = static_cast<std::uint8_t>(210.f * f);
      app.renderer().draw_hud_rect(0.f, 0.f, W, H, Color{210, 225, 255, a});
    }
    // Quality tip pip (F6) — geometric bars encode low/med/high
    if (quality_tip_timer > 0.f) {
      const float W = static_cast<float>(app.window().width());
      const float H = static_cast<float>(app.window().height());
      const float fade = std::clamp(quality_tip_timer / 0.4f, 0.f, 1.f);
      const std::uint8_t a = static_cast<std::uint8_t>(220 * fade);
      const int q = static_cast<int>(quality.level);
      app.renderer().draw_hud_rect(W * 0.5f - 90.f, H - 92.f, 180.f, 28.f,
                                   Color{12, 18, 28, a});
      for (int i = 0; i < 3; ++i) {
        const bool on = i <= q;
        app.renderer().draw_hud_rect(
            W * 0.5f - 70.f + static_cast<float>(i) * 50.f, H - 84.f, 40.f, 12.f,
            on ? Color{80, 220, 160, a} : Color{40, 55, 70, a});
      }
    }
    // Mute tip pip (F8) — single bar on = unmuted, dim = muted
    if (mute_tip_timer > 0.f) {
      const float W = static_cast<float>(app.window().width());
      const float H = static_cast<float>(app.window().height());
      const float fade = std::clamp(mute_tip_timer / 0.35f, 0.f, 1.f);
      const std::uint8_t a = static_cast<std::uint8_t>(220 * fade);
      app.renderer().draw_hud_rect(W * 0.5f - 70.f, H - 128.f, 140.f, 24.f,
                                   Color{12, 18, 28, a});
      const bool on = !audio->muted();
      app.renderer().draw_hud_rect(
          W * 0.5f - 50.f, H - 120.f, 100.f, 10.f,
          on ? Color{120, 200, 255, a} : Color{90, 50, 60, a});
    }

    // Replay scrub timeline (F10) — fill shows scrub_u along ring buffer
    if (replay.scrubbing) {
      const float W = static_cast<float>(app.window().width());
      const float H = static_cast<float>(app.window().height());
      app.renderer().draw_hud_rect(W * 0.5f - 180.f, H - 64.f, 360.f, 28.f,
                                   Color{10, 16, 24, 200});
      app.renderer().draw_hud_rect(W * 0.5f - 160.f, H - 54.f, 320.f, 10.f,
                                   Color{40, 55, 70, 220});
      app.renderer().draw_hud_rect(
          W * 0.5f - 160.f, H - 54.f, 320.f * std::clamp(replay.scrub_u, 0.f, 1.f),
          10.f, Color{80, 220, 255, 240});
      // Playhead
      const float hx = W * 0.5f - 160.f + 320.f * std::clamp(replay.scrub_u, 0.f, 1.f);
      app.renderer().draw_hud_rect(hx - 3.f, H - 58.f, 6.f, 18.f, Color{255, 220, 100, 250});
      if (replay_tip_timer > 0.f) {
        const float fade = std::clamp(replay_tip_timer / 0.35f, 0.f, 1.f);
        const std::uint8_t a = static_cast<std::uint8_t>(210 * fade);
        app.renderer().draw_hud_rect(W * 0.5f - 110.f, 24.f, 220.f, 18.f,
                                     Color{12, 18, 28, a});
        app.renderer().draw_hud_rect(W * 0.5f - 90.f, 28.f, 180.f, 10.f,
                                     Color{80, 220, 255, a});
      }
    }
  };

  const int code = app.run();

  autosave_slot();
  net_client->disconnect();  // joins/stops embedded UDP host thread
  audio->shutdown();
  return code;
}
