#include <fury/fury.hpp>

#include <cstdlib>

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  fury::AppConfig config;
  config.window.title = "Fury";
  config.window.width = 1280;
  config.window.height = 720;
  config.clear_color = {72, 110, 160, 255};
  config.log_fps = true;
  config.fps_log_interval = 1.0f;
  config.prefer_opengl = true;
  config.capture_mouse = true;
  config.enable_collision = false;

  fury::Application app(std::move(config));
  if (!app.init()) {
    fury::Log::error("Failed to initialize Fury demo");
    return EXIT_FAILURE;
  }

  auto* cube = app.scene().add_mesh(
      fury::make_colored_box({2.f, 2.f, 2.f}, {0.9f, 0.4f, 0.2f},
                             {0.7f, 0.3f, 0.15f}));
  fury::Entity e;
  e.name = "DemoCube";
  e.mesh = cube;
  e.transform.position = {0.f, 1.f, 0.f};
  e.material.metallic = 0.15f;
  e.material.roughness = 0.45f;
  app.scene().add_entity(std::move(e));

  auto* floor = app.scene().add_mesh(
      fury::make_plane(20.f, 20.f, {0.35f, 0.36f, 0.38f}, 4.f));
  fury::Entity f;
  f.name = "Floor";
  f.mesh = floor;
  f.material.texture = fury::TextureSlot::Checker;
  f.material.roughness = 0.8f;
  app.scene().add_entity(std::move(f));

  app.camera().position = {0.f, 2.f, 8.f};
  fury::Log::info("Demo: WASD+mouse, Esc quits. Prefer Vaultline for the heist slice.");
  return app.run();
}
