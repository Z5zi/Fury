// Individual-profile framebuffer regression. Uses Fury's renderer API and the
// exact Vale profile; this is a diagnostic fixture, not game-world proof.
#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/gl_loader.hpp"
#include "fury/renderer.hpp"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace fury;
namespace {
constexpr int width = 288, height = 384;
using Pixels = std::vector<std::uint8_t>;
void require(bool ok, const std::string& message) {
  if (!ok) throw std::runtime_error(message);
}
unsigned difference(const Pixels& a, const Pixels& b) {
  require(a.size() == b.size(), "Comparable complete framebuffer sizes");
  unsigned result = 0;
  for (unsigned i = 0; i < a.size(); i += 3)
    result += a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2];
  return result;
}
void save(const std::filesystem::path& directory, const std::string& label,
          const Pixels& pixels) {
  if (directory.empty()) return;
  std::filesystem::create_directories(directory);
  std::ofstream out(directory / (label + ".ppm"), std::ios::binary);
  out << "P6\n" << width << ' ' << height << "\n255\n";
  out.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
  require(bool(out), "Complete framebuffer evidence written");
}
struct ValeRendererFixture {
  SDL_Window* window;
  std::unique_ptr<IRenderBackend> backend;
  Mesh ground = make_plane(5, 5, {.13f, .16f, .19f});
  Material floor_material;
  Lighting lighting;
  ValeRendererFixture(SDL_Window* w, RenderBackendKind kind): window(w) {
    backend = kind == RenderBackendKind::OpenGL ? create_gl_backend()
              : kind == RenderBackendKind::Software ? create_software_backend()
              : create_cpu_ray_backend();
    require(backend && backend->create(window, width, height), "Renderer initialized");
    require(backend->kind() == kind, "No renderer fallback in individual test");
    lighting.ambient = {.40f, .43f, .47f};
    lighting.sun_direction = {-.4f, -.8f, -.5f};
    lighting.sun_color = {1, .95f, .88f};
    lighting.sun_intensity = .8f;
    lighting.ao_strength = 0;
    lighting.fog_start = 100; lighting.fog_end = 200;
    lighting.enable_shadows = lighting.enable_reflections = lighting.enable_bloom = false;
    lighting.point_light_count = 0;
    floor_material.albedo = {1, 1, 1}; floor_material.roughness = 1;
    if (kind == RenderBackendKind::CpuRayTracing) {
      auto settings = backend->settings();
      settings.trace_mode = TraceMode::RayTraced;
      settings.samples_per_pixel = 1; settings.max_bounces = 1;
      settings.accumulate = false; settings.denoise = false; settings.vsync = false;
      require(backend->configure(settings), "Deterministic CPU ray configuration");
    }
    std::cout << "Individual Vale renderer: " << backend->name() << '\n';
  }
  Pixels draw(const Mesh* actor, const Material& material) {
    const Vec3 camera{2.0f, 1.3f, 3.4f};
    backend->begin_frame({24, 29, 35, 255});
    backend->set_view_proj(look_at(camera, {0, .92f, 0}, {0, 1, 0}),
                          perspective(38.f * 3.141592653589793f / 180.f,
                                      float(width) / height, .1f, 30.f));
    backend->set_camera_position(camera); backend->set_lighting(lighting);
    backend->set_time(0);
    backend->set_object_id(1); backend->draw_mesh(ground, Mat4::identity(), floor_material);
    if (actor) {
      backend->set_object_id(2);
      backend->draw_mesh(*actor, translate({0, .93f, 0}), material);
    }
    Pixels pixels, repeated; int w{}, h{}, rw{}, rh{};
    require(backend->read_rgb_framebuffer(pixels, w, h) &&
                w == width && h == height && pixels.size() == width * height * 3,
            "Complete actual RGB framebuffer");
    require(backend->read_rgb_framebuffer(repeated, rw, rh) &&
                w == rw && h == rh && pixels == repeated,
            "Readback is idempotent");
    const auto metrics = backend->statistics();
    require(metrics.validation_errors == 0, "No backend validation errors");
    if (backend->kind() == RenderBackendKind::CpuRayTracing)
      require(metrics.rays_traced > 0 && metrics.accumulated_frames == 1 &&
                  metrics.software_ray_tracing && !metrics.hardware_ray_tracing,
              "Real CPU ray frame without accumulation or claimed hardware RT");
    return pixels;
  }
};
void verify(ValeRendererFixture& r, CharacterLod lod, const std::filesystem::path& out) {
  const auto profile = make_character_profile(1.86f, CharacterRole::Security, "NpcAlleyHmpd", lod);
  const auto& model = profile.model;
  require(profile.individualized && model.dual_quaternion_skinning, "Exact individual profile");
  auto mesh = model.bind_mesh;
  const auto* storage = mesh.vertices.data(); const auto* indices = mesh.indices.data();
  const auto geometry_id = mesh.geometry_identity;
  const auto topology = mesh.indices;
  const auto empty = r.draw(nullptr, profile.material);
  apply_character_pose(model, make_character_pose(model.rig, {}, {0, .01f, 0}), mesh);
  apply_character_pose(model, CharacterPose{}, mesh);
  const auto bind = r.draw(&mesh, profile.material);
  require(difference(empty, bind) > 1000, "Vale visibly reaches framebuffer");
  const std::string prefix = lod == CharacterLod::Near ? "near-" : "far-";
  save(out, prefix + "bind", bind);
  const std::array<std::string, 8> names{{"idle", "walk", "run", "crouch", "crouch-walk", "talk", "attention-left", "attention-right"}};
  for (unsigned pose = 0; pose < names.size(); ++pose) {
    CharacterAnimationSample sample;
    sample.seed = model.seed; sample.phase_cycles = .23; sample.idle_time = 1.7;
    sample.speed = pose == 1 ? 1.4f : pose == 2 ? 3.8f : pose == 4 ? 1.023f : 0;
    sample.move_weight = sample.speed > 0 ? 1 : 0;
    sample.crouch_weight = (pose == 3 || pose == 4) ? 1 : 0;
    sample.talk_weight = pose >= 5 ? 1 : 0;
    sample.attention_weight = pose >= 6 ? 1 : 0;
    sample.attention_yaw = pose == 6 ? -.9f : pose == 7 ? .9f : 0;
    sample.attention_pitch = pose == 6 ? -.4f : pose == 7 ? .4f : 0;
    sample.stride_length = character_stride_length(model.rig.height, sample.speed, sample.crouch_weight);
    const auto target = sample_character_animation(model.rig, sample);
    const auto builds_before = r.backend->statistics().blas_builds;
    require(apply_character_pose(model, target, mesh), "Changed pose updates mesh");
    for (const auto& vertex : mesh.vertices)
      require(std::isfinite(vertex.position.x) && std::isfinite(vertex.position.y) &&
                  std::isfinite(vertex.position.z) && std::fabs(length(vertex.normal) - 1) < .001f,
              "Finite unit posed geometry reaches renderer");
    const auto posed = r.draw(&mesh, profile.material);
    const auto changed = difference(bind, posed);
    require(changed > 100, "Pose visibly changes actual pixels");
    if (r.backend->kind() == RenderBackendKind::CpuRayTracing)
      require(r.backend->statistics().blas_builds == builds_before + 1,
              "Exactly one dirty actor BLAS rebuild");
    const auto revision = mesh.geometry_revision;
    const auto builds = r.backend->statistics().blas_builds;
    require(!apply_character_pose(model, target, mesh), "Identical pose is a true no-op");
    require(mesh.geometry_revision == revision && r.draw(&mesh, profile.material) == posed,
            "Stable pose has exact frame and revision");
    if (r.backend->kind() == RenderBackendKind::CpuRayTracing)
      require(r.backend->statistics().blas_builds == builds, "Stable pose reuses BLAS");
    save(out, prefix + names[pose], posed);
    apply_character_pose(model, CharacterPose{}, mesh);
    require(r.draw(&mesh, profile.material) == bind, "Exact bind pixels restored after every pose");
    require(mesh.vertices.data() == storage && mesh.indices.data() == indices &&
                mesh.geometry_identity == geometry_id && mesh.indices == topology,
            "Pose and rendering retain storage, identity and topology");
    if (r.backend->kind() == RenderBackendKind::OpenGL)
      require(mesh.gpu_uploaded && mesh.gpu_vao && mesh.gpu_vbo && mesh.gpu_ibo && !mesh.gpu_dirty,
              "Dirty Vale geometry is really uploaded to OpenGL");
    std::cout << prefix << names[pose] << " changed_pixels=" << changed << '\n';
  }
  if (r.backend->kind() == RenderBackendKind::OpenGL) {
    const auto get_error = reinterpret_cast<gl::GLenum(*)()>(SDL_GL_GetProcAddress("glGetError"));
    require(get_error && get_error() == 0, "No GL errors after individual profile draws");
  }
}
} // namespace
int main(int argc, char** argv) {
  bool gl = false; std::filesystem::path out;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--gl") gl = true;
    else if (arg == "--output" && i + 1 < argc) out = argv[++i];
    else { std::cerr << "Usage: fury_character_vale_renderer_tests [--gl] [--output directory]\n"; return 1; }
  }
  if (!gl) SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
  SDL_setenv("FURY_CPU_THREADS", "2", 1);
  if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::cerr << SDL_GetError() << '\n'; return gl ? 77 : 1; }
  if (gl) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  }
  SDL_Window* window = SDL_CreateWindow("Vale individual renderer verification", 0, 0, width, height,
                                       SDL_WINDOW_HIDDEN | (gl ? SDL_WINDOW_OPENGL : 0));
  if (!window) { std::cerr << SDL_GetError() << '\n'; SDL_Quit(); return gl ? 77 : 1; }
  int result = 0;
  try {
    for (const auto kind : {RenderBackendKind::Software, RenderBackendKind::CpuRayTracing, RenderBackendKind::OpenGL}) {
      if ((kind == RenderBackendKind::OpenGL) != gl) continue;
      ValeRendererFixture renderer(window, kind);
      auto directory = out.empty() ? out : out / (gl ? "gl" : kind == RenderBackendKind::Software ? "software" : "cpu-ray");
      for (const auto lod : {CharacterLod::Near, CharacterLod::Far}) verify(renderer, lod, directory);
    }
    std::cout << "Vale individual renderer checks passed\n";
  } catch (const std::exception& e) { std::cerr << "Vale renderer validation: " << e.what() << '\n'; result = 1; }
  SDL_DestroyWindow(window); SDL_Quit(); return result;
}
