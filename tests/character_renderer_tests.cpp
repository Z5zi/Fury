#include "fury/character_animation.hpp"
#include "fury/gl_loader.hpp"
#include "fury/renderer.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace fury;
constexpr int kWidth = 160, kHeight = 128;
constexpr float kActorHeight = 1.8f;
using Image = std::vector<std::uint8_t>;

void require(bool condition, const std::string& description) {
  if (!condition) throw std::runtime_error(description);
}
std::size_t changed_pixels(const Image& a, const Image& b) {
  require(a.size() == b.size(), "Compared frame sizes match");
  std::size_t result = 0;
  for (std::size_t i = 0; i < a.size(); i += 3)
    if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2]) ++result;
  return result;
}
void validate_geometry(const Mesh& mesh) {
  for (const auto& vertex : mesh.vertices) {
    const Vec3 p = vertex.position, n = vertex.normal;
    require(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
            std::isfinite(n.x) && std::isfinite(n.y) && std::isfinite(n.z),
            "Posed render geometry and normals remain finite");
    require(std::fabs(length(n) - 1.f) < .0003f, "Rendered skinned normals remain normalized");
    require(p.y >= -kActorHeight * .5001f, "The rendered actor stays above its ground plane");
  }
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

void verify_model_updates(Fixture& fixture, CharacterRole role, CharacterLod lod, const Image& empty) {
  const auto seed = character_seed(std::string("renderer_") + character_role_name(role));
  const auto model = make_character_model(kActorHeight, role, seed, lod);
  Mesh mesh = model.bind_mesh;
  const auto identity = mesh.geometry_identity;
  const auto* vertices = mesh.vertices.data();
  const auto* indices = mesh.indices.data();
  const auto original_indices = mesh.indices;
  const std::string label = std::string(character_role_name(role)) + (lod == CharacterLod::Near ? " near" : " far");
  // Establish bind through evaluated skinning, including weighted floating-point
  // arithmetic, rather than comparing it with unevaluated source vertex bytes.
  // The common translation makes every vertex take that path before restoration.
  const CharacterPose bind_pose;
  apply_character_pose(model, make_character_pose(model.rig, {}, {0.f, .1f, 0.f}), mesh);
  apply_character_pose(model, bind_pose, mesh);
  validate_geometry(mesh);
  const auto bind = fixture.draw(&mesh);
  require(changed_pixels(bind, empty) > 250, label + ": character visibly reaches the framebuffer");

  CharacterAnimationSample sample;
  sample.seed = seed; sample.idle_time = 2.7;
  const auto idle_pose = sample_character_animation(model.rig, sample);
  require(apply_character_pose(model, idle_pose, mesh), label + ": idle changes skin geometry");
  validate_geometry(mesh);
  const auto idle = fixture.draw(&mesh);
  require(changed_pixels(idle, bind) > 10, label + ": idle geometry reaches the renderer");
  const auto before_gait_builds = fixture.backend->statistics().blas_builds;
  sample.phase_cycles = .17; sample.move_weight = 1.f; sample.speed = 2.2f;
  const auto gait_pose = sample_character_animation(model.rig, sample);
  require(apply_character_pose(model, gait_pose, mesh), label + ": gait changes skin geometry");
  validate_geometry(mesh);
  const auto gait = fixture.draw(&mesh);
  const auto gait_changes = changed_pixels(gait, idle);
  require(gait_changes > 80, label + ": articulated gait changes real pixels");
  if (fixture.backend->kind() == RenderBackendKind::CpuRayTracing)
    require(fixture.backend->statistics().blas_builds == before_gait_builds + 1,
            label + ": dirty character geometry refreshes exactly one cached BLAS");

  sample.phase_cycles = 0.; sample.move_weight = sample.speed = 0.f;
  sample.talk_weight = 1.f; sample.attention_weight = 1.f; sample.attention_yaw = .55f;
  const auto talk_pose = sample_character_animation(model.rig, sample);
  require(apply_character_pose(model, talk_pose, mesh), label + ": dialogue changes skin geometry");
  validate_geometry(mesh);
  const auto talk = fixture.draw(&mesh);
  const auto talk_changes = changed_pixels(talk, idle);
  require(talk_changes > 60, label + ": articulated dialogue changes real pixels");
  const auto revision = mesh.geometry_revision;
  const auto builds = fixture.backend->statistics().blas_builds;
  require(!apply_character_pose(model, talk_pose, mesh), label + ": repeated pose does not dirty geometry");
  require(mesh.geometry_revision == revision && fixture.draw(&mesh) == talk,
          label + ": an unchanged pose preserves both revision and exact image");
  if (fixture.backend->kind() == RenderBackendKind::CpuRayTracing)
    require(fixture.backend->statistics().blas_builds == builds,
            label + ": stable animation does not rebuild the cached BLAS");

  require(apply_character_pose(model, idle_pose, mesh), label + ": restore idle geometry");
  require(fixture.draw(&mesh) == idle, label + ": restored idle restores every image byte");
  require(apply_character_pose(model, bind_pose, mesh), label + ": restore bind geometry");
  require(fixture.draw(&mesh) == bind, label + ": restored bind restores every image byte");
  // Multiple simulation poses can precede one 30/15/8 Hz render upload. A
  // backend must consume the latest revision, not an intermediate cached shape.
  apply_character_pose(model, gait_pose, mesh);
  apply_character_pose(model, talk_pose, mesh);
  apply_character_pose(model, idle_pose, mesh);
  require(fixture.draw(&mesh) == idle, label + ": coalesced revisions upload the final pose");
  require(mesh.geometry_identity == identity && mesh.vertices.data() == vertices &&
          mesh.indices.data() == indices && mesh.indices == original_indices,
          label + ": rendering animation preserves mesh identity, storage and topology");
  if (fixture.backend->kind() == RenderBackendKind::OpenGL)
    require(mesh.gpu_uploaded && mesh.gpu_vao && mesh.gpu_vbo && mesh.gpu_ibo && !mesh.gpu_dirty,
            label + ": OpenGL uploaded and consumed the dirty skinned vertex stream");
  std::cout << "  " << label << ": gait " << gait_changes << ", dialogue " << talk_changes
            << " changed pixels; exact hold/restore\n";
}

void verify_throttled_updates(Fixture& fixture) {
  for (int hz : {30, 15, 8}) {
    const auto lod = hz == 8 ? CharacterLod::Far : CharacterLod::Near;
    const auto model = make_character_model(kActorHeight, CharacterRole::Commuter, 81, lod);
    Mesh mesh = model.bind_mesh;
    CharacterAnimationState state;
    state.sample.seed = model.seed;
    CharacterAnimationInput input;
    input.delta_time = 1.f / 60.f; input.travel_speed = 1.8f;
    input.distance_delta = input.travel_speed * input.delta_time; input.move_weight = 1.f;
    apply_character_pose(model, sample_character_animation(model.rig, state), mesh);
    Image previous = fixture.draw(&mesh);
    int previous_tick = 0, updates = 0, holds = 0;
    for (int frame = 1; frame <= 24; ++frame) {
      advance_character_animation(state, input, kActorHeight);
      const int tick = frame * hz / 60;
      const auto old_revision = mesh.geometry_revision;
      const auto old_builds = fixture.backend->statistics().blas_builds;
      const bool update = tick != previous_tick;
      if (update) {
        require(apply_character_pose(model, sample_character_animation(model.rig, state), mesh),
                "A scheduled distance-driven skin update changes the mesh");
        ++updates;
      }
      const auto image = fixture.draw(&mesh);
      if (update) {
        require(mesh.geometry_revision == old_revision + 1 && changed_pixels(image, previous) > 5,
                "A throttled skin update changes its revision and reaches the framebuffer");
      } else {
        require(mesh.geometry_revision == old_revision && image == previous,
                "Skipped skin updates retain an exact cached frame while simulation advances");
        if (fixture.backend->kind() == RenderBackendKind::CpuRayTracing)
          require(fixture.backend->statistics().blas_builds == old_builds,
                  "Held skin frames reuse CPU-ray geometry");
        ++holds;
      }
      previous = image; previous_tick = tick;
    }
    require(updates >= 3 && holds >= 12, "Each 30/15/8 Hz schedule exercises changes and stable holds");
    std::cout << "  " << hz << " Hz: " << updates << " changed frames, " << holds << " exact holds\n";
  }
}

void verify_backend(SDL_Window* window, RenderBackendKind kind) {
  Fixture fixture(window, kind);
  const auto empty = fixture.draw(nullptr);
  for (unsigned role = 0; role < static_cast<unsigned>(CharacterRole::Count); ++role)
    for (auto lod : {CharacterLod::Near, CharacterLod::Far})
      verify_model_updates(fixture, static_cast<CharacterRole>(role), lod, empty);
  verify_throttled_updates(fixture);
  if (kind == RenderBackendKind::OpenGL) {
    auto get_error = reinterpret_cast<gl::GLenum(*)()>(SDL_GL_GetProcAddress("glGetError"));
    require(get_error && get_error() == 0, "Skinned OpenGL character draws leave no API errors");
  }
}
}  // namespace

int main(int argc, char** argv) {
  const bool use_gl = argc == 2 && std::string(argv[1]) == "--gl";
  if (argc > 1 && !use_gl) {
    std::cerr << "Usage: fury_character_renderer_tests [--gl]\n";
    return 1;
  }
  if (!use_gl) SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
  SDL_setenv("FURY_CPU_THREADS", "2", 1);
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << (use_gl ? "SKIP: " : "FAIL: ") << "SDL video unavailable: " << SDL_GetError() << '\n';
    return use_gl ? 77 : 1;
  }
  if (use_gl) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  }
  SDL_Window* window = SDL_CreateWindow("Character renderer regression", 0, 0, kWidth, kHeight,
      SDL_WINDOW_HIDDEN | (use_gl ? SDL_WINDOW_OPENGL : 0));
  if (!window) {
    std::cerr << (use_gl ? "SKIP: " : "FAIL: ") << "Window unavailable: " << SDL_GetError() << '\n';
    SDL_Quit(); return use_gl ? 77 : 1;
  }
  if (use_gl) {
    const auto context = SDL_GL_CreateContext(window);
    if (!context) {
      std::cout << "SKIP: OpenGL 3.3 unavailable: " << SDL_GetError() << '\n';
      SDL_DestroyWindow(window); SDL_Quit(); return 77;
    }
    SDL_GL_DeleteContext(context);
  }
  int result = 0;
  try {
    if (use_gl) verify_backend(window, RenderBackendKind::OpenGL);
    else {
      verify_backend(window, RenderBackendKind::Software);
      verify_backend(window, RenderBackendKind::CpuRayTracing);
    }
    std::cout << "Character renderer regression passed\n";
  } catch (const std::exception& error) {
    std::cerr << "Character renderer regression: " << error.what() << '\n'; result = 1;
  }
  SDL_DestroyWindow(window); SDL_Quit();
  return result;
}
