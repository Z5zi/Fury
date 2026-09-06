#include <fury/fury.hpp>

#include <SDL.h>

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <memory>
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

constexpr const char* kSessionPath = "vaultline_session.json";

void add_solid_box(fury::Scene& scene, fury::Mesh* mesh, const char* name,
                   const Vec3& pos, const Vec3& size, Material mat,
                   const std::string& tag = {}) {
  Entity e;
  e.name = name;
  e.mesh = mesh;
  e.transform.position = pos;
  e.material = mat;
  e.solid = true;
  e.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, size);
  e.tag = tag;
  scene.add_entity(std::move(e));
}

void add_prop(fury::Scene& scene, fury::Mesh* mesh, const char* name, const Vec3& pos,
              Material mat, bool solid = false, const Vec3& solid_size = {}) {
  Entity e;
  e.name = name;
  e.mesh = mesh;
  e.transform.position = pos;
  e.material = mat;
  if (solid) {
    e.solid = true;
    e.collider = Aabb::from_center_size({0.f, 0.f, 0.f}, solid_size);
  }
  scene.add_entity(std::move(e));
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
  glass_mat.metallic = 0.2f;
  glass_mat.roughness = 0.15f;
  glass_mat.albedo = {0.7f, 0.85f, 0.95f};
  glass_mat.emissive = 0.15f;
  add_prop(scene, desk_top, "TellerGlass1", {-5.2f, 1.35f, bank_cz + 1.7f},
           glass_mat);
  add_prop(scene, desk_top, "TellerGlass2", {5.2f, 1.35f, bank_cz + 1.7f},
           glass_mat);

  // ATMs
  auto* atm = scene.add_mesh(
      fury::make_box({1.1f, 1.8f, 0.55f}, Vec3{0.12f, 0.14f, 0.18f}));
  Material atm_mat;
  atm_mat.metallic = 0.65f;
  atm_mat.roughness = 0.35f;
  atm_mat.albedo = {0.85f, 0.88f, 0.95f};
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

  // Waiting chairs / rope queue stubs
  auto* chair = scene.add_mesh(
      fury::make_box({0.7f, 0.85f, 0.7f}, Vec3{0.35f, 0.22f, 0.18f}));
  Material chair_mat;
  chair_mat.roughness = 0.7f;
  for (float x = -2.5f; x <= 2.5f; x += 1.25f) {
    add_prop(scene, chair, "LobbyChair", {x, 0.42f, bank_cz + 5.0f}, chair_mat,
             true, {0.7f, 0.85f, 0.7f});
  }

  auto* plant = scene.add_mesh(
      fury::make_box({0.6f, 1.4f, 0.6f}, Vec3{0.18f, 0.45f, 0.22f}));
  Material plant_mat;
  plant_mat.roughness = 0.85f;
  plant_mat.albedo = {0.7f, 1.0f, 0.7f};
  add_prop(scene, plant, "LobbyPlant", {7.2f, 0.7f, bank_cz + 5.2f}, plant_mat);
  add_prop(scene, plant, "LobbyPlant2", {-7.5f, 0.7f, bank_cz - 0.5f}, plant_mat);
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
                {0.8f, h, d}, stone);
  add_solid_box(scene, wall_e, "JewelWallE", {cx + w * 0.5f, h * 0.5f, cz},
                {0.8f, h, d}, stone);
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
    w.material.roughness = 0.22f;
    w.material.metallic = 0.4f;
    w.material.albedo = {0.85f, 0.95f, 1.1f};
    w.material.uv_scroll_u = 0.03f;
    w.material.uv_scroll_v = 0.02f;
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
  add_prop(scene, road, "AshcourtRoad", {-62.f, 0.2f, 28.f}, road_mat);

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

  // ATM heist-lite target (standalone kiosk)
  auto* atm_booth = scene.add_mesh(
      fury::make_box({2.4f, 2.6f, 2.0f}, Vec3{0.18f, 0.20f, 0.24f}));
  Material booth;
  booth.metallic = 0.55f;
  booth.roughness = 0.4f;
  booth.albedo = {0.9f, 0.92f, 0.98f};
  add_solid_box(scene, atm_booth, "AshAtmBooth", {ox - 2.f, 1.3f, oz - 14.f},
                {2.4f, 2.6f, 2.0f}, booth);

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
    t.transform.position = {ox - 2.f, 1.4f, oz - 12.9f};
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
  add_prop(scene, atm_screen, "AshAtmScreen", {ox - 2.f, 1.65f, oz - 12.7f},
           screen);

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

void build_harbor_metro(fury::Scene& scene) {
  auto* asphalt = scene.add_mesh(
      fury::make_plane(320.f, 260.f, Vec3{0.22f, 0.22f, 0.24f}, 36.f));
  {
    Entity ground;
    ground.name = "StreetGrid";
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
  win_mat.albedo = {0.65f, 0.85f, 1.15f};
  win_mat.roughness = 0.35f;
  win_mat.metallic = 0.15f;
  win_mat.emissive = 0.2f;  // scaled by night via tag "window"

  int bi = 0;
  int wi = 0;
  for (const BldgSpec& spec : buildings) {
    auto* mesh = scene.add_mesh(
        fury::make_colored_box(spec.size, spec.top, spec.side));
    Material bm;
    bm.texture = TextureSlot::Concrete;
    bm.roughness = 0.55f + 0.25f * static_cast<float>((bi * 17) % 5) / 4.f;
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
    w.material.roughness = 0.22f;
    w.material.metallic = 0.4f;
    w.material.albedo = {0.9f, 1.0f, 1.1f};
    w.material.uv_scroll_u = 0.035f;
    w.material.uv_scroll_v = 0.018f;
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

  auto* van_body = scene.add_mesh(
      fury::make_box({4.5f, 2.2f, 2.2f}, Vec3{0.12f, 0.14f, 0.16f}));
  Material van_mat;
  van_mat.metallic = 0.6f;
  van_mat.roughness = 0.4f;
  van_mat.albedo = {0.95f, 0.95f, 1.0f};
  {
    Entity van;
    van.name = "GetawayVan";
    van.tag = "vehicle";
    van.mesh = van_body;
    van.transform.position = {34.f, 1.2f, 33.5f};
    van.material = van_mat;
    van.solid = false;  // enterable stub — collision handled while driving
    scene.add_entity(std::move(van));
  }
  // Cabin hint / windshield stub
  auto* van_cabin = scene.add_mesh(
      fury::make_box({1.6f, 1.0f, 2.0f}, Vec3{0.25f, 0.45f, 0.55f}));
  Material cabin_mat;
  cabin_mat.metallic = 0.2f;
  cabin_mat.roughness = 0.25f;
  cabin_mat.albedo = {0.55f, 0.75f, 0.9f};
  cabin_mat.emissive = 0.12f;
  add_prop(scene, van_cabin, "GetawayVanCabin", {32.4f, 1.5f, 33.5f}, cabin_mat);

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
}

void draw_hud_bars(fury::Renderer& r, const fury::HeistController& heist,
                   const fury::HeatMeter& heat, bool in_vehicle, int win_w,
                   int win_h, const fury::MissionBoard& board,
                   const Vec3& player_pos, const Vec3& objective_pos,
                   int crew_nearby) {
  // Panel background (taller for heat + crew stub)
  r.draw_hud_rect(16.f, 16.f, 340.f, 128.f, Color{12, 16, 24, 170});
  // Cash bar
  const float cash_t =
      std::min(1.f, static_cast<float>(heist.inventory().cash) / 50000.f);
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
  r.draw_hud_rect(28.f, 50.f, 316.f * std::max(loot_t, 0.02f), 14.f, loot_col);

  // Score stub bar
  const float score_t =
      std::min(1.f, static_cast<float>(heist.score().lifetime_cash) / 80000.f);
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
  r.draw_hud_rect(28.f, 94.f, 316.f * std::max(heat_t, 0.02f), 14.f, heat_col);

  // Crew nearby indicator (short bars)
  r.draw_hud_rect(28.f, 116.f, 316.f, 10.f, Color{40, 50, 60, 220});
  if (crew_nearby > 0) {
    r.draw_hud_rect(28.f, 116.f, 158.f * static_cast<float>(crew_nearby), 10.f,
                    Color{90, 180, 255, 230});
  }

  if (in_vehicle) {
    r.draw_hud_rect(16.f, 152.f, 180.f, 22.f, Color{20, 40, 30, 180});
    r.draw_hud_rect(28.f, 158.f, 156.f, 10.f, Color{60, 200, 120, 220});
  }

  // Mission board (M) — list of 3 jobs with payout tier bars
  if (board.open) {
    r.draw_hud_rect(16.f, 190.f, 360.f, 118.f, Color{10, 14, 22, 210});
    for (int i = 0; i < static_cast<int>(fury::kMissionCount); ++i) {
      const fury::MissionJob& job = fury::mission_job(static_cast<std::size_t>(i));
      const float y = 202.f + static_cast<float>(i) * 32.f;
      const bool sel = (board.selected == i);
      r.draw_hud_rect(28.f, y, 336.f, 26.f,
                      sel ? Color{40, 70, 110, 230} : Color{28, 34, 48, 210});
      // Tier bar (1..3)
      const float tier_t = static_cast<float>(job.payout_tier) / 3.f;
      Color tier_col{80, 200, 120, 230};
      if (job.payout_tier >= 3) {
        tier_col = Color{255, 200, 60, 230};
      } else if (job.payout_tier == 2) {
        tier_col = Color{180, 140, 255, 230};
      }
      r.draw_hud_rect(40.f, y + 8.f, 300.f * tier_t, 10.f, tier_col);
    }
  } else {
    // Compact selected-mission tier stub
    const fury::MissionJob& job = board.current();
    const float tier_t = static_cast<float>(job.payout_tier) / 3.f;
    r.draw_hud_rect(16.f, 190.f, 200.f, 18.f, Color{12, 16, 24, 150});
    r.draw_hud_rect(28.f, 194.f, 176.f * tier_t, 10.f, Color{255, 200, 80, 210});
  }

  // Minimap stub — top-right screen-space quad
  const float map_s = 150.f;
  const float map_x = static_cast<float>(win_w) - map_s - 16.f;
  const float map_y = 16.f;
  r.draw_hud_rect(map_x, map_y, map_s, map_s, Color{18, 24, 34, 190});
  r.draw_hud_rect(map_x + 2.f, map_y + 2.f, map_s - 4.f, map_s - 4.f,
                  Color{28, 40, 55, 160});
  // World extents roughly covering Harbor + districts
  constexpr float world_min_x = -120.f;
  constexpr float world_max_x = 130.f;
  constexpr float world_min_z = -60.f;
  constexpr float world_max_z = 80.f;
  auto world_to_map = [&](const Vec3& p, float& ox, float& oy) {
    const float u = (p.x - world_min_x) / (world_max_x - world_min_x);
    const float v = (p.z - world_min_z) / (world_max_z - world_min_z);
    ox = map_x + 6.f + std::clamp(u, 0.f, 1.f) * (map_s - 12.f);
    oy = map_y + 6.f + std::clamp(v, 0.f, 1.f) * (map_s - 12.f);
  };
  float px = 0.f, py = 0.f, ox = 0.f, oy = 0.f;
  world_to_map(player_pos, px, py);
  world_to_map(objective_pos, ox, oy);
  // Objective blip (gold)
  r.draw_hud_rect(ox - 4.f, oy - 4.f, 8.f, 8.f, Color{255, 200, 60, 240});
  // Player blip (cyan)
  r.draw_hud_rect(px - 3.f, py - 3.f, 6.f, 6.f, Color{80, 220, 255, 255});
  (void)win_h;
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  fury::AppConfig config;
  config.window.title = "Fury — Vaultline";
  config.window.width = 1280;
  config.window.height = 720;
  config.clear_color = {78, 118, 168, 255};
  config.log_fps = true;
  config.fps_log_interval = 1.0f;
  config.prefer_opengl = true;
  config.cull_distance = 120.f;
  config.capture_mouse = true;
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
  lit.fog_start = 40.f;
  lit.fog_end = 150.f;
  lit.fog_color = {78.f / 255.f, 118.f / 255.f, 168.f / 255.f};
  lit.ao_strength = 0.55f;
  app.renderer().set_lighting(lit);

  build_harbor_metro(app.scene());

  app.camera().position = {0.f, 1.7f, 12.f};
  app.camera().yaw = -1.5707963f;
  app.camera().pitch = -0.08f;
  app.camera().fly_mode = false;
  app.camera().move_speed = 9.f;
  app.camera().far_plane = 360.f;

  fury::DayNightCycle day_night;
  day_night.day_length = 160.f;
  day_night.time_of_day = 0.34f;
  const fury::Lighting base_lit = lit;

  auto audio = fury::create_audio();
  audio->init();

  fury::NpcSystem npcs;
  auto* civ_mesh = app.scene().add_mesh(
      fury::make_capsule(0.38f, 1.75f, fury::Vec3{0.55f, 0.72f, 0.85f}));
  auto* civ_mesh_b = app.scene().add_mesh(
      fury::make_capsule(0.38f, 1.7f, fury::Vec3{0.85f, 0.62f, 0.45f}));
  auto* civ_mesh_c = app.scene().add_mesh(
      fury::make_capsule(0.36f, 1.65f, fury::Vec3{0.65f, 0.80f, 0.55f}));
  auto* guard_mesh = app.scene().add_mesh(
      fury::make_capsule(0.42f, 1.85f, fury::Vec3{0.25f, 0.35f, 0.55f}));

  auto spawn_npc = [&](fury::NpcAgent agent, fury::Mesh* mesh,
                       const fury::Vec3& color) {
    fury::Entity e;
    e.name = agent.entity_name.empty() ? agent.name : agent.entity_name;
    agent.entity_name = e.name;
    e.mesh = mesh;
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
    a.entity_name = "NpcCivA";
    a.kind = fury::NpcKind::Civilian;
    a.position = {-12.f, 0.9f, 10.f};
    a.speed = 2.4f;
    a.waypoints = {{-12.f, 0.f, 10.f}, {12.f, 0.f, 10.f}, {12.f, 0.f, -18.f},
                   {-12.f, 0.f, -18.f}};
    spawn_npc(std::move(a), civ_mesh, {0.55f, 0.72f, 0.85f});
  }
  {
    fury::NpcAgent a;
    a.name = "CivB";
    a.entity_name = "NpcCivB";
    a.kind = fury::NpcKind::Civilian;
    a.position = {18.f, 0.9f, 22.f};
    a.speed = 2.1f;
    a.waypoints = {{18.f, 0.f, 22.f}, {34.f, 0.f, 22.f}, {34.f, 0.f, 8.f},
                   {18.f, 0.f, 8.f}};
    spawn_npc(std::move(a), civ_mesh_b, {0.85f, 0.62f, 0.45f});
  }
  {
    fury::NpcAgent a;
    a.name = "CivC";
    a.entity_name = "NpcCivC";
    a.kind = fury::NpcKind::Civilian;
    a.position = {88.f, 0.9f, 8.f};
    a.speed = 2.0f;
    a.waypoints = {{88.f, 0.f, 8.f}, {102.f, 0.f, 8.f}, {102.f, 0.f, 18.f},
                   {88.f, 0.f, 18.f}, {70.f, 0.f, 6.f}};
    spawn_npc(std::move(a), civ_mesh_c, {0.65f, 0.80f, 0.55f});
  }
  {
    fury::NpcAgent g;
    g.name = "BankGuard";
    g.entity_name = "NpcGuard";
    g.kind = fury::NpcKind::Guard;
    g.position = {4.f, 0.95f, -2.f};
    g.speed = 1.6f;
    g.chase_speed = 3.5f;
    g.waypoints = {{4.f, 0.f, -2.f}, {-4.f, 0.f, -2.f}, {-4.f, 0.f, 4.f},
                   {4.f, 0.f, 4.f}, {0.f, 0.f, -6.f}};
    spawn_npc(std::move(g), guard_mesh, {0.25f, 0.35f, 0.55f});
  }
  // Ashcourt civilian
  {
    fury::NpcAgent a;
    a.name = "CivAsh";
    a.entity_name = "NpcCivAsh";
    a.kind = fury::NpcKind::Civilian;
    a.position = {-88.f, 0.9f, 42.f};
    a.speed = 1.9f;
    a.waypoints = {{-88.f, 0.f, 42.f}, {-80.f, 0.f, 42.f}, {-80.f, 0.f, 50.f},
                   {-92.f, 0.f, 50.f}, {-70.f, 0.f, 28.f}};
    spawn_npc(std::move(a), civ_mesh, {0.72f, 0.58f, 0.40f});
  }

  fury::HeistController heist;
  heist.vault_position = {0.f, 0.f, -15.2f};
  heist.escape_position = {34.f, 0.f, 30.f};
  heist.approach_radius = 5.5f;
  heist.interact_radius = 3.8f;
  heist.breach_duration = 2.5f;
  heist.loot_duration = 7.f;
  heist.escape_radius = 5.f;
  heist.escape_timeout = 55.f;
  heist.base_payout = 10000;
  heist.jewelry_bonus = 0;

  // Active target via mission board: 0 Meridian, 1 Crown, 2 Ashcourt ATM
  fury::MissionBoard mission_board;
  const Vec3 meridian_vault{0.f, 0.f, -15.2f};
  const Vec3 jewel_vault{-22.f, 0.f, 4.8f};
  const Vec3 ashcourt_atm{-90.f, 0.f, 29.1f};  // AshcourtAtm face
  const Vec3 vault_positions[3] = {meridian_vault, jewel_vault, ashcourt_atm};

  fury::HeatMeter heat;
  const float base_escape_timeout = heist.escape_timeout;

  // Driveable getaway van near extraction pad
  Vec3 vehicle_pos{34.f, 1.2f, 33.5f};
  constexpr float kVehicleEnterRadius = 4.2f;
  bool in_vehicle = false;

  // AI crew stubs (follow during heist)
  fury::CrewSystem crew;
  auto* crew_mesh_a = app.scene().add_mesh(
      fury::make_capsule(0.36f, 1.7f, fury::Vec3{0.35f, 0.75f, 0.55f}));
  auto* crew_mesh_b = app.scene().add_mesh(
      fury::make_capsule(0.36f, 1.72f, fury::Vec3{0.75f, 0.45f, 0.35f}));
  {
    fury::CrewMember c;
    c.name = "Crew-Rook";
    c.entity_name = "CrewRook";
    c.follow_offset = {-1.8f, 0.f, -1.4f};
    c.position = {-2.f, 0.9f, 14.f};
    crew.add(std::move(c));
    fury::Entity e;
    e.name = "CrewRook";
    e.mesh = crew_mesh_a;
    e.transform.position = {-2.f, 0.9f, 14.f};
    e.material.albedo = {0.35f, 0.75f, 0.55f};
    e.material.roughness = 0.6f;
    app.scene().add_entity(std::move(e));
  }
  {
    fury::CrewMember c;
    c.name = "Crew-Sparrow";
    c.entity_name = "CrewSparrow";
    c.follow_offset = {1.8f, 0.f, -1.2f};
    c.position = {2.f, 0.9f, 14.f};
    crew.add(std::move(c));
    fury::Entity e;
    e.name = "CrewSparrow";
    e.mesh = crew_mesh_b;
    e.transform.position = {2.f, 0.9f, 14.f};
    e.material.albedo = {0.75f, 0.45f, 0.35f};
    e.material.roughness = 0.6f;
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

  auto net_client = fury::net::create_loopback_client();
  net_client->connect("127.0.0.1", 7777);
  // Session crew roles (net stub)
  net_client->assign_crew_role(10, "Crew-Rook", fury::net::CrewRole::Muscle);
  net_client->assign_crew_role(11, "Crew-Sparrow", fury::net::CrewRole::Lookout);

  fury::SessionSnapshot session;
  {
    std::ostringstream sid;
    sid << "vl-" << net_client->session().session_id;
    session.session_id = sid.str();
  }
  session.world = "Harbor Metro / Ridge Pier / Ashcourt";
  session.player_name = "Operator";
  if (fury::load_session_json(kSessionPath, session)) {
    heist.inventory().cash = session.cash;
    heist.score().successes = session.successes;
    heist.score().failures = session.failures;
    heist.score().lifetime_cash = session.lifetime_score;
    mission_board.selected = std::clamp(session.heist_target_index, 0, 2);
  }

  auto apply_target = [&]() {
    const int idx = mission_board.selected;
    const fury::MissionJob& job = mission_board.current();
    heist.vault_position = vault_positions[idx];
    heist.base_payout = job.base_payout;
    heist.jewelry_bonus = job.jewelry_bonus;
    heist.breach_duration = job.breach_duration;
    heist.loot_duration = job.loot_duration;
    fury::Log::info(std::string("Mission selected: ") + job.title +
                    " (tier " + std::to_string(job.payout_tier) + ", $" +
                    std::to_string(job.base_payout + job.jewelry_bonus) + ")");
    heist.reset();
    heat.reset();
  };
  apply_target();

  fury::Log::info("=== Vaultline 0.8.0 — UDP Loopback Net + Denser Art + Cull ===");
  fury::Log::info("Original bank-heist open-world MMO prototype (not a GTA clone).");
  fury::Log::info("WASD move, mouse look, Space/Ctrl up/down (fly), F walk/fly, Shift sprint");
  fury::Log::info("E near vault/safe/ATM to breach → loot → green pad to extract");
  fury::Log::info("F/E near getaway van to enter/exit; WASD drive (faster, no fly)");
  fury::Log::info("M opens mission board; 1/2/3 select job (or T cycles)");
  fury::Log::info("Crew stubs follow during heist and boost loot speed nearby");
  fury::Log::info("Net: localhost UDP loopback syncs transform/heat/phase to Ghost-Loop");
  fury::Log::info("Distance/frustum cull @ 120m; gold FX burst on heist success");
  fury::Log::info("Heat rises near guards during breach/loot; max heat fails the job");
  fury::Log::info("Day/night + NPCs + Ridge Pier + Ashcourt Market districts");
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
  bool digit_was_down[4] = {false, false, false, false};

  auto dist_xz = [](const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
  };

  auto sync_vehicle_entity = [&]() {
    if (auto* van = app.scene().find_by_tag("vehicle")) {
      van->transform.position = vehicle_pos;
      van->visible = !in_vehicle;
    }
    if (auto* cabin = app.scene().find_by_name("GetawayVanCabin")) {
      cabin->transform.position = {vehicle_pos.x - 1.6f, vehicle_pos.y + 0.3f,
                                   vehicle_pos.z};
      cabin->visible = !in_vehicle;
    }
  };

  auto try_toggle_vehicle = [&](bool pressed) -> bool {
    if (!pressed) {
      return false;
    }
    if (in_vehicle) {
      in_vehicle = false;
      app.camera().vehicle_seated = false;
      app.camera().fly_mode = false;
      // Exit beside the van
      app.camera().position = {vehicle_pos.x - 3.2f, 1.7f, vehicle_pos.z};
      sync_vehicle_entity();
      fury::Log::info("Exited getaway van");
      return true;
    }
    const float d = dist_xz(app.camera().position, vehicle_pos);
    if (d <= kVehicleEnterRadius) {
      in_vehicle = true;
      app.camera().vehicle_seated = true;
      app.camera().fly_mode = false;
      app.camera().position = {vehicle_pos.x, 1.55f, vehicle_pos.z};
      sync_vehicle_entity();
      fury::Log::info("Entered getaway van — WASD to drive, F/E to exit");
      return true;
    }
    return false;
  };

  app.on_pre_update = [&](float /*dt*/, const fury::InputState& input) {
    // Consume F when used for vehicle enter/exit (near van or already seated)
    const float d = dist_xz(app.camera().position, vehicle_pos);
    const bool near = in_vehicle || d <= kVehicleEnterRadius;
    if (input.key_f && near) {
      try_toggle_vehicle(true);
      return true;
    }
    return false;
  };

  app.on_update = [&](float dt, const fury::InputState& input) {
    day_night.update(dt);
    fury::Lighting framed = day_night.apply(base_lit);

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
      const int n = std::min(3, static_cast<int>(cands.size()));
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
    } else {
      const float dvan = dist_xz(app.camera().position, vehicle_pos);
      if (dvan <= kVehicleEnterRadius && input.interact_pressed) {
        try_toggle_vehicle(true);
      }
    }

    if (in_vehicle) {
      vehicle_pos = {app.camera().position.x, 1.2f, app.camera().position.z};
      sync_vehicle_entity();
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

    npcs.update(dt);
    for (const auto& agent : npcs.agents()) {
      if (auto* ent = app.scene().find_by_name(agent.entity_name)) {
        ent->transform.position = agent.position;
        ent->transform.rotation_euler.y = agent.yaw;
      }
    }
    for (const auto& agent : npcs.agents()) {
      if (agent.kind == fury::NpcKind::Guard) {
        guard_pos = agent.position;
      }
    }

    // M = mission board; 1/2/3 select; T = cycle target
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    const bool can_retarget =
        heist.phase() == fury::HeistPhase::Idle ||
        heist.phase() == fury::HeistPhase::Success ||
        heist.phase() == fury::HeistPhase::Failed;
    const bool m_down = keys[SDL_SCANCODE_M] != 0;
    if (m_down && !m_was_down) {
      mission_board.toggle();
      fury::Log::info(mission_board.open ? "Mission board OPEN (1/2/3 to select)"
                                         : "Mission board closed");
      fury::Log::info(mission_board.status_line());
    }
    m_was_down = m_down;

    const SDL_Scancode digit_scans[3] = {
        SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3};
    for (int i = 0; i < 3; ++i) {
      const bool down = keys[digit_scans[i]] != 0;
      if (down && !digit_was_down[i + 1] && can_retarget) {
        if (mission_board.select(i)) {
          apply_target();
        } else {
          fury::Log::info(mission_board.status_line());
        }
      }
      digit_was_down[i + 1] = down;
    }

    const bool t_down = keys[SDL_SCANCODE_T] != 0;
    if (t_down && !t_was_down && can_retarget) {
      mission_board.selected = (mission_board.selected + 1) % 3;
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
      }
    }
    heist.loot_speed_mul = crew.loot_speed_boost(app.camera().position, 5.5f);

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

    const bool interact_for_heist =
        input.interact_pressed && !in_vehicle &&
        dist_xz(app.camera().position, vehicle_pos) > kVehicleEnterRadius;
    heist.update(app.camera().position, interact_for_heist, dt);

    const bool hidden = in_vehicle;  // van counts as cover for heat decay
    const bool heat_fail =
        heat.update(dt, heist.phase(), app.camera().position, guard_pos, hidden);
    if (heat_fail ||
        (heat.is_max() && heist.phase() == fury::HeistPhase::Escape)) {
      if (heist.phase() != fury::HeistPhase::Failed &&
          heist.phase() != fury::HeistPhase::Success &&
          heist.phase() != fury::HeistPhase::Idle) {
        fury::Log::info("Heat max — job burned");
        heist.force_fail();
      }
    }

    if (heist.phase() != last_phase) {
      fury::Log::info(std::string("Heist state -> ") + heist.phase_name());
      if (heist.phase() == fury::HeistPhase::Breach) {
        audio->play_cue("heist_start");
      } else if (heist.phase() == fury::HeistPhase::Success) {
        audio->play_cue("heist_success");
        heat.reset();
        particles.emit_burst(app.camera().position + Vec3{0.f, 1.2f, 0.f}, 48, 8.f);
      } else if (heist.phase() == fury::HeistPhase::Failed) {
        heat.value = std::min(1.f, heat.value + 0.25f);
      }
      if (heist.phase() == fury::HeistPhase::Success ||
          heist.phase() == fury::HeistPhase::Failed) {
        session.cash = heist.inventory().cash;
        session.successes = heist.score().successes;
        session.failures = heist.score().failures;
        session.lifetime_score = heist.score().lifetime_cash;
        session.heist_target_index = mission_board.selected;
        fury::save_session_json(kSessionPath, session);
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
    net_client->send_player_state(local);
    net_client->poll();

    particles.update(dt);
    particles.sync_scene(app.scene(), fx_quad);

    if (auto* remote_ent = app.scene().find_by_name("GhostLoop")) {
      if (!net_client->remote_players().empty()) {
        const auto& rp = net_client->remote_players().front();
        remote_ent->transform.position = {rp.position.x, 0.9f, rp.position.z};
        remote_ent->transform.rotation_euler.y = rp.yaw;
        remote_ent->visible = true;
        // Tint remote pawn by synced heat / heist phase
        const float ht = std::clamp(rp.heat, 0.f, 1.f);
        remote_ent->material.albedo = {0.3f + 0.7f * ht, 0.7f - 0.4f * ht,
                                       0.9f - 0.6f * ht};
        remote_ent->material.emissive = rp.in_heist ? 0.45f : 0.05f;
      }
    }

    status_timer += dt;
    if (status_timer >= 2.0f) {
      std::ostringstream oss;
      oss << heist.status_line();
      oss << " | heat=" << heat.normalized()
          << (in_vehicle ? " [van]" : "")
          << " | tod=" << day_night.time_of_day
          << " night=" << day_night.night_factor()
          << " npcs=" << npcs.agents().size()
          << " crew=" << crew.nearby_count(app.camera().position, 5.5f)
          << " lootx=" << heist.loot_speed_mul
          << " lights=" << framed.point_light_count
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
    const int crew_n = crew.nearby_count(app.camera().position, 5.5f);
    draw_hud_bars(app.renderer(), heist, heat, in_vehicle, app.window().width(),
                  app.window().height(), mission_board, app.camera().position,
                  heist.vault_position, crew_n);
  };

  const int code = app.run();

  session.cash = heist.inventory().cash;
  session.successes = heist.score().successes;
  session.failures = heist.score().failures;
  session.lifetime_score = heist.score().lifetime_cash;
  session.heist_target_index = mission_board.selected;
  fury::save_session_json(kSessionPath, session);

  audio->shutdown();
  return code;
}
