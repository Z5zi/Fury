#include <fury/fury.hpp>

#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>

namespace {

void build_harbor_metro(fury::Scene& scene) {
  using fury::Entity;
  using fury::Transform;
  using fury::Vec3;

  // Asphalt streets / plaza
  auto* street = scene.add_mesh(
      fury::make_plane(80.f, 80.f, Vec3{0.18f, 0.18f, 0.20f}));
  Entity ground;
  ground.name = "StreetGrid";
  ground.mesh = street;
  ground.transform.position = {0.f, 0.f, 0.f};
  scene.add_entity(std::move(ground));

  // Sidewalk / plaza tint around bank
  auto* plaza = scene.add_mesh(
      fury::make_plane(28.f, 22.f, Vec3{0.35f, 0.34f, 0.32f}));
  Entity plaza_e;
  plaza_e.name = "BankPlaza";
  plaza_e.mesh = plaza;
  plaza_e.transform.position = {0.f, 0.02f, 0.f};
  scene.add_entity(std::move(plaza_e));

  // Meridian Mutual — main bank building (original)
  auto* bank_mesh = scene.add_mesh(fury::make_colored_box(
      Vec3{14.f, 10.f, 10.f}, Vec3{0.75f, 0.78f, 0.82f},
      Vec3{0.45f, 0.50f, 0.58f}));
  Entity bank;
  bank.name = "MeridianMutual";
  bank.tag = "bank";
  bank.mesh = bank_mesh;
  bank.transform.position = {0.f, 5.f, -6.f};
  scene.add_entity(std::move(bank));

  // Columns / entrance
  auto* col = scene.add_mesh(
      fury::make_box(Vec3{1.2f, 6.f, 1.2f}, Vec3{0.85f, 0.85f, 0.88f}));
  for (float x : {-5.f, -1.7f, 1.7f, 5.f}) {
    Entity c;
    c.name = "Column";
    c.mesh = col;
    c.transform.position = {x, 3.f, -0.5f};
    scene.add_entity(std::move(c));
  }

  // Vault chamber (interact target) — gold-ish box inside/behind entrance
  auto* vault_mesh = scene.add_mesh(
      fury::make_box(Vec3{3.f, 2.5f, 2.5f}, Vec3{0.72f, 0.55f, 0.18f}));
  Entity vault;
  vault.name = "VaultDoor";
  vault.tag = "vault";
  vault.mesh = vault_mesh;
  vault.transform.position = {0.f, 1.25f, 1.5f};
  scene.add_entity(std::move(vault));

  // Neighbor buildings lining the block
  auto* bldg_a = scene.add_mesh(
      fury::make_colored_box(Vec3{8.f, 7.f, 8.f}, Vec3{0.55f, 0.42f, 0.38f},
                             Vec3{0.40f, 0.32f, 0.30f}));
  Entity left;
  left.name = "CafeRow";
  left.mesh = bldg_a;
  left.transform.position = {-18.f, 3.5f, -4.f};
  scene.add_entity(std::move(left));

  auto* bldg_b = scene.add_mesh(
      fury::make_colored_box(Vec3{10.f, 12.f, 8.f}, Vec3{0.35f, 0.40f, 0.48f},
                             Vec3{0.28f, 0.32f, 0.38f}));
  Entity right;
  right.name = "HarborOffices";
  right.mesh = bldg_b;
  right.transform.position = {18.f, 6.f, -2.f};
  scene.add_entity(std::move(right));

  auto* bldg_c = scene.add_mesh(
      fury::make_box(Vec3{12.f, 5.f, 6.f}, Vec3{0.50f, 0.48f, 0.42f}));
  Entity across;
  across.name = "AcrossStreet";
  across.mesh = bldg_c;
  across.transform.position = {4.f, 2.5f, 18.f};
  scene.add_entity(std::move(across));

  // Escape van / extraction zone marker (flat green pad)
  auto* escape_mesh = scene.add_mesh(
      fury::make_box(Vec3{6.f, 0.3f, 4.f}, Vec3{0.20f, 0.65f, 0.30f}));
  Entity escape;
  escape.name = "ExtractionPad";
  escape.tag = "escape";
  escape.mesh = escape_mesh;
  escape.transform.position = {22.f, 0.15f, 16.f};
  scene.add_entity(std::move(escape));

  // Street lamps (simple poles)
  auto* pole = scene.add_mesh(
      fury::make_box(Vec3{0.25f, 4.f, 0.25f}, Vec3{0.15f, 0.15f, 0.15f}));
  auto* lamp = scene.add_mesh(
      fury::make_box(Vec3{0.8f, 0.3f, 0.8f}, Vec3{0.95f, 0.90f, 0.55f}));
  for (const Vec3& p : {Vec3{-10.f, 0.f, 8.f}, Vec3{10.f, 0.f, 8.f},
                        Vec3{-10.f, 0.f, -14.f}, Vec3{10.f, 0.f, -14.f}}) {
    Entity pe;
    pe.mesh = pole;
    pe.transform.position = {p.x, 2.f, p.z};
    scene.add_entity(std::move(pe));
    Entity le;
    le.mesh = lamp;
    le.transform.position = {p.x, 4.2f, p.z};
    scene.add_entity(std::move(le));
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
  config.clear_color = {18, 22, 32, 255};
  config.log_fps = true;
  config.fps_log_interval = 1.0f;
  config.prefer_opengl = true;
  config.capture_mouse = true;

  fury::Application app(std::move(config));
  if (!app.init()) {
    fury::Log::error("Failed to initialize Vaultline");
    return EXIT_FAILURE;
  }

  build_harbor_metro(app.scene());

  app.camera().position = {0.f, 1.7f, 14.f};
  app.camera().yaw = -1.5707963f;  // look toward -Z (bank)
  app.camera().pitch = -0.12f;
  app.camera().fly_mode = true;
  app.camera().move_speed = 10.f;

  fury::HeistController heist;
  heist.vault_position = {0.f, 0.f, 1.5f};
  heist.escape_position = {22.f, 0.f, 16.f};
  heist.loot_duration = 6.f;

  auto net_client = fury::net::create_stub_client();
  net_client->connect("127.0.0.1", 7777);

  fury::Log::info("=== Vaultline — Harbor Metro slice ===");
  fury::Log::info("Original bank-heist open-world MMO prototype (not GTA).");
  fury::Log::info("WASD move, mouse look, Space/Ctrl up/down (fly), F walk/fly");
  fury::Log::info("E near golden vault to start heist; reach green pad to escape");
  fury::Log::info("Esc releases mouse, Esc again quits");

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
    local.in_heist = heist.phase() == fury::HeistPhase::Looting ||
                     heist.phase() == fury::HeistPhase::Escaping;
    net_client->send_player_state(local);
    net_client->poll();

    // Draw stub remote pawn as a small box following net state
    if (auto* remote_ent = app.scene().find_by_name("GhostStub")) {
      if (!net_client->remote_players().empty()) {
        remote_ent->transform.position =
            net_client->remote_players().front().position;
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

  // Add remote pawn mesh after scene build
  {
    auto* ghost_mesh = app.scene().add_mesh(
        fury::make_box(fury::Vec3{0.8f, 1.8f, 0.8f},
                       fury::Vec3{0.3f, 0.7f, 0.9f}));
    fury::Entity ghost;
    ghost.name = "GhostStub";
    ghost.mesh = ghost_mesh;
    ghost.transform.position = {6.f, 0.9f, 4.f};
    app.scene().add_entity(std::move(ghost));
  }

  return app.run();
}
