#include "fury/application.hpp"
#include "fury/cutscene.hpp"
#include "fury/log.hpp"
#include "fury/replay.hpp"
#include "test_files.hpp"

#include <SDL.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace fury;
namespace fs = std::filesystem;

#ifdef FURY_TEST_CONTROLLED_TIMER
namespace {
bool controlled_timer = false;
Uint64 controlled_ticks = 0;
}
extern "C" Uint64 __real_SDL_GetTicks64();
extern "C" Uint64 __wrap_SDL_GetTicks64() {
  return controlled_timer ? controlled_ticks : __real_SDL_GetTicks64();
}
#endif

namespace {
constexpr int width = 48;
constexpr int height = 32;
constexpr unsigned frames = 3;

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

struct TemporaryDirectory {
  fs::path path;
  TemporaryDirectory() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
      path = fs::temp_directory_path() /
          ("fury-application-capture-" + std::to_string(stamp) + "-" + std::to_string(attempt));
      if (fs::create_directory(path)) return;
    }
    throw std::runtime_error("Create unique capture test directory");
  }
  ~TemporaryDirectory() {
    try { remove_fury_fixture(path); } catch (...) {}
  }
};

struct ErrorCapture {
  std::ostringstream text;
  std::streambuf* previous{std::cerr.rdbuf(text.rdbuf())};
  ~ErrorCapture() { std::cerr.rdbuf(previous); }
};

AppConfig config() {
  AppConfig result;
  result.window.title = "Bounded application frame capture tests";
  result.window.width = width;
  result.window.height = height;
  result.window.resizable = false;
  result.prefer_opengl = false;
  result.preferred_backend = RenderBackendKind::Software;
  result.capture_mouse = false;
  result.enable_collision = false;
  result.log_fps = false;
  result.max_frames = frames;
  result.fixed_timestep = 1.f / 12.f;
  return result;
}

#ifdef FURY_TEST_CONTROLLED_TIMER
void startup_clock_contract() {
  struct ClockScope {
    ClockScope() { controlled_timer = true; controlled_ticks = 10000; }
    ~ClockScope() { controlled_timer = false; }
  } clock;
  struct InfoCapture {
    std::ostringstream text;
    std::streambuf* previous{std::cout.rdbuf(text.rdbuf())};
    LogLevel level{Log::level()};
    InfoCapture() { Log::set_level(LogLevel::Info); }
    ~InfoCapture() { Log::set_level(level); std::cout.rdbuf(previous); }
  } logs;

  auto settings = config();
  settings.fixed_timestep = 0.f;
  settings.log_fps = true;
  settings.fps_log_interval = .5f;
  Application app(settings);
  require(app.init(), "Initialize controlled-clock application");
  app.scene().add_mesh(make_box({1, 1, 1}, {1, 0, 0}));
  controlled_ticks += 6346;  // Asset setup between init() and run().

  CutsceneStub intro;
  intro.begin();
  ReplayBuffer replay;
  unsigned updates = 0;
  float simulation_time = 0.f;
  const Uint64 delays[] = {16, 738, 200};
  app.on_diagnostic_input = [&](InputState&, unsigned frame) {
    require(frame == updates, "Diagnostic frame index starts at zero");
    controlled_ticks += delays[frame];
  };
  app.on_pre_update = [&](float dt, const InputState&) {
    std::cerr << "Startup clock measured frame=" << updates << " dt=" << dt
              << " elapsed=" << app.timer().elapsed_seconds() << " fps=" << app.timer().fps() << '\n';
    require(std::abs(dt - float(delays[updates]) / 1000.f) < 1e-6f,
            "Pre-update receives only elapsed run time, including a full hitch");
    return false;
  };
  app.on_update = [&](float dt, const InputState&) {
    simulation_time += dt;
    intro.update(dt, app.camera());
    replay.push(app.camera(), dt);
    require(std::abs(app.timer().delta_seconds() - dt) < 1e-6f,
            "Measured Timer delta reaches simulation without replacement");
    require(std::abs(app.timer().elapsed_seconds() - simulation_time) < 1e-6f,
            "Timer elapsed starts at the run boundary");
    if (updates == 0) {
      require(std::abs(dt - .016f) < 1e-6f && std::abs(intro.t - .016f) < 1e-6f,
              "Setup cannot advance simulation or intro; first real 16 ms is retained");
      require(intro.active && replay.count == 0 && app.timer().fps() == 0.f,
              "Setup cannot finish intro, fill replay or contaminate initial FPS");
    }
    if (updates == 1) {
      require(std::abs(dt - .738f) < 1e-6f && std::abs(intro.t - .754f) < 1e-6f,
              "A real 738 ms gameplay hitch remains unclamped");
      require(replay.count == 22 && std::abs(app.timer().fps() - 2.f / .754f) < 1e-5f,
              "Replay and FPS consume the full measured interval");
    }
    ++updates;
  };
  require(app.run() == 0 && updates == frames, "Measured timing run succeeds");
  require(logs.text.str().find("FPS:") != std::string::npos,
          "Measured run emits an FPS sample");

  // The first run left .2 s in its log interval. The next run must not inherit it
  // or the FPS sample history, even though init() is not called again.
  controlled_ticks += 9000;
  logs.text.str("");
  logs.text.clear();
  updates = 0;
  simulation_time = 0.f;
  const Uint64 restart_delays[] = {16, 300, 200};
  app.on_diagnostic_input = [&](InputState&, unsigned frame) {
    require(frame == updates, "Repeated run restarts diagnostic frame index");
    controlled_ticks += restart_delays[frame];
  };
  app.on_pre_update = {};
  app.on_update = [&](float dt, const InputState&) {
    std::cerr << "Startup clock repeated frame=" << updates << " dt=" << dt
              << " elapsed=" << app.timer().elapsed_seconds() << " fps=" << app.timer().fps() << '\n';
    require(std::abs(dt - float(restart_delays[updates]) / 1000.f) < 1e-6f,
            "Repeated run excludes its inter-run pause");
    simulation_time += dt;
    require(std::abs(app.timer().elapsed_seconds() - simulation_time) < 1e-6f,
            "Repeated run starts a new elapsed epoch");
    if (updates < 2) require(app.timer().fps() == 0.f, "Repeated run clears FPS history");
    require(logs.text.str().find("FPS:") == std::string::npos,
            "Repeated run clears the partial FPS logging interval");
    if (updates == 2) require(std::abs(app.timer().fps() - 3.f / .516f) < 1e-5f,
                              "Repeated run reports FPS for only its new frames");
    ++updates;
  };
  require(app.run() == 0 && updates == frames, "Repeated measured run succeeds");
  const auto restart_log = logs.text.str();
  const auto sample = restart_log.find("FPS:");
  require(sample != std::string::npos && restart_log.find("FPS:", sample + 1) == std::string::npos,
          "Repeated run logs exactly one valid fresh FPS sample");

  controlled_ticks += 5000;
  app.config().fixed_timestep = 1.f / 12.f;
  app.config().log_fps = false;
  updates = 0;
  ReplayBuffer fixed_replay, expected_replay;
  app.on_diagnostic_input = [&](InputState&, unsigned frame) { controlled_ticks += delays[frame]; };
  app.on_pre_update = [&](float dt, const InputState&) {
    require(dt == 1.f / 12.f, "Fixed-step pre-update ignores measured setup and hitches");
    return false;
  };
  app.on_update = [&](float dt, const InputState&) {
    std::cerr << "Startup clock fixed frame=" << updates << " dt=" << dt
              << " measured=" << app.timer().delta_seconds() << '\n';
    require(dt == 1.f / 12.f, "Fixed-step simulation keeps exactly the configured timestep");
    require(std::abs(app.timer().delta_seconds() - float(delays[updates]) / 1000.f) < 1e-6f,
            "Fixed-step mode still records the actual unclamped Timer delta");
    app.camera().position.x = float(updates);
    fixed_replay.push(app.camera(), dt);
    expected_replay.push(app.camera(), 1.f / 12.f);
    ++updates;
  };
  require(app.run() == 0 && updates == frames, "Fixed-step timing run succeeds");
  require(fixed_replay.count == 7 && fixed_replay.count == expected_replay.count,
          "Replay sampling cadence stays fixed-step despite measured hitches");
  for (std::size_t i = 0; i < fixed_replay.count; ++i) {
    require(fixed_replay.at_chrono(i).position.x == expected_replay.at_chrono(i).position.x,
            "Fixed-step replay samples are unchanged");
  }
}
#endif

std::string frame_name(unsigned frame) {
  std::ostringstream name;
  name << "frame_" << std::setw(6) << std::setfill('0') << frame << ".ppm";
  return name.str();
}

std::vector<std::uint8_t> read_ppm(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  require(bool(input), "Capture exists and is readable");
  std::string magic;
  int actual_width{}, actual_height{}, maximum{};
  input >> magic >> actual_width >> actual_height >> maximum;
  require(magic == "P6" && actual_width == width && actual_height == height && maximum == 255,
          "Capture has exact P6 RGB header and dimensions");
  require(input.get() == '\n', "Capture header has one newline before pixel data");
  const std::vector<std::uint8_t> rgb{std::istreambuf_iterator<char>(input), {}};
  require(rgb.size() == std::size_t(width) * height * 3, "Capture contains exactly one RGB framebuffer");
  return rgb;
}

void expect_failure(AppConfig settings, const char* diagnostic) {
  unsigned updates = 0;
  Application app(std::move(settings));
  app.on_update = [&](float, const InputState&) { ++updates; app.request_quit(); };
  ErrorCapture errors;
  require(app.run() != 0, "Invalid capture request returns failure");
  require(updates == 0, "Invalid capture request fails before simulation updates");
  require(errors.text.str().find(diagnostic) != std::string::npos, "Capture failure includes actionable diagnostic");
}

void invalid_requests(const fs::path& root) {
  auto settings = config();
  settings.capture_sequence_directory = (root / "unbounded").string();
  settings.max_frames = 0;
  {
    Application app(settings);
    ErrorCapture errors;
    require(!app.init(), "Unbounded sequence rejected at initialization");
    require(!app.window().valid(), "Invalid sequence does not create a window");
  }
  expect_failure(settings, "max_frames > 0");
  require(!fs::exists(settings.capture_sequence_directory), "Unbounded request creates no output directory");

  const auto file = root / "regular-file";
  { std::ofstream output(file); output << "sentinel"; }
  settings = config();
  settings.capture_sequence_directory = file.string();
  expect_failure(settings, "Frame sequence directory failed");
  settings.capture_sequence_directory = (file / "nested").string();
  expect_failure(settings, "Frame sequence directory failed");

  const auto nonempty = root / "nonempty";
  fs::create_directory(nonempty);
  { std::ofstream output(nonempty / frame_name(1)); output << "preserve me"; }
  settings.capture_sequence_directory = nonempty.string();
  expect_failure(settings, "must be empty");
  std::ifstream sentinel(nonempty / frame_name(1));
  require(std::string(std::istreambuf_iterator<char>(sentinel), {}) == "preserve me",
          "Existing sequence contents are never overwritten");

  settings.capture_sequence_directory = (root / "same-destination").string();
  settings.capture_path = (fs::path(settings.capture_sequence_directory) / frame_name(1)).string();
  expect_failure(settings, "must be outside");
  settings.capture_path = (fs::path(settings.capture_sequence_directory) / "nested" / "last.ppm").string();
  expect_failure(settings, "must be outside");

  settings = config();
  settings.capture_sequence_directory = std::string("bad\0directory", 13);
  expect_failure(settings, "NUL");
  settings.capture_sequence_directory.clear();
  settings.capture_path = std::string("bad\0capture", 11);
  expect_failure(settings, "NUL");

  // A caller may edit config after initializing an otherwise valid app.
  settings = config();
  Application app(settings);
  require(app.init(), "Initialize before capture configuration changes");
  app.config().capture_sequence_directory = (root / "late-unbounded").string();
  app.config().max_frames = 0;
  ErrorCapture errors;
  require(app.run() != 0 && errors.text.str().find("max_frames > 0") != std::string::npos,
          "run revalidates changed configuration even after initialization");
}

std::vector<std::vector<std::uint8_t>> capture_motion(const fs::path& directory,
    const fs::path& last, RenderBackendKind backend, bool mutate_options = false) {
  auto settings = config();
  settings.preferred_backend = backend;
  settings.capture_sequence_directory = directory.string();
  settings.capture_path = last.string();
  Application app(settings);
  app.camera().position = {0, 0, 4};
  require(app.init(), "Initialize bounded capture application");
  if (backend == RenderBackendKind::CpuRayTracing) {
    auto rendering = app.renderer().settings();
    rendering.trace_mode = TraceMode::RayTraced;
    rendering.samples_per_pixel = 1;
    rendering.denoise = false;
    require(app.renderer().configure(rendering), "Configure deterministic CPU ray capture");
  }

  Mesh mesh = make_box({.8f, .8f, .3f}, {1, .15f, .05f});
  unsigned step = 0;
  std::vector<std::vector<std::uint8_t>> expected;
  app.on_update = [&](float dt, const InputState&) {
    require(std::abs(dt - 1.f / 12.f) < 1e-7f, "Fixed timestep reaches simulation unchanged");
    ++step;
    if (step > frames) app.request_quit();  // Bound the regression test even if the capture limit breaks.
    if (mutate_options) {
      app.config().max_frames = 0;
      app.config().capture_sequence_directory.clear();
      app.config().capture_path.clear();
    }
  };
  app.on_render = [&] {
    app.renderer().draw_mesh(mesh, translate({(float(step) - 2.f) * .9f, 0, 0}), {}, 1);
  };
  app.on_hud = [&] {
    app.renderer().draw_hud_rect(0, 0, 4, 4, {17, 219, 83, 255});
    int actual_width{}, actual_height{};
    expected.emplace_back();
    require(app.renderer().read_rgb_framebuffer(expected.back(), actual_width, actual_height),
            "Read independently rendered frame after HUD");
    require(actual_width == width && actual_height == height, "Renderer dimensions match requested capture");
  };
  require(app.run() == 0, "Bounded sequence capture succeeds");
  require(step == frames && expected.size() == frames, "Exactly the bounded number of frames rendered");
  require(std::distance(fs::directory_iterator(directory), fs::directory_iterator{}) == frames,
          "Exactly one file per rendered frame, with no hidden or stale files");
  for (unsigned index = 0; index < frames; ++index) {
    const auto actual = read_ppm(directory / frame_name(index + 1));
    require(actual == expected[index], "Sequence file is byte-identical to the actual renderer readback");
    require(actual[0] == 17 && actual[1] == 219 && actual[2] == 83,
            "Capture includes the current frame HUD with top-left origin");
  }
  require(expected[0] != expected[1] && expected[1] != expected[2],
          "Moving rendered geometry changes successive captured frames");
  require(read_ppm(last) == expected.back(), "Ordinary final capture matches the final sequence frame exactly");
  if (backend == RenderBackendKind::CpuRayTracing) {
    require(app.renderer().statistics().frame_index == frames,
            "Readback and presentation do not render CPU ray frames twice");
  }
  return expected;
}

void runtime_failures(const fs::path& root) {
  auto settings = config();
  settings.capture_sequence_directory = (root / "write-failure").string();
  {
    Application app(settings);
    app.on_update = [&](float, const InputState&) {
      fs::create_directory(fs::path(settings.capture_sequence_directory) / frame_name(1));
    };
    ErrorCapture errors;
    require(app.run() != 0 && errors.text.str().find("Frame capture open failed") != std::string::npos,
            "A sequence output that cannot be opened reports failure");
    require(!fs::exists(fs::path(settings.capture_sequence_directory) / frame_name(2)),
            "Sequence stops at the first failed write");
  }
  settings.capture_sequence_directory = (root / "read-failure").string();
  {
    Application app(settings);
    app.on_hud = [&] { app.renderer().destroy(); };
    ErrorCapture errors;
    require(app.run() != 0 && errors.text.str().find("framebuffer read failed") != std::string::npos,
            "Unavailable renderer readback reports failure instead of silently omitting frames");
    require(fs::is_empty(settings.capture_sequence_directory), "Failed readback writes no bogus image");
  }
  settings.capture_sequence_directory.clear();
  settings.capture_path = (root / "last-is-directory").string();
  fs::create_directory(settings.capture_path);
  {
    Application app(settings);
    ErrorCapture errors;
    require(app.run() != 0 && errors.text.str().find("Frame capture open failed") != std::string::npos,
            "Ordinary last-frame capture errors remain visible");
  }
}

void legacy_and_hud(const fs::path& root) {
  auto settings = config();
  settings.capture_path = (root / "legacy" / "last.ppm").string();
  settings.show_hud = false;
  unsigned updates = 0;
  unsigned hud_calls = 0;
  {
    Application app(settings);
    app.on_update = [&](float, const InputState&) { ++updates; };
    app.on_hud = [&] { ++hud_calls; app.renderer().draw_hud_rect(0, 0, width, height, {255, 0, 0, 255}); };
    require(app.run() == 0 && updates == frames && hud_calls == 0,
            "Legacy last-frame capture preserves bounded rendering and show_hud=false");
  }
  const auto image = read_ppm(settings.capture_path);
  require(image[0] == settings.clear_color.r && image[1] == settings.clear_color.g && image[2] == settings.clear_color.b,
          "Disabled HUD is absent from ordinary last-frame capture");

  settings.capture_sequence_directory = (root / "quit-early").string();
  settings.capture_path = (root / "early-final.ppm").string();
  {
    Application app(settings);
    app.on_update = [&](float, const InputState&) { app.request_quit(); };
    require(app.run() == 0, "Requested early quit preserves normal application behavior");
  }
  require(fs::exists(fs::path(settings.capture_sequence_directory) / frame_name(1)) &&
          !fs::exists(fs::path(settings.capture_sequence_directory) / frame_name(2)) &&
          !fs::exists(settings.capture_path),
          "Early quit captures only actual frames and does not invent a final bounded frame");

  settings.capture_sequence_directory = (root / "wide-numbering").string();
  settings.max_frames = 1000000;
  settings.capture_path.clear();
  {
    Application app(settings);
    app.on_update = [&](float, const InputState&) { app.request_quit(); };
    app.on_hud = [&] { throw std::runtime_error("Disabled HUD must not run during sequence capture"); };
    require(app.run() == 0, "Wide frame numbering remains bounded by ordinary early quit");
  }
  require(read_ppm(fs::path(settings.capture_sequence_directory) / "frame_0000001.ppm") == image,
          "Padding expands to fit the configured bound and sequences honor show_hud=false");
}
}  // namespace

int main(int, char**) {
  try {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    SDL_setenv("FURY_CPU_THREADS", "2", 1);
    SDL_setenv("FURY_SURFACE_DETAIL", "0", 1);
    Log::set_level(LogLevel::Error);
#ifdef FURY_TEST_CONTROLLED_TIMER
    startup_clock_contract();
#endif
    TemporaryDirectory temporary;
    invalid_requests(temporary.path);
    for (const auto backend : {RenderBackendKind::Software, RenderBackendKind::CpuRayTracing}) {
      const std::string name = backend == RenderBackendKind::Software ? "raster" : "ray";
      const auto first = capture_motion(temporary.path / (name + "-first"), temporary.path / (name + "-last.ppm"), backend);
      fs::create_directory(temporary.path / (name + "-second"));
      const auto second = capture_motion(temporary.path / (name + "-second"), temporary.path / (name + "-last2.ppm"), backend, true);
      require(first == second, "Repeated fixed-step captures are deterministic on each CPU backend");
    }
    runtime_failures(temporary.path);
    legacy_and_hud(temporary.path);
    std::cout << "Application frame capture checks passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Application frame capture check failed: " << error.what() << '\n';
    return 1;
  }
}
