#include <fury/fury.hpp>

#include <SDL.h>

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdio>
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

constexpr int kSaveSlotCount = 3;
constexpr float kShopRadius = 6.5f;
const Vec3 kAshcourtShopPos{-86.f, 0.f, 48.f};
constexpr float kSafehouseEnterRadius = 5.8f;
/// Harbor loft safehouse (waterfront north of extraction).
const Vec3 kHarborLoftPos{42.f, 0.f, 52.f};

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
  steel.albedo = {0.42f, 0.46f, 0.52f};
  steel.metallic = 0.72f;
  steel.roughness = 0.38f;
  steel.texture = TextureSlot::Concrete;

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
                {hall_w, hall_h, 1.0f}, steel);
  // Front split walls leave a doorway
  add_solid_box(scene, wall_s, "DepotWallSL",
                {ox - hall_w * 0.32f, hall_h * 0.5f, oz + hall_d * 0.5f},
                {hall_w * 0.35f, hall_h, 1.0f}, steel);
  add_solid_box(scene, wall_s, "DepotWallSR",
                {ox + hall_w * 0.32f, hall_h * 0.5f, oz + hall_d * 0.5f},
                {hall_w * 0.35f, hall_h, 1.0f}, steel);
  add_solid_box(scene, wall_e, "DepotWallE", {ox + hall_w * 0.5f, hall_h * 0.5f, oz},
                {1.0f, hall_h, hall_d}, steel);
  add_solid_box(scene, wall_w, "DepotWallW", {ox - hall_w * 0.5f, hall_h * 0.5f, oz},
                {1.0f, hall_h, hall_d}, steel);

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
}

void build_harbor_loft(fury::Scene& scene) {
  // Enterable Harbor loft safehouse — clears heat while inside (no IP refs).
  const float cx = kHarborLoftPos.x;
  const float cz = kHarborLoftPos.z;
  const float w = 11.f;
  const float d = 9.f;
  const float h = 5.5f;

  Material brick;
  brick.albedo = {0.62f, 0.48f, 0.40f};
  brick.roughness = 0.7f;
  brick.texture = TextureSlot::Concrete;

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
                {w, h, 0.7f}, brick, "safehouse");
  add_solid_box(scene, wall_w, "LoftWallW", {cx - w * 0.5f, h * 0.5f, cz},
                {0.7f, h, d}, brick, "safehouse");
  add_solid_box(scene, wall_e, "LoftWallE", {cx + w * 0.5f, h * 0.5f, cz},
                {0.7f, h, d}, brick, "safehouse");
  add_solid_box(scene, wall_s_l, "LoftWallSL",
                {cx - 3.2f, h * 0.5f, cz + d * 0.5f}, {3.6f, h, 0.7f}, brick,
                "safehouse");
  add_solid_box(scene, wall_s_r, "LoftWallSR",
                {cx + 3.2f, h * 0.5f, cz + d * 0.5f}, {3.6f, h, 0.7f}, brick,
                "safehouse");

  auto* roof = scene.add_mesh(
      fury::make_box({w + 0.3f, 0.4f, d + 0.3f}, Vec3{0.35f, 0.32f, 0.30f}));
  add_prop(scene, roof, "LoftRoof", {cx, h + 0.12f, cz}, brick);

  auto* floor = scene.add_mesh(
      fury::make_plane(w - 1.0f, d - 1.0f, Vec3{0.42f, 0.34f, 0.28f}, 2.5f));
  {
    Entity f;
    f.name = "LoftFloor";
    f.tag = "safehouse";
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

  // Exterior sign plate
  auto* sign = scene.add_mesh(
      fury::make_box({2.8f, 0.55f, 0.18f}, Vec3{0.2f, 0.55f, 0.7f}));
  Material sign_mat;
  sign_mat.albedo = {0.35f, 0.85f, 1.1f};
  sign_mat.emissive = 0.9f;
  sign_mat.roughness = 0.85f;
  add_prop(scene, sign, "LoftSign", {cx, 3.8f, door_z + 0.5f}, sign_mat);
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
                   const fury::FactionReputations& reps) {
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

  // Panel background (taller for heat + crew stub)
  r.draw_hud_rect(16.f, 16.f, 340.f, 128.f, Color{12, 16, 24, 170});
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

  // Mission board (M) — list of jobs with payout tier bars
  if (board.open) {
    r.draw_hud_rect(16.f, 190.f, 360.f, 150.f, Color{10, 14, 22, 210});
    for (int i = 0; i < static_cast<int>(fury::kMissionCount); ++i) {
      const fury::MissionJob& job = fury::mission_job(static_cast<std::size_t>(i));
      const float y = 202.f + static_cast<float>(i) * 32.f;
      const bool sel = (board.selected == i);
      r.draw_hud_rect(28.f, y, 336.f, 26.f,
                      sel ? Color{40, 70, 110, 230} : Color{28, 34, 48, 210});
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
    const fury::MissionJob& job = board.current();
    const float tier_t = static_cast<float>(job.payout_tier) / 3.f;
    r.draw_hud_rect(16.f, 190.f, 200.f, 18.f, Color{12, 16, 24, 150});
    r.draw_hud_rect(28.f, 194.f, 176.f * tier_t, 10.f, Color{255, 200, 80, 210});
  }


  // Quest journal (J) — mission list + completion flags
  if (journal.open) {
    r.draw_hud_rect(W - 390.f, 180.f, 370.f, 168.f, Color{10, 14, 22, 220});
    for (int i = 0; i < static_cast<int>(fury::kMissionCount); ++i) {
      const fury::MissionJob& job = fury::mission_job(static_cast<std::size_t>(i));
      const float y = 192.f + static_cast<float>(i) * 36.f;
      const bool done = journal.complete[i] != 0;
      r.draw_hud_rect(W - 378.f, y, 346.f, 30.f,
                      done ? Color{28, 55, 40, 230} : Color{28, 34, 48, 220});
      // completion pip
      r.draw_hud_rect(W - 368.f, y + 8.f, 14.f, 14.f,
                      done ? Color{90, 255, 140, 240} : Color{60, 70, 90, 220});
      const float tier_t = static_cast<float>(job.payout_tier) / 3.f;
      r.draw_hud_rect(W - 340.f, y + 10.f, 300.f * tier_t, 10.f,
                      done ? Color{90, 220, 140, 230} : Color{255, 200, 80, 210});
    }
  }
  // Buy/sell menu (B) — Ashcourt fence perks + sell selected loot chip
  if (buy_open) {
    r.draw_hud_rect(16.f, 220.f, 360.f, 248.f, Color{8, 18, 14, 220});
    const float levels[3] = {
        static_cast<float>(perks.crew),
        static_cast<float>(perks.heat_damp),
        static_cast<float>(perks.loot_speed)};
    const Color cols[3] = {Color{90, 200, 140, 230}, Color{255, 160, 60, 230},
                           Color{120, 180, 255, 230}};
    for (int i = 0; i < 3; ++i) {
      const float y = 232.f + static_cast<float>(i) * 34.f;
      r.draw_hud_rect(28.f, y, 336.f, 28.f,
                      near_shop ? Color{30, 55, 40, 230} : Color{40, 35, 30, 210});
      const float t = (std::min)(1.f, levels[i] / 3.f);
      r.draw_hud_rect(40.f, y + 9.f, 300.f * (std::max)(t, 0.04f), 10.f, cols[i]);
    }
    // Sell rows — highlight selected chip; S sells one when near shop
    const Color chip_cols[3] = {Color{220, 200, 90, 230}, Color{80, 160, 255, 230},
                                Color{180, 120, 255, 230}};
    for (int i = 0; i < 3; ++i) {
      const float y = 340.f + static_cast<float>(i) * 34.f;
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

  // Inventory panel (I) — cash + named chips
  if (inv_open) {
    r.draw_hud_rect(W - 390.f, 360.f, 370.f, 150.f, Color{10, 16, 22, 220});
    // Cash bar
    const float cash_fill =
        (std::min)(1.f, static_cast<float>(heist.inventory().cash) / 50000.f);
    r.draw_hud_rect(W - 378.f, 372.f, 346.f, 26.f, Color{28, 40, 32, 230});
    r.draw_hud_rect(W - 366.f, 380.f, 322.f * (std::max)(cash_fill, 0.04f), 10.f,
                    Color{50, 200, 90, 230});
    const Color chip_cols[3] = {Color{220, 200, 90, 230}, Color{80, 160, 255, 230},
                                Color{180, 120, 255, 230}};
    for (int i = 0; i < 3; ++i) {
      const float y = 408.f + static_cast<float>(i) * 30.f;
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
    r.draw_hud_rect(W - 390.f, 200.f, 370.f, 148.f, Color{14, 12, 22, 220});
    const int vals[3] = {reps.pierline, reps.metro_watch, reps.syndicate};
    const Color cols[3] = {Color{90, 200, 140, 230}, Color{80, 140, 255, 230},
                           Color{220, 120, 80, 230}};
    for (int i = 0; i < 3; ++i) {
      const float y = 214.f + static_cast<float>(i) * 40.f;
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
  if (onboard_step >= 0 && onboard_step < 3 && splash_t <= 0.f) {
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

  // Success / fail banner
  if (banner_t > 0.f && splash_t <= 0.f) {
    const float fade = std::clamp(banner_t / 0.4f, 0.f, 1.f);
    const std::uint8_t a = static_cast<std::uint8_t>(210 * fade);
    Color bg = banner_success ? Color{12, 48, 28, a} : Color{48, 14, 18, a};
    Color bar = banner_success ? Color{90, 255, 140, a} : Color{255, 70, 70, a};
    r.draw_hud_rect(W * 0.5f - 260.f, H * 0.28f, 520.f, 90.f, bg);
    r.draw_hud_rect(W * 0.5f - 240.f, H * 0.28f + 20.f, 480.f, 16.f, bar);
    r.draw_hud_rect(W * 0.5f - 200.f, H * 0.28f + 48.f, 400.f, 12.f, bar);
    r.draw_hud_rect(W * 0.5f - 160.f, H * 0.28f + 68.f, 320.f, 8.f,
                    Color{255, 255, 255, static_cast<std::uint8_t>(160 * fade)});
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

  // Safehouse tip — save prompt while cooling heat inside Harbor loft
  if (in_safehouse && splash_t <= 0.f) {
    r.draw_hud_rect(W * 0.5f - 200.f, H - 178.f, 400.f, 26.f, Color{18, 40, 36, 210});
    r.draw_hud_rect(W * 0.5f - 188.f, H - 170.f, 376.f, 10.f, Color{80, 220, 180, 230});
  }

  // Ready-check pips — local + crew + remotes
  {
    const float rx = 16.f;
    float ry = 148.f;
    if (in_vehicle) ry = 180.f;
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

  // Chat log — last 4 messages as HUD bars + input buffer
  {
    const float cx0 = 16.f;
    const float cy0 = H - 160.f;
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

  // Minimap stub — top-right
  const float map_s = 150.f;
  const float map_x = W - map_s - 16.f;
  const float map_y = 16.f;
  r.draw_hud_rect(map_x, map_y, map_s, map_s, Color{18, 24, 34, 190});
  r.draw_hud_rect(map_x + 2.f, map_y + 2.f, map_s - 4.f, map_s - 4.f,
                  Color{28, 40, 55, 160});
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
  float sx = 0.f, sy = 0.f;
  world_to_map(kHarborLoftPos, sx, sy);
  r.draw_hud_rect(sx - 3.f, sy - 3.f, 6.f, 6.f, Color{90, 220, 180, 230});
  r.draw_hud_rect(ox - 4.f, oy - 4.f, 8.f, 8.f, Color{255, 200, 60, 240});
  r.draw_hud_rect(px - 3.f, py - 3.f, 6.f, 6.f, Color{80, 220, 255, 255});
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
  // Smoke / default stays on embedded loopback host+client.
  if (smoke_mode) {
    net_mode = fury::net::NetMode::Embedded;
  }

  fury::AppConfig config;
  config.window.title = "Fury — Vaultline 1.7.0";
  config.window.width = 1280;
  config.window.height = 720;
  config.clear_color = {78, 118, 168, 255};
  config.log_fps = false;  // optional; toggle with P
  config.fps_log_interval = 1.0f;
  config.prefer_opengl = !force_soft;
  config.cull_distance = 120.f;
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
  lit.fog_start = 40.f;
  lit.fog_end = 150.f;
  lit.fog_color = {78.f / 255.f, 118.f / 255.f, 168.f / 255.f};
  lit.ao_strength = 0.55f;
  lit.enable_shadows = true;
  lit.shadow_strength = 0.45f;
  app.renderer().set_lighting(lit);

  build_harbor_metro(app.scene());

  app.camera().position = {0.f, 1.7f, 12.f};
  app.camera().yaw = -1.5707963f;
  app.camera().pitch = -0.08f;
  app.camera().fly_mode = false;
  app.camera().move_speed = 9.f;
  app.camera().far_plane = 360.f;
  app.camera().snap_look();

  fury::DayNightCycle day_night;
  day_night.day_length = 160.f;
  day_night.time_of_day = 0.34f;
  const fury::Lighting base_lit = lit;
  fury::WeatherStub weather;

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

  // Active target via mission board: 0 Meridian, 1 Crown, 2 Ashcourt ATM, 3 Harbor Depot
  fury::MissionBoard mission_board;
  fury::QuestJournal quest_journal;
  const Vec3 meridian_vault{0.f, 0.f, -15.2f};
  const Vec3 jewel_vault{-22.f, 0.f, 4.8f};
  const Vec3 ashcourt_atm{-90.f, 0.f, 28.55f};  // AshcourtAtm alcove face
  const Vec3 harbor_depot{58.f, 0.f, -51.2f};   // HarborDepotCage face
  const Vec3 vault_positions[4] = {meridian_vault, jewel_vault, ashcourt_atm,
                                   harbor_depot};

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
  int active_slot = 0;
  {
    std::ostringstream sid;
    sid << "vl-" << net_client->session().session_id;
    session.session_id = sid.str();
  }
  session.world = "Harbor Metro / Ridge Pier / Ashcourt / Depot";
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
    }
    session.save_slot = active_slot;
    apply_session_to_play();
    heist.reset();
    heat.reset();
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
    probe.item_bearer_bond = 3;
    probe.item_sapphire = 2;
    probe.item_ledger_drive = 1;
    probe.rep_pierline = 42;
    probe.rep_metro_watch = -35;
    probe.rep_syndicate = 12;
    const std::string rt_path = "vaultline_roundtrip_tmp.json";
    if (fury::save_session_json(rt_path, probe)) {
      fury::SessionSnapshot back{};
      if (fury::load_session_json(rt_path, back) && back.cash == 4242 &&
          back.successes == 7 && back.perk_crew == 2 && back.save_slot == 1 &&
          back.mission_complete[0] == 1 && back.mission_complete[2] == 1 &&
          back.mission_complete[3] == 1 && back.item_bearer_bond == 3 &&
          back.item_sapphire == 2 && back.item_ledger_drive == 1 &&
          back.rep_pierline == 42 && back.rep_metro_watch == -35 &&
          back.rep_syndicate == 12) {
        fury::Log::info("Session save/load roundtrip OK");
      } else {
        fury::Log::warn("Session save/load roundtrip MISMATCH");
      }
      std::remove(rt_path.c_str());
    }
  }

  auto apply_target = [&]() {
    const int idx = mission_board.selected;
    const fury::MissionJob& job = mission_board.current();
    heist.vault_position =
        vault_positions[static_cast<std::size_t>(idx) %
                       (sizeof(vault_positions) / sizeof(vault_positions[0]))];
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

  fury::Log::info("=== Vaultline 1.7.0 — Factions / reputation (Pierline, Metro Watch, Syndicate) ===");
  fury::Log::info("Original bank-heist open-world MMO prototype — no Rockstar/GTA IP.");
  fury::Log::info("WASD move (accel/decel), mouse look (smoothed), Space/Ctrl up/down (fly), F walk/fly, Shift sprint");
  fury::Log::info("E near vault/safe/ATM/depot cage to breach → loot → green pad to extract");
  fury::Log::info("F/E near getaway van to enter/exit; WASD drive (faster, no fly)");
  fury::Log::info("M opens mission board; 1/2/3/4 select job (or T cycles)");
  fury::Log::info("J opens quest journal (missions + completion flags in save)");
  fury::Log::info("B opens Ashcourt fence buy/sell (near shop): 1-3 buy perks; Left/Right select chip; S sell one");
  fury::Log::info("I toggles inventory panel (cash + BearerBond / Sapphire / LedgerDrive)");
  fury::Log::info("U toggles faction reputation panel (Pierline / Metro Watch / Syndicate)");
  fury::Log::info("Heist success raises Pierline, lowers Metro Watch; fence sell raises Syndicate tension");
  fury::Log::info("Low Metro Watch → faster pursuits; high Pierline → Ashcourt shop discount");
  fury::Log::info("Successful extract rolls per-mission loot table (cash + named chips)");
  fury::Log::info("[ ] cycle save slots (vaultline_session_slotN.json); autosaves active slot + items");
  fury::Log::info("P toggles FPS overlay/log; R cycles weather (clear / rain / auto-drizzle)");
  fury::Log::info("Title splash then onboarding breadcrumbs; footstep/impact audio cues (silent OK)");
  fury::Log::info("TIP: Press M to open the mission board, then head to the gold objective");
  fury::Log::info("Crew stubs follow during heist and boost loot speed nearby");
  fury::Log::info("Net: UDP syncs pose/heat/phase/cash/ready; Enter/Y chat; K ready toggle");
  fury::Log::info("Net modes: default embedded | FURY_NET=host listen | FURY_NET=join + FURY_NET_HOST");
  fury::Log::info("Meridian Mutual heist tuned for ~2–5 min including travel");
  fury::Log::info("Day/night + NPCs + Ridge Pier + Ashcourt + Harbor Armored Depot");
  fury::Log::info("Crew banter on phase changes; siren flashes when heat high while looting");
  fury::Log::info("High heat/alarm spawns patrol cars — lose by distance, van, or Harbor loft");
  fury::Log::info("Harbor loft safehouse (waterfront) clears heat; save tip while inside ([/])");
  fury::Log::info("Weather stub: denser fog + rain streaks + wet asphalt tint when raining");
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
  bool s_was_down = false;
  bool left_was = false;
  bool right_was = false;
  bool bracket_l_was = false;
  bool bracket_r_was = false;
  bool digit_was_down[5] = {false, false, false, false, false};
  float ghost_cash_flash = 0.f;
  float ghost_last_cash = -1.f;

  // Presentation + onboarding + pursuit / factions / safehouse / alarm / weather / chat / ready / loot (1.7.0)
  float splash_remaining = 1.5f;
  float banner_timer = 0.f;
  bool banner_success = false;
  bool show_fps = false;
  bool p_was_down = false;
  // 0 = open board, 1 = go to target, 2 = escape, 3 = done
  int onboard_step = (session.successes > 0) ? 3 : 0;
  int onboard_tip_logged = -1;
  float smoke_elapsed = 0.f;
  fury::CrewBanter crew_banter;
  float banter_timer = 0.f;
  const char* banter_line = "";
  float alarm_time = 0.f;
  bool alarm_active = false;
  bool in_safehouse = false;
  bool safehouse_tip_logged = false;
  int pursuit_count = 0;
  bool r_was_down = false;
  float footstep_accum = 0.f;
  Vec3 foot_last_pos = app.camera().position;
  float rain_emit_accum = 0.f;
  bool chat_open = false;
  std::string chat_buffer;
  bool local_ready = false;

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
      app.camera().velocity = {};
      // Exit beside the van
      app.camera().position = {vehicle_pos.x - 3.2f, 1.7f, vehicle_pos.z};
      app.camera().snap_look();
      sync_vehicle_entity();
      fury::Log::info("Exited getaway van");
      return true;
    }
    const float d = dist_xz(app.camera().position, vehicle_pos);
    if (d <= kVehicleEnterRadius) {
      in_vehicle = true;
      app.camera().vehicle_seated = true;
      app.camera().fly_mode = false;
      app.camera().velocity = {};
      app.camera().position = {vehicle_pos.x, 1.55f, vehicle_pos.z};
      app.camera().snap_look();
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
    if (splash_remaining > 0.f) {
      splash_remaining = (std::max)(0.f, splash_remaining - dt);
    }
    if (banner_timer > 0.f) {
      banner_timer = (std::max)(0.f, banner_timer - dt);
    }
    if (banter_timer > 0.f) {
      banter_timer = (std::max)(0.f, banter_timer - dt);
    }
    alarm_time += dt;
    if (smoke_mode) {
      smoke_elapsed += dt;
      if (smoke_elapsed >= 2.6f) {
        app.request_quit();
      }
    }

    // Chat stub — Enter / Y open buffer; Esc cancels; Enter sends Chat UDP
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
    } else if (input.key_enter || input.key_y) {
      chat_open = true;
      chat_buffer.clear();
      app.input().set_text_entry(true);
      buy_menu.open = false;
      mission_board.open = false;
      quest_journal.open = false;
      inv_panel.open = false;
      rep_panel.open = false;
      fury::Log::info("Chat open — type message, Enter to send, Esc to cancel");
    }

    // K — toggle local ready (synced via PlayerState flags + crew pips)
    if (!chat_open && input.key_k) {
      local_ready = !local_ready;
      net_client->set_crew_ready(10, local_ready);
      net_client->set_crew_ready(11, local_ready);
      fury::Log::info(local_ready ? "Ready ON (K)" : "Ready OFF (K)");
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
    }

    // Onboarding tip log lines (once per step)
    if (onboard_step != onboard_tip_logged && splash_remaining <= 0.f) {
      onboard_tip_logged = onboard_step;
      if (onboard_step == 0) {
        fury::Log::info("TIP: Press M — open the mission board and pick a job (1/2/3/4)");
      } else if (onboard_step == 1) {
        fury::Log::info("TIP: Follow the gold compass/minimap blip to the target — press E to breach");
      } else if (onboard_step == 2) {
        fury::Log::info("TIP: Reach the green extraction pad (or drive the getaway van with F/E)");
      } else if (onboard_step == 3 && session.successes > 0) {
        fury::Log::info("TIP: Slice complete — press E to reset, B near Ashcourt fence to spend cash");
      }
    }

    day_night.update(dt);
    fury::Lighting framed = day_night.apply(base_lit);

    // R — cycle weather stub (clear / rain / auto-drizzle)
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
    framed = weather.apply(framed, rain);

    // Wetter asphalt tint
    for (const auto& dry : asphalt_dry) {
      if (auto* ent = app.scene().find_by_name(dry.name)) {
        weather.tint_asphalt(ent->material.albedo, ent->material.roughness,
                             ent->material.metallic, dry.albedo, dry.roughness,
                             dry.metallic, rain);
      }
    }

    // Rain particle streaks near camera
    if (rain > 0.05f) {
      rain_emit_accum += dt * (18.f + 55.f * rain);
      const int n = static_cast<int>(rain_emit_accum);
      if (n > 0) {
        rain_emit_accum -= static_cast<float>(n);
        particles.emit_rain_streaks(app.camera().position, n, 16.f + 6.f * rain);
      }
    } else {
      rain_emit_accum = 0.f;
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
        const float stride = input.key_shift ? 1.05f : 1.35f;
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

    // M = mission board; B = buy menu; 1/2/3 select/buy; T = cycle; [ ] slots
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    const bool can_retarget =
        heist.phase() == fury::HeistPhase::Idle ||
        heist.phase() == fury::HeistPhase::Success ||
        heist.phase() == fury::HeistPhase::Failed;
    const bool near_shop =
        dist_xz(app.camera().position, kAshcourtShopPos) <= kShopRadius;

    const bool m_down = keys[SDL_SCANCODE_M] != 0;
    if (!chat_open && m_down && !m_was_down) {
      mission_board.toggle();
      if (mission_board.open) {
        buy_menu.open = false;
        quest_journal.open = false;
        inv_panel.open = false;
        rep_panel.open = false;
      }
      fury::Log::info(mission_board.open ? "Mission board OPEN (1/2/3/4 to select)"
                                         : "Mission board closed");
      fury::Log::info(mission_board.status_line());
      if (mission_board.open && onboard_step == 0) {
        onboard_step = 1;
      }
    }
    m_was_down = m_down;

    const bool b_down = keys[SDL_SCANCODE_B] != 0;
    if (!chat_open && b_down && !b_was_down) {
      buy_menu.open = !buy_menu.open;
      if (buy_menu.open) {
        mission_board.open = false;
        quest_journal.open = false;
        inv_panel.open = false;
        rep_panel.open = false;
      }
      fury::Log::info(buy_menu.open
                          ? (near_shop
                                 ? "Fence OPEN — 1-3 buy perks; Left/Right select chip; S sell one"
                                 : "Fence OPEN — approach Ashcourt shop to buy/sell")
                          : "Fence menu closed");
    }
    b_was_down = b_down;

    const bool i_down = keys[SDL_SCANCODE_I] != 0;
    if (!chat_open && i_down && !i_was_down) {
      inv_panel.open = !inv_panel.open;
      if (inv_panel.open) {
        mission_board.open = false;
        buy_menu.open = false;
        quest_journal.open = false;
        rep_panel.open = false;
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
    if (!chat_open && u_down && !u_was_down) {
      rep_panel.open = !rep_panel.open;
      if (rep_panel.open) {
        mission_board.open = false;
        buy_menu.open = false;
        quest_journal.open = false;
        inv_panel.open = false;
        fury::Log::info(std::string("Reputation OPEN (U) — ") +
                        factions.status_line());
      } else {
        fury::Log::info("Reputation closed");
      }
    }
    u_was_down = u_down;

    const bool j_down = keys[SDL_SCANCODE_J] != 0;
    if (!chat_open && j_down && !j_was_down) {
      quest_journal.toggle();
      if (quest_journal.open) {
        mission_board.open = false;
        buy_menu.open = false;
        inv_panel.open = false;
        rep_panel.open = false;
      }
      fury::Log::info(quest_journal.open ? "Quest journal OPEN (J)"
                                         : "Quest journal closed");
      fury::Log::info(quest_journal.status_line());
    }
    j_was_down = j_down;


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

    const SDL_Scancode digit_scans[4] = {
        SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4};
    const int perk_costs[3] = {3500, 4500, 4000};  // affordable after one Meridian
    for (int i = 0; i < 4; ++i) {
      const bool down = keys[digit_scans[i]] != 0;
      if (!chat_open && down && !digit_was_down[i + 1]) {
        if (buy_menu.open) {
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
      mission_board.selected =
          (mission_board.selected + 1) % static_cast<int>(fury::kMissionCount);
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
    heist.loot_speed_mul =
        crew.loot_speed_boost(app.camera().position, 5.5f) * perks.crew_mul() *
        perks.loot_mul();

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

    // Harbor loft safehouse — interior AABB clears heat over time
    {
      const Vec3& p = app.camera().position;
      const float dx = p.x - kHarborLoftPos.x;
      const float dz = p.z - kHarborLoftPos.z;
      // Interior shell ~11x9 with doorway on +Z
      in_safehouse = !in_vehicle && std::fabs(dx) < 4.6f && std::fabs(dz) < 3.8f &&
                     p.y < 4.5f;
      if (in_safehouse && !safehouse_tip_logged) {
        safehouse_tip_logged = true;
        fury::Log::info(
            "TIP: Harbor loft — heat cooling. Press [ / ] to switch save slots "
            "(autosaves on extract/quit)");
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

    const bool hidden =
        in_vehicle || in_safehouse;  // van / loft count as cover for heat decay
    const float base_rise = heat.rise_rate;
    heat.rise_rate = base_rise * perks.heat_rise_mul();
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
    alarm_active = heist.phase() == fury::HeistPhase::Looting &&
                   heat.normalized() >= 0.55f;
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
        particles.emit_burst(app.camera().position + Vec3{0.f, 1.2f, 0.f}, 48, 8.f);
        banner_timer = 2.2f;
        banner_success = true;
        onboard_step = 3;
        quest_journal.mark_complete(mission_board.selected);
        factions.on_heist_success();
        fury::Log::info(std::string("Reputation: ") + factions.status_line());
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
        fury::Log::info(std::string("Journal: marked complete — ") +
                        mission_board.current().title);
      } else if (heist.phase() == fury::HeistPhase::Failed) {
        heat.value = (std::min)(1.f, heat.value + 0.25f);
        banner_timer = 2.2f;
        banner_success = false;
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

    status_timer += dt;
    if (status_timer >= 2.0f) {
      std::ostringstream oss;
      oss << heist.status_line();
      oss << " | heat=" << heat.normalized()
          << (in_vehicle ? " [van]" : "")
          << (in_safehouse ? " [loft]" : "")
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
    draw_hud_bars(app.renderer(), heist, heat, in_vehicle, app.window().width(),
                  app.window().height(), mission_board, app.camera().position,
                  objective, crew_n, buy_menu.open, perks, active_slot, near_shop,
                  splash_remaining, banner_timer, banner_success, onboard_step,
                  app.camera().yaw, show_fps, app.timer().fps(), quest_journal,
                  banter_timer, banter_line, alarm_active, local_ready,
                  net_client->crew_roster(), net_client->remote_players(),
                  net_client->chat_log(), chat_open, chat_buffer, inv_panel.open,
                  buy_menu.sell_selected, pursuit_count, in_safehouse,
                  rep_panel.open, factions);
  };

  const int code = app.run();

  autosave_slot();
  net_client->disconnect();  // joins/stops embedded UDP host thread
  audio->shutdown();
  return code;
}
