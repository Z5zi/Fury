#include <fury/fury.hpp>

#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  fury::AppConfig config;
  config.window.title = "Fury Demo";
  config.window.width = 1280;
  config.window.height = 720;
  config.clear_color = {30, 30, 46, 255};  // soft dark indigo
  config.log_fps = true;
  config.fps_log_interval = 1.0f;

  fury::Application app(std::move(config));
  if (!app.init()) {
    fury::Log::error("Failed to initialize Fury demo");
    return EXIT_FAILURE;
  }

  return app.run();
}
