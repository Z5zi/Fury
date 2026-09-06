#include <fury/fury.hpp>

#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

using fury::Aabb;
using fury::Entity;
using fury::Material;
using fury::TextureSlot;
using fury::Transform;
using fury::Vec3;

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
  dark.albedo = {0.9f, 0.9f, 0.9f};
  dark.metallic = 0.7f;
  dark.roughness = 0.35f;
  Material glow;
  glow.albedo = {1.2f, 1.1f, 0.7f};
  glow.roughness = 0.9f;
  add_prop(scene, pole, "LampPole", {x, 2.2f, z}, dark);
  add_prop(scene, lamp_head, "LampHead", {x, 4.5f, z}, glow);
}

void build_harbor_metro(fury::Scene& scene) {
  // --- Ground layers ---
  auto* asphalt = scene.add_mesh(
      fury::make_plane(160.f, 160.f, Vec3{0.22f, 0.22f, 0.24f}, 24.f));
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
      fury::make_plane(120.f, 14.f, Vec3{0.42f, 0.41f, 0.38f}, 10.f));
  Material concrete_mat;
  concrete_mat.texture = TextureSlot::Concrete;
  concrete_mat.roughness = 0.75f;

  // Sidewalk strips along main avenues
  for (float z : {-28.f, 0.f, 28.f}) {
    Entity sw;
    sw.name = "Sidewalk";
    sw.mesh = sidewalk;
    sw.transform.position = {0.f, 0.03f, z};
    sw.material = concrete_mat;
    scene.add_entity(std::move(sw));
  }
  auto* sidewalk_ns = scene.add_mesh(
      fury::make_plane(14.f, 120.f, Vec3{0.42f, 0.41f, 0.38f}, 10.f));
  for (float x : {-28.f, 0.f, 28.f}) {
    Entity sw;
    sw.name = "SidewalkNS";
    sw.mesh = sidewalk_ns;
    sw.transform.position = {x, 0.04f, 0.f};
    sw.material = concrete_mat;
    scene.add_entity(std::move(sw));
  }

  // Bank plaza
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

  // --- Meridian Mutual: hollow exterior + enterable interior ---
  // Building footprint centered near origin; entrance opens toward +Z (south).
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

  // Outer walls (north, west, east) + south with door gap (two segments)
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

  // Roof slab
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

  // Interior floor
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

  // Columns at entrance
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

  // Interior partition + vault room (north end)
  auto* partition = scene.add_mesh(
      fury::make_colored_box({12.f, 5.5f, 0.8f}, Vec3{0.40f, 0.42f, 0.48f},
                             Vec3{0.32f, 0.34f, 0.40f}));
  Material part_mat = bank_dark;
  // Leave a doorway in the middle: two partition wings
  auto* part_l = scene.add_mesh(
      fury::make_box({4.5f, 5.5f, 0.8f}, Vec3{0.38f, 0.40f, 0.46f}));
  auto* part_r = scene.add_mesh(
      fury::make_box({4.5f, 5.5f, 0.8f}, Vec3{0.38f, 0.40f, 0.46f}));
  (void)partition;
  add_solid_box(scene, part_l, "VaultPartitionL",
                {bank_cx - 4.0f, 2.75f, bank_cz - 2.5f}, {4.5f, 5.5f, 0.8f},
                part_mat);
  add_solid_box(scene, part_r, "VaultPartitionR",
                {bank_cx + 4.0f, 2.75f, bank_cz - 2.5f}, {4.5f, 5.5f, 0.8f},
                part_mat);

  // Vault door / chamber target
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

  // Lobby counters
  auto* desk = scene.add_mesh(
      fury::make_box({5.f, 1.2f, 1.4f}, Vec3{0.25f, 0.28f, 0.32f}));
  Material desk_mat;
  desk_mat.metallic = 0.4f;
  desk_mat.roughness = 0.45f;
  add_solid_box(scene, desk, "TellerDesk", { -4.5f, 0.6f, bank_cz + 2.f},
                {5.f, 1.2f, 1.4f}, desk_mat);
  add_solid_box(scene, desk, "TellerDesk2", {4.5f, 0.6f, bank_cz + 2.f},
                {5.f, 1.2f, 1.4f}, desk_mat);

  // --- City block buildings (street grid) ---
  struct BldgSpec {
    Vec3 pos;
    Vec3 size;
    Vec3 top;
    Vec3 side;
  };
  const BldgSpec buildings[] = {
      {{-22.f, 0.f, -8.f}, {10.f, 9.f, 10.f}, {0.62f, 0.45f, 0.40f}, {0.45f, 0.34f, 0.30f}},
      {{-38.f, 0.f, -6.f}, {8.f, 7.f, 9.f}, {0.50f, 0.48f, 0.42f}, {0.38f, 0.36f, 0.32f}},
      {{22.f, 0.f, -6.f}, {12.f, 14.f, 10.f}, {0.38f, 0.44f, 0.55f}, {0.28f, 0.32f, 0.40f}},
      {{40.f, 0.f, -10.f}, {10.f, 11.f, 12.f}, {0.42f, 0.40f, 0.48f}, {0.30f, 0.28f, 0.35f}},
      {{-20.f, 0.f, 18.f}, {11.f, 8.f, 8.f}, {0.55f, 0.50f, 0.38f}, {0.40f, 0.36f, 0.28f}},
      {{18.f, 0.f, 20.f}, {9.f, 6.f, 9.f}, {0.48f, 0.52f, 0.50f}, {0.35f, 0.38f, 0.36f}},
      {{-40.f, 0.f, 16.f}, {10.f, 10.f, 8.f}, {0.45f, 0.40f, 0.42f}, {0.32f, 0.28f, 0.30f}},
      {{38.f, 0.f, 18.f}, {11.f, 13.f, 9.f}, {0.35f, 0.42f, 0.50f}, {0.25f, 0.30f, 0.36f}},
      {{-22.f, 0.f, -32.f}, {9.f, 7.f, 8.f}, {0.58f, 0.48f, 0.42f}, {0.42f, 0.34f, 0.30f}},
      {{20.f, 0.f, -34.f}, {10.f, 9.f, 9.f}, {0.40f, 0.45f, 0.52f}, {0.30f, 0.34f, 0.40f}},
      {{0.f, 0.f, 36.f}, {14.f, 5.f, 8.f}, {0.52f, 0.50f, 0.45f}, {0.38f, 0.36f, 0.32f}},
      {{-36.f, 0.f, -30.f}, {8.f, 12.f, 8.f}, {0.32f, 0.36f, 0.42f}, {0.24f, 0.26f, 0.32f}},
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

  // --- Waterfront / pier strip (south-east) ---
  auto* water = scene.add_mesh(
      fury::make_plane(70.f, 28.f, Vec3{0.15f, 0.35f, 0.55f}, 8.f));
  {
    Entity w;
    w.name = "HarborWater";
    w.mesh = water;
    w.transform.position = {20.f, -0.35f, 52.f};
    w.material.texture = TextureSlot::Water;
    w.material.roughness = 0.25f;
    w.material.metallic = 0.35f;
    w.material.albedo = {0.9f, 1.0f, 1.1f};
    scene.add_entity(std::move(w));
  }

  auto* pier = scene.add_mesh(
      fury::make_box({40.f, 0.5f, 8.f}, Vec3{0.40f, 0.32f, 0.22f}));
  Material wood;
  wood.roughness = 0.8f;
  wood.albedo = {1.f, 0.95f, 0.85f};
  add_prop(scene, pier, "PierDeck", {15.f, 0.25f, 42.f}, wood);

  auto* pier_post = scene.add_mesh(
      fury::make_box({0.6f, 3.f, 0.6f}, Vec3{0.30f, 0.24f, 0.16f}));
  for (float x = -2.f; x <= 32.f; x += 6.f) {
    add_prop(scene, pier_post, "PierPost", {x, -0.5f, 45.5f}, wood);
  }

  // Crates on pier
  auto* crate = scene.add_mesh(
      fury::make_box({1.6f, 1.6f, 1.6f}, Vec3{0.55f, 0.40f, 0.22f}));
  Material crate_mat;
  crate_mat.roughness = 0.75f;
  crate_mat.texture = TextureSlot::Checker;
  add_solid_box(scene, crate, "CrateA", {10.f, 1.0f, 41.f}, {1.6f, 1.6f, 1.6f},
                crate_mat);
  add_solid_box(scene, crate, "CrateB", {12.f, 1.0f, 42.5f}, {1.6f, 1.6f, 1.6f},
                crate_mat);

  // --- Escape vehicle pad / alley ---
  auto* escape_mesh = scene.add_mesh(
      fury::make_box({7.f, 0.25f, 5.f}, Vec3{0.18f, 0.70f, 0.28f}));
  Material escape_mat;
  escape_mat.albedo = {0.7f, 1.2f, 0.7f};
  escape_mat.roughness = 0.9f;
  {
    Entity escape;
    escape.name = "ExtractionPad";
    escape.tag = "escape";
    escape.mesh = escape_mesh;
    escape.transform.position = {34.f, 0.15f, 30.f};
    escape.material = escape_mat;
    scene.add_entity(std::move(escape));
  }

  // Getaway van (simple box prop)
  auto* van_body = scene.add_mesh(
      fury::make_box({4.5f, 2.2f, 2.2f}, Vec3{0.12f, 0.14f, 0.16f}));
  Material van_mat;
  van_mat.metallic = 0.6f;
  van_mat.roughness = 0.4f;
  add_solid_box(scene, van_body, "GetawayVan", {34.f, 1.2f, 33.5f},
                {4.5f, 2.2f, 2.2f}, van_mat);

  // Alley dumpsters / props
  auto* dumpster = scene.add_mesh(
      fury::make_box({2.2f, 1.4f, 1.4f}, Vec3{0.20f, 0.45f, 0.22f}));
  Material dump_mat;
  dump_mat.metallic = 0.55f;
  dump_mat.roughness = 0.5f;
  add_solid_box(scene, dumpster, "Dumpster", {28.f, 0.7f, 26.f},
                {2.2f, 1.4f, 1.4f}, dump_mat);
  add_solid_box(scene, dumpster, "Dumpster2", {26.f, 0.7f, 28.f},
                {2.2f, 1.4f, 1.4f}, dump_mat);

  // --- Street lamps along avenues ---
  auto* pole = scene.add_mesh(
      fury::make_box({0.22f, 4.4f, 0.22f}, Vec3{0.12f, 0.12f, 0.12f}));
  auto* lamp = scene.add_mesh(
      fury::make_box({0.75f, 0.28f, 0.75f}, Vec3{0.95f, 0.90f, 0.55f}));
  const Vec3 lamp_pts[] = {
      {-14.f, 0.f, 8.f},  {14.f, 0.f, 8.f},   {-14.f, 0.f, -20.f},
      {14.f, 0.f, -20.f}, {-14.f, 0.f, 28.f}, {14.f, 0.f, 28.f},
      {-42.f, 0.f, 0.f},  {42.f, 0.f, 0.f},   {30.f, 0.f, 40.f},
      {8.f, 0.f, 40.f},   {-30.f, 0.f, -16.f},{30.f, 0.f, -16.f},
  };
  for (const Vec3& p : lamp_pts) {
    place_lamp(scene, pole, lamp, p.x, p.z);
  }
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  fury::AppConfig config;
  config.window.title = "Fury — Vaultline";
  config.window.width = 1280;
  config.window.height = 720;
  config.clear_color = {78, 118, 168, 255};  // outdoor sky
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

  // Fog matches sky clear color
  fury::Lighting lit = app.renderer().lighting();
  lit.sun_direction = {-0.35f, -0.88f, -0.28f};
  lit.sun_color = {1.f, 0.96f, 0.88f};
  lit.sun_intensity = 1.2f;
  lit.ambient = {0.18f, 0.22f, 0.30f};
  lit.fog_start = 40.f;
  lit.fog_end = 140.f;
  lit.fog_color = {78.f / 255.f, 118.f / 255.f, 168.f / 255.f};
  app.renderer().set_lighting(lit);

  build_harbor_metro(app.scene());

  // Spawn on plaza looking into Meridian Mutual
  app.camera().position = {0.f, 1.7f, 12.f};
  app.camera().yaw = -1.5707963f;
  app.camera().pitch = -0.08f;
  app.camera().fly_mode = false;  // walk + collision by default
  app.camera().move_speed = 9.f;
  app.camera().far_plane = 250.f;

  fury::HeistController heist;
  heist.vault_position = {0.f, 0.f, -15.2f};  // vault room
  heist.escape_position = {34.f, 0.f, 30.f};
  heist.approach_radius = 5.5f;
  heist.interact_radius = 3.8f;
  heist.breach_duration = 2.5f;
  heist.loot_duration = 7.f;
  heist.escape_radius = 5.f;
  heist.escape_timeout = 55.f;

  auto net_client = fury::net::create_stub_client();
  net_client->connect("127.0.0.1", 7777);

  fury::Log::info("=== Vaultline — Harbor Metro / Meridian Mutual ===");
  fury::Log::info("Original bank-heist open-world MMO prototype (not a GTA clone).");
  fury::Log::info("WASD move, mouse look, Space/Ctrl up/down (fly), F walk/fly, Shift sprint");
  fury::Log::info("E near vault to breach → loot → green pad / van to extract");
  fury::Log::info("Esc releases mouse, Esc again quits");

  // Remote pawn mesh
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

  app.on_update = [&](float dt, const fury::InputState& input) {
    heist.update(app.camera().position, input.interact_pressed, dt);

    if (heist.phase() != last_phase) {
      fury::Log::info(std::string("Heist state -> ") + heist.phase_name());
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

  return app.run();
}
