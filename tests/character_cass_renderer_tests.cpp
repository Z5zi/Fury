#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/gl_loader.hpp"
#include "fury/renderer.hpp"
#include "fury/render_settings.hpp"
#include <SDL.h>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using namespace fury;
int kWidth = 160, kHeight = 128;
constexpr float kActorHeight = 1.78f;
using Image = std::vector<std::uint8_t>;
void require(bool condition, const std::string &why) {
  if (!condition) throw std::runtime_error(why);
}
std::size_t changed_pixels(const Image &a, const Image &b) {
  require(a.size() == b.size(), "Framebuffer lengths match");
  std::size_t count = 0;
  for (std::size_t i = 0; i < a.size(); i += 3)
    count += a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2];
  return count;
}
struct Fixture {
  SDL_Window* window{};
  std::unique_ptr<IRenderBackend> backend;
  Mesh ground = make_plane(4.f, 4.f, {.15f, .18f, .22f});
  Lighting light;
  Material actor_material, ground_material;
  Vec3 eye{2.7f, 1.65f, 4.6f};
  Mat4 view = look_at(eye, {0.f, .91f, 0.f}, {0.f, 1.f, 0.f});
  Mat4 projection = orthographic(-1.25f, 1.25f, -1.f, 1.f, .1f, 20.f);
  Mat4 actor_transform = translate({0.f, kActorHeight * .5f, 0.f});

  Fixture(SDL_Window* w, RenderBackendKind kind) : window(w) {
    if (kind == RenderBackendKind::Software) backend = create_software_backend();
    else if (kind == RenderBackendKind::CpuRayTracing) backend = create_cpu_ray_backend();
    else backend = create_gl_backend();
    require(backend && backend->create(window, kWidth, kHeight), "Requested renderer factory initializes");
    require(backend->kind() == kind, "Renderer test never silently falls back to another backend");
    light.ambient = {.62f, .65f, .70f}; light.sun_direction = {-.4f, -.8f, -.5f};
    light.sun_color = {1.f, .95f, .88f}; light.sun_intensity = .7f;
    light.ao_strength = 0.f; light.fog_start = 100.f; light.fog_end = 200.f;
    light.enable_shadows = light.enable_reflections = light.enable_bloom = false;
    light.point_light_count = 0;
    actor_material.albedo = {1.f, 1.f, 1.f}; actor_material.roughness = .92f;
    ground_material.albedo = {1.f, 1.f, 1.f}; ground_material.roughness = 1.f;
    if (kind == RenderBackendKind::CpuRayTracing) {
      auto settings = backend->settings();
      settings.trace_mode = TraceMode::RayTraced;
      settings.samples_per_pixel = 1; settings.max_bounces = 1;
      settings.accumulate = false; settings.denoise = false; settings.vsync = false;
      require(backend->configure(settings), "Deterministic direct CPU-ray configuration succeeds");
    }
    if (kind == RenderBackendKind::Software) {
      SDL_RendererInfo info{};
      require(SDL_GetRendererInfo(SDL_GetRenderer(window), &info) == 0 &&
              (info.flags & SDL_RENDERER_SOFTWARE) != 0 && (info.flags & SDL_RENDERER_ACCELERATED) == 0,
              "Software backend uses CPU-only SDL presentation");
    }
    std::cout << "Character renderer: " << backend->name() << '\n';
  }

  Image draw(const Mesh* actor) {
    backend->begin_frame({14, 19, 27, 255});
    backend->set_view_proj(view, projection); backend->set_camera_position(eye);
    backend->set_lighting(light); backend->set_time(0.f);
    backend->set_object_id(1); backend->draw_mesh(ground, Mat4::identity(), ground_material);
    if (actor) {
      backend->set_object_id(2); backend->draw_mesh(*actor, actor_transform, actor_material);
    }
    Image image; int width = 0, height = 0;
    require(backend->read_rgb_framebuffer(image, width, height), "Read the actual backend RGB framebuffer");
    require(width == kWidth && height == kHeight && image.size() == kWidth * kHeight * 3,
            "Actual RGB readback has finite, complete expected dimensions");
    Image again; int rw = 0, rh = 0;
    require(backend->read_rgb_framebuffer(again, rw, rh) && rw == width && rh == height && again == image,
            "Repeated readback never changes the presented character pose");
    if (backend->kind() == RenderBackendKind::CpuRayTracing) {
      const auto statistics = backend->statistics();
      require(statistics.validation_errors == 0 && statistics.rays_traced > 0 &&
              statistics.accumulated_frames == 1 && statistics.software_ray_tracing &&
              !statistics.hardware_ray_tracing,
              "CPU-ray frame is valid and rendered without temporal accumulation");
    }
    return image;
  }
};


void verify_cass(SDL_Window *window, RenderBackendKind kind) {
  Fixture fixture(window, kind);
  const auto empty = fixture.draw(nullptr);
  for (auto lod : {CharacterLod::Near, CharacterLod::Far}) {
    const auto profile = make_character_profile(kActorHeight, CharacterRole::Fence, "NpcFence", lod);
    const auto &model = profile.model;
    require(profile.individualized && model.seed == 0x513cdaa9,
            "Actual Cass shipping profile is rendered, including glasses");
    fixture.actor_material = profile.material;
    Mesh mesh = model.bind_mesh;
    const auto storage = mesh.vertices.data();
    const auto index_storage = mesh.indices.data();
    const auto identity = mesh.geometry_identity;
    const auto source_bind = fixture.draw(&mesh);
    // Match the shared renderer regression: establish the reference through
    // evaluated skinning. Normalization of source float normals may differ by
    // one ULP; the exact restoration assertion must compare evaluated poses.
    // Translation ensures every vertex reaches the evaluator before bind.
    apply_character_pose(model, make_character_pose(model.rig, {}, {0.f, .1f, 0.f}), mesh);
    apply_character_pose(model, CharacterPose{}, mesh);
    const auto bind = fixture.draw(&mesh);
    std::cout << "Source/evaluated-bind pixel differences "
              << (lod == CharacterLod::Near ? "Near" : "Far") << ": "
              << changed_pixels(source_bind, bind) << '\n';
    require(changed_pixels(bind, empty) > 400, "Cass reaches the actual framebuffer");
    std::size_t poses = 0;
    for (int mode = 0; mode < 6; ++mode) {
      Image previous;
      for (int frame : {4, 20}) {
        CharacterAnimationSample sample;
        sample.seed = model.seed;
        sample.phase_cycles = double(frame) / 32;
        sample.idle_time = double(frame) / 8;
        if (mode == 1 || mode == 2 || mode == 5) {
          sample.speed = kActorHeight * (mode == 2 ? 2.8f : mode == 5 ? .55f : .8f);
          sample.move_weight = 1;
        }
        if (mode == 3) {
          sample.talk_weight = sample.attention_weight = 1;
          sample.attention_yaw = frame == 4 ? -.65f : .65f;
          sample.attention_pitch = -.15f;
        }
        if (mode == 4 || mode == 5) sample.crouch_weight = 1;
        apply_character_pose(model, sample_character_animation(model.rig, sample), mesh);
        require(mesh.vertices.data() == storage && mesh.indices.data() == index_storage &&
                mesh.geometry_identity == identity, "Rendering updates the same skinned allocation");
        const auto actual = fixture.draw(&mesh);
        require(changed_pixels(actual, empty) > 250, "Posed Cass remains visible");
        require(changed_pixels(actual, bind) > 5, "Runtime pose changes real Cass pixels");
        const auto held_revision = mesh.geometry_revision;
        const auto held = fixture.draw(&mesh);
        require(held == actual && held_revision == mesh.geometry_revision,
                "Repeated held mesh draw is pixel-exact without source mutation");
        if (!previous.empty() && mode != 4)
          require(changed_pixels(actual, previous) > 1,
                  "Independent animation phases reach the actual framebuffer");
        previous = actual;
        ++poses;
      }
    }
    apply_character_pose(model, CharacterPose{}, mesh);
    const auto restored = fixture.draw(&mesh);
    float max_position_delta = 0, max_normal_delta = 0;
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
      max_position_delta = std::max(max_position_delta, length(mesh.vertices[i].position - model.bind_mesh.vertices[i].position));
      max_normal_delta = std::max(max_normal_delta, length(mesh.vertices[i].normal - model.bind_mesh.vertices[i].normal));
    }
    unsigned max_byte_delta = 0;
    for (std::size_t i = 0; i < restored.size(); ++i)
      max_byte_delta = std::max(max_byte_delta, unsigned(std::abs(int(restored[i]) - int(bind[i]))));
    std::cout << std::setprecision(12) << "Bind restoration diagnostic "
              << (lod == CharacterLod::Near ? "Near" : "Far")
              << ": pixel differences " << changed_pixels(restored, bind)
              << ", max channel delta " << max_byte_delta
              << ", max source position delta " << max_position_delta
              << ", max source normal delta " << max_normal_delta << '\n';
    require(restored == bind, "Bind restoration recovers exact evaluated shipping pixels");
    std::cout << "PASS Cass " << (lod == CharacterLod::Near ? "Near" : "Far")
              << " " << fixture.backend->name() << ": " << poses
              << " motion/attention poses plus exact holds/restoration\n";
  }
  if (kind == RenderBackendKind::OpenGL) {
    auto get_error = reinterpret_cast<gl::GLenum(*)()>(SDL_GL_GetProcAddress("glGetError"));
    require(get_error && get_error() == 0, "Cass OpenGL draw has no API errors");
  }
}
void stress_evidence(SDL_Window *window, const std::filesystem::path &directory) {
  std::filesystem::create_directories(directory);
  Fixture fixture(window, RenderBackendKind::OpenGL);
  std::ofstream manifest(directory / "manifest.json");
  manifest << std::setprecision(9) << "{\n  \"capture_kind\": \"exact_contact_witness_engine_stage\",\n"
           << "  \"game_world_proof\": false,\n  \"source_fingerprint\": \"" << build_source_fingerprint()
           << "\",\n  \"captures\": [";
  bool first = true;
  for (auto lod : {CharacterLod::Near, CharacterLod::Far}) {
    const auto profile = make_character_profile(kActorHeight, CharacterRole::Fence, "NpcFence", lod);
    fixture.actor_material = profile.material;
    for (int frame : {0, 4, 12}) {
      CharacterAnimationSample sample;
      sample.seed = profile.model.seed;
      sample.phase_cycles = double(frame) / 32;
      sample.idle_time = double(frame) / 8;
      sample.speed = kActorHeight * .55f;
      sample.move_weight = sample.crouch_weight = 1;
      Mesh mesh = profile.model.bind_mesh;
      apply_character_pose(profile.model, sample_character_animation(profile.model.rig, sample), mesh);
      for (int view = 0; view < 3; ++view) {
        const Vec3 target = view == 2 ? Vec3{.10f, .14f, -.035f} : Vec3{0, .63f, 0};
        fixture.eye = view == 0 ? Vec3{0, .75f, 3.2f} :
                      view == 1 ? Vec3{3.2f, .75f, 0} : Vec3{1.5f, .16f, -.035f};
        fixture.view = look_at(fixture.eye, target, {0, 1, 0});
        fixture.projection = view == 2 ? orthographic(-.25f, .25f, -.3125f, .3125f, .1f, 20.f) :
                                        orthographic(-.70f, .70f, -.875f, .875f, .1f, 20.f);
        const auto pixels = fixture.draw(&mesh);
        const std::string name = std::string(lod == CharacterLod::Near ? "near" : "far") +
            "-crouch-walk-frame-" + std::to_string(frame) + "-" +
            (view == 0 ? "front" : view == 1 ? "side" : "boot-contact");
        std::ofstream image(directory / (name + ".ppm"), std::ios::binary);
        image << "P6\n" << kWidth << ' ' << kHeight << "\n255\n";
        image.write(reinterpret_cast<const char *>(pixels.data()), pixels.size());
        require(bool(image), "Exact stress witness framebuffer write");
        if (!first) manifest << ','; first = false;
        manifest << "\n    {\"name\":\"" << name << "\",\"pose\":\"crouch-walk\","
                 << "\"phase_cycles\":" << sample.phase_cycles << ",\"idle_time\":" << sample.idle_time
                 << ",\"speed\":" << sample.speed << ",\"move_weight\":1,\"crouch_weight\":1,"
                 << "\"attention_yaw\":0,\"attention_pitch\":0,\"attention_weight\":0,"
                 << "\"backend\":\"" << fixture.backend->name() << "\",\"framebuffer\":\"" << name << ".ppm\"}";
        std::cout << "Saved exact stress witness " << name << '\n';
      }
    }
  }
  manifest << "\n  ]\n}\n";
  require(bool(manifest), "Stress evidence manifest write");
}
void detail_evidence(SDL_Window *window, const std::filesystem::path &directory, RenderBackendKind kind = RenderBackendKind::OpenGL) {
  std::filesystem::create_directories(directory);
  Fixture fixture(window, kind);
  struct View { const char *name; Vec3 target, offset; float half_width; };
  const std::array<View, 6> views{{
      {"hand-front", {-.29f,.715f,.030f}, {-.28f,.025f,.36f}, .13f},
      {"hand-back", {-.29f,.715f,.030f}, {-.28f,.035f,-.36f}, .13f},
      {"hand-inner", {-.29f,.715f,.030f}, {.18f,-.06f,.36f}, .13f},
      {"boots-quarter", {0,.085f,.035f}, {.32f,.13f,.42f}, .21f},
      {"boot-side", {.098f,.085f,.035f}, {.46f,.05f,0}, .17f},
      {"jacket-quarter", {0,1.32f,.060f}, {.80f,.08f,1.10f}, .35f}
  }};
  std::ofstream manifest(directory / "manifest.json");
  manifest << std::setprecision(9) << "{\n  \"capture_kind\": \"shipping_mesh_detail_engine_stage\",\n"
           << "  \"game_world_proof\": false,\n  \"geometry_hidden_or_removed\": false,\n  \"source_fingerprint\": \""
           << build_source_fingerprint() << "\",\n  \"captures\": [";
  bool first = true;
  for (auto lod : {CharacterLod::Near, CharacterLod::Far}) {
    const auto profile = make_character_profile(kActorHeight, CharacterRole::Fence, "NpcFence", lod);
    fixture.actor_material = profile.material;
    for (const auto &view : views) {
      fixture.eye = view.target + view.offset;
      fixture.view = look_at(fixture.eye, view.target, {0,1,0});
      fixture.projection = orthographic(-view.half_width, view.half_width,
          -1.25f*view.half_width, 1.25f*view.half_width, .1f, 20.f);
      const auto pixels = fixture.draw(&profile.model.bind_mesh);
      const std::string name = std::string(lod == CharacterLod::Near ? "near-" : "far-") + view.name;
      std::ofstream image(directory / (name + ".ppm"), std::ios::binary);
      image << "P6\n" << kWidth << ' ' << kHeight << "\n255\n";
      image.write(reinterpret_cast<const char *>(pixels.data()), pixels.size());
      require(bool(image), "Shipping detail framebuffer write");
      if (!first) manifest << ','; first = false;
      manifest << "\n    {\"name\":\"" << name << "\",\"framebuffer\":\"" << name << ".ppm\","
               << "\"camera_eye\":[" << fixture.eye.x << ',' << fixture.eye.y << ',' << fixture.eye.z
               << "],\"camera_target\":[" << view.target.x << ',' << view.target.y << ',' << view.target.z
               << "],\"orthographic_half_width\":" << view.half_width
               << ",\"backend\":\"" << fixture.backend->name() << "\"}";
      std::cout << "Saved shipping detail " << name << '\n';
    }
  }
  manifest << "\n  ]\n}\n";
  require(bool(manifest), "Shipping detail manifest write");
}
} // namespace
int main(int argc, char **argv) {
  const bool stress = argc == 3 && std::string(argv[1]) == "--stress-evidence";
  const bool detail = argc == 3 && std::string(argv[1]) == "--detail-evidence";
  const bool cpu_detail = argc == 3 && std::string(argv[1]) == "--cpu-detail-evidence";
  const bool use_gl = stress || detail || (argc == 2 && std::string(argv[1]) == "--gl");
  if (argc > 1 && !use_gl && !cpu_detail) return 1;
  if (stress || detail || cpu_detail) { kWidth = 640; kHeight = 800; }
  if (!use_gl) SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
  SDL_setenv("FURY_CPU_THREADS", "1", 1);
  if (SDL_Init(SDL_INIT_VIDEO) != 0) return use_gl ? 77 : 1;
  if (use_gl) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  }
  auto *window = SDL_CreateWindow("Cass shipping renderer regression", 0, 0, kWidth, kHeight,
      SDL_WINDOW_HIDDEN | (use_gl ? SDL_WINDOW_OPENGL : 0));
  if (!window) { SDL_Quit(); return use_gl ? 77 : 1; }
  SDL_GLContext context{};
  if (use_gl) {
    context = SDL_GL_CreateContext(window);
    if (!context) { SDL_DestroyWindow(window); SDL_Quit(); return 77; }
  }
  int result = 0;
  try {
    if (cpu_detail) {
      detail_evidence(window, std::filesystem::path(argv[2]) / "software", RenderBackendKind::Software);
      detail_evidence(window, std::filesystem::path(argv[2]) / "cpu-ray", RenderBackendKind::CpuRayTracing);
    } else if (detail) detail_evidence(window, argv[2]);
    else if (stress) stress_evidence(window, argv[2]);
    else if (use_gl) verify_cass(window, RenderBackendKind::OpenGL);
    else {
      verify_cass(window, RenderBackendKind::Software);
      verify_cass(window, RenderBackendKind::CpuRayTracing);
    }
  } catch (const std::exception &e) { std::cerr << "FAIL Cass renderer: " << e.what() << '\n'; result = 1; }
  if (context) SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window); SDL_Quit(); return result;
}
