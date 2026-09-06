#include <fury/fury.hpp>

#include <SDL.h>

#include <algorithm>
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
  glow.emissive = 2.4f;
  add_prop(scene, pole, "LampPole", {x, 2.2f, z}, dark);
  add_prop(scene, lamp_head, "LampHead", {x, 4.5f, z}, glow);
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
      fury::make_box({3.2f, 2.8f, 2.2f}, Vec3{0.78f, 0.58f, 0.16f}));
  Material vault_mat;
  vault_mat.albedo = {1.1f, 0.95f, 0.55f};
  vault_mat.metallic = 0.85f;
  vault_mat.roughness = 0.28f;
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
  jt.albedo = {1.2f, 1.0f, 0.55f};
  jt.metallic = 0.9f;
  jt.roughness = 0.2f;
  jt.emissive = 0.35f;
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

void build_harbor_metro(fury::Scene& scene) {
  auto* asphalt = scene.add_mesh(
      fury::make_plane(200.f, 200.f, Vec3{0.22f, 0.22f, 0.24f}, 28.f));
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
  };

  int bi = 0;
  for (const BldgSpec& spec : buildings) {
    auto* mesh = scene.add_mesh(
        fury::make_colored_box(spec.size, spec.top, spec.side));
    Material bm;
    bm.texture = TextureSlot::Concrete;
    bm.roughness = 0.7f;
    bm.albedo = {1.f, 1.f, 1.f};
    const Vec3 pos{spec.pos.x, spec.size.y * 0.5f, spec.pos.z};
    const std::string bname = "Bldg" + std::to_string(bi++);
    add_solid_box(scene, mesh, bname.c_str(), pos, spec.size, bm);
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
  add_solid_box(scene, van_body, "GetawayVan", {34.f, 1.2f, 33.5f},
                {4.5f, 2.2f, 2.2f}, van_mat);

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
}

void draw_hud_bars(fury::Renderer& r, const fury::HeistController& heist,
                   int win_w) {
  (void)win_w;
  // Panel background
  r.draw_hud_rect(16.f, 16.f, 340.f, 86.f, Color{12, 16, 24, 170});
  // Cash bar (green fill proportional to capped cash)
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
  app.camera().far_plane = 280.f;

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

  // Active target: 0 Meridian Mutual, 1 Crown & Cutler (T switches)
  int target_index = 0;
  const Vec3 meridian_vault{0.f, 0.f, -15.2f};
  const Vec3 jewel_vault{-22.f, 0.f, 4.8f};

  auto net_client = fury::net::create_stub_client();
  net_client->connect("127.0.0.1", 7777);

  fury::SessionSnapshot session;
  {
    std::ostringstream sid;
    sid << "vl-" << net_client->session().session_id;
    session.session_id = sid.str();
  }
  session.world = "Harbor Metro";
  session.player_name = "Operator";
  if (fury::load_session_json(kSessionPath, session)) {
    heist.inventory().cash = session.cash;
    heist.score().successes = session.successes;
    heist.score().failures = session.failures;
    heist.score().lifetime_cash = session.lifetime_score;
    target_index = session.heist_target_index;
  }

  auto apply_target = [&]() {
    if (target_index == 0) {
      heist.vault_position = meridian_vault;
      heist.base_payout = 10000;
      heist.jewelry_bonus = 0;
      fury::Log::info("Heist target: Meridian Mutual vault");
    } else {
      heist.vault_position = jewel_vault;
      heist.base_payout = 6500;
      heist.jewelry_bonus = 3500;
      fury::Log::info("Heist target: Crown & Cutler display safe (stub)");
    }
    heist.reset();
  };
  apply_target();

  fury::Log::info("=== Vaultline — Harbor Metro / Meridian Mutual ===");
  fury::Log::info("Original bank-heist open-world MMO prototype (not a GTA clone).");
  fury::Log::info("WASD move, mouse look, Space/Ctrl up/down (fly), F walk/fly, Shift sprint");
  fury::Log::info("E near vault/safe to breach → loot → green pad / van to extract");
  fury::Log::info("T switches heist target (Meridian Mutual <-> Crown & Cutler)");
  fury::Log::info("Esc releases mouse, Esc again quits — session autosaves on success/fail");

  {
    auto* ghost_mesh = app.scene().add_mesh(
        fury::make_box(fury::Vec3{0.8f, 1.8f, 0.8f},
                       fury::Vec3{0.3f, 0.7f, 0.9f}));
    fury::Entity ghost;
    ghost.name = "GhostStub";
    ghost.mesh = ghost_mesh;
    ghost.transform.position = {8.f, 0.9f, 10.f};
    ghost.material.metallic = 0.2f;
    ghost.material.roughness = 0.5f;
    app.scene().add_entity(std::move(ghost));
  }

  fury::HeistPhase last_phase = heist.phase();
  float status_timer = 0.f;
  bool t_was_down = false;

  app.on_update = [&](float dt, const fury::InputState& input) {
    // T = toggle heist target (poll via SDL since InputState may lack it)
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    const bool t_down = keys[SDL_SCANCODE_T] != 0;
    if (t_down && !t_was_down &&
        (heist.phase() == fury::HeistPhase::Idle ||
         heist.phase() == fury::HeistPhase::Success ||
         heist.phase() == fury::HeistPhase::Failed)) {
      target_index = 1 - target_index;
      apply_target();
    }
    t_was_down = t_down;

    heist.update(app.camera().position, input.interact_pressed, dt);

    if (heist.phase() != last_phase) {
      fury::Log::info(std::string("Heist state -> ") + heist.phase_name());
      if (heist.phase() == fury::HeistPhase::Success ||
          heist.phase() == fury::HeistPhase::Failed) {
        session.cash = heist.inventory().cash;
        session.successes = heist.score().successes;
        session.failures = heist.score().failures;
        session.lifetime_score = heist.score().lifetime_cash;
        session.heist_target_index = target_index;
        fury::save_session_json(kSessionPath, session);
      }
      last_phase = heist.phase();
    }

    fury::net::PlayerState local;
    local.id = net_client->local_player_id();
    local.display_name = "Operator";
    local.position = app.camera().position;
    local.yaw = app.camera().yaw;
    local.in_heist = heist.phase() == fury::HeistPhase::Breach ||
                     heist.phase() == fury::HeistPhase::Looting ||
                     heist.phase() == fury::HeistPhase::Escape;
    net_client->send_player_state(local);
    net_client->poll();

    if (auto* remote_ent = app.scene().find_by_name("GhostStub")) {
      if (!net_client->remote_players().empty()) {
        const auto& rp = net_client->remote_players().front();
        remote_ent->transform.position = {rp.position.x, 0.9f, rp.position.z};
        remote_ent->visible = true;
      }
    }

    status_timer += dt;
    if (status_timer >= 2.0f) {
      std::ostringstream oss;
      oss << heist.status_line();
      if (net_client->connected()) {
        oss << " | session=" << net_client->session().session_id
            << " remotes=" << net_client->remote_players().size();
      }
      fury::Log::info(oss.str());
      status_timer = 0.f;
    }
  };

  app.on_hud = [&]() {
    draw_hud_bars(app.renderer(), heist, app.window().width());
  };

  const int code = app.run();

  session.cash = heist.inventory().cash;
  session.successes = heist.score().successes;
  session.failures = heist.score().failures;
  session.lifetime_score = heist.score().lifetime_cash;
  session.heist_target_index = target_index;
  fury::save_session_json(kSessionPath, session);

  return code;
}
