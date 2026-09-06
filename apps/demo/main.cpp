#include <fury/fury.hpp>

#include <cstdlib>

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  fury::AppConfig config;
  config.window.title = "Fury";
  config.window.width = 1280;
  config.window.height = 720;
  config.clear_color = {30, 30, 46, 255};
  config.log_fps = true;
  config.fps_log_interval = 1.0f;
  config.prefer_opengl = true;
  config.capture_mouse = true;

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
  app.scene().add_entity(std::move(e));

  auto* floor = app.scene().add_mesh(
      fury::make_plane(20.f, 20.f, {0.25f, 0.28f, 0.32f}));
  fury::Entity f;
  f.name = "Floor";
  f.mesh = floor;
  app.scene().add_entity(std::move(f));

  app.camera().position = {0.f, 2.f, 8.f};
  fury::Log::info("Demo: WASD+mouse, Esc quits. Prefer Vaultline for the heist slice.");
  return app.run();
}
