#include "fury/application.hpp"
#include "character_gait_trace_inputs.hpp"
#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/texture.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fury {
CharacterProfile make_cass_glasses_removed_diagnostic(float height, CharacterLod lod);
}

namespace {
using namespace fury;

struct Options {
  std::string identity{"NpcCivA"}, pose{"bind"}, capture, sequence, report;
  CharacterRole role{CharacterRole::Commuter};
  CharacterLod lod{CharacterLod::Near};
  RenderBackendKind backend{RenderBackendKind::OpenGL};
  unsigned width{720}, image_height{960}, frames{0}, spp{1}, bounces{4};
  float height{1.75f}, yaw{0.f}, elevation{0.f}, distance{0.f}, fps{12.f};
  float attention_yaw{0.f}, attention_pitch{0.f};
  float speed_override{-1.f};
  double idle_time_override{-1.};
  double phase{0.0};
  bool face{false}, clay{false}, legacy{false}, animate{false}, vertex_material{false}, cass_no_glasses{false};
};

std::string normalized(std::string text) {
  text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char c) {
    return c == '_' || c == '-';
  }), text.end());
  for (char& c : text) c = char(std::tolower(static_cast<unsigned char>(c)));
  return text;
}

double number(const std::string& text, const std::string& flag) {
  std::size_t used{};
  const double result = std::stod(text, &used);
  if (used != text.size() || !std::isfinite(result))
    throw std::runtime_error(flag + " requires a finite number");
  return result;
}

unsigned unsigned_number(const std::string& text, const std::string& flag) {
  if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
    throw std::runtime_error(flag + " requires a nonnegative integer");
  const auto value = std::stoull(text);
  if (value > std::numeric_limits<unsigned>::max())
    throw std::runtime_error(flag + " is too large");
  return static_cast<unsigned>(value);
}

void help() {
  std::cout <<
    "Fury Characterlab: diagnostic engine stage, NOT a Vaultline game-world capture\n"
    "--soft | --cpu-ray            Default: OpenGL (engine software fallback allowed)\n"
    "--spp 1..64 --bounces 1..16   CPU ray settings (defaults: 1 spp, 4 bounces)\n"
    "--id NpcCivA --role Commuter  Stable identity and role (enum names accepted)\n"
    "--height 1.75                Actor height in meters, not image dimensions\n"
    "--width 720 --image-height 960\n"
    "--yaw 0 --elevation 0        Camera orbit degrees: 0=front/+Z, 90=+X side\n"
    "--distance meters --face    Fixed body or face framing; distance is optional\n"
    "--pose bind|idle|walk|run|talk|crouch|crouch-walk --phase 0\n"
    "--speed 0..20                 Optional m/s override for locomotion poses\n"
    "--idle-time 0..1000000        Optional exact animation idle-time seconds\n"
    "--attention-yaw -0.9..0.9 --attention-pitch -0.4..0.4 (radians, non-bind poses)\n"
    "--lod near|far --legacy-profile --clay --vertex-material\n"
    "--cass-no-glasses            Labeled Cass-only face diagnostic; not the shipping identity\n"
    "--frames N --capture last.ppm --capture-sequence new-directory --fps 12\n"
    "--animate                   Advance animation without a frame sequence\n"
    "--report metrics.json --no-hud (the stage never draws HUD)\n"
    "Stills hold the selected phase. Sequences advance at the simulated fps;\n"
    "the first frame samples exactly --phase. Lighting and actor origin stay fixed.\n"
    "Roles: Commuter Market Dock Security Fence CrewScout CrewTech Enforcer\n"
    "       Player Ghost BankStaff\n";
}

Options parse_options(int argc, char** argv) {
  Options o;
  bool soft = false, cpu = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto value = [&]() -> std::string {
      if (i + 1 >= argc) throw std::runtime_error("Missing value for " + arg);
      return argv[++i];
    };
    auto real = [&]() {
      const double result = number(value(), arg);
      if (std::abs(result) > std::numeric_limits<float>::max())
        throw std::runtime_error(arg + " is too large");
      return static_cast<float>(result);
    };
    if (arg == "--soft") soft = true;
    else if (arg == "--cpu-ray") cpu = true;
    else if (arg == "--id") o.identity = value();
    else if (arg == "--role") {
      const auto role = normalized(value());
      bool found = false;
      for (unsigned r = 0; r < static_cast<unsigned>(CharacterRole::Count); ++r) {
        if (normalized(character_role_name(static_cast<CharacterRole>(r))) == role) {
          o.role = static_cast<CharacterRole>(r); found = true; break;
        }
      }
      if (!found) throw std::runtime_error("Unknown character role: " + role);
    }
    else if (arg == "--height") o.height = real();
    else if (arg == "--width") o.width = unsigned_number(value(), arg);
    else if (arg == "--image-height") o.image_height = unsigned_number(value(), arg);
    else if (arg == "--frames") o.frames = unsigned_number(value(), arg);
    else if (arg == "--spp") o.spp = unsigned_number(value(), arg);
    else if (arg == "--bounces") o.bounces = unsigned_number(value(), arg);
    else if (arg == "--yaw") o.yaw = real();
    else if (arg == "--elevation") o.elevation = real();
    else if (arg == "--distance") {
      o.distance = real();
      if (o.distance <= 0.f) throw std::runtime_error("--distance must be positive");
    }
    else if (arg == "--fps") o.fps = real();
    else if (arg == "--speed") {
      o.speed_override=real();
      if(o.speed_override<0 || o.speed_override>20)
        throw std::runtime_error("--speed must be in 0..20 meters per second");
    }
    else if (arg == "--idle-time") {
      o.idle_time_override=number(value(),arg);
      if(o.idle_time_override<0 || o.idle_time_override>1000000)
        throw std::runtime_error("--idle-time must be in 0..1000000 seconds");
    }
    else if (arg == "--phase") o.phase = number(value(), arg);
    else if (arg == "--attention-yaw") o.attention_yaw = real();
    else if (arg == "--attention-pitch") o.attention_pitch = real();
    else if (arg == "--pose") o.pose = normalized(value());
    else if (arg == "--lod") {
      const auto lod = normalized(value());
      if (lod != "near" && lod != "far") throw std::runtime_error("--lod must be near or far");
      o.lod = lod == "near" ? CharacterLod::Near : CharacterLod::Far;
    }
    else if (arg == "--capture") o.capture = value();
    else if (arg == "--capture-sequence") o.sequence = value();
    else if (arg == "--report") o.report = value();
    else if (arg == "--face") o.face = true;
    else if (arg == "--clay") o.clay = true;
    else if (arg == "--vertex-material") o.vertex_material = true;
    else if (arg == "--legacy-profile") o.legacy = true;
    else if (arg == "--cass-no-glasses") o.cass_no_glasses = true;
    else if (arg == "--animate") o.animate = true;
    else if (arg == "--no-hud") {} // Diagnostic captures always omit HUD.
    else throw std::runtime_error("Unknown argument: " + arg + " (see --help)");
  }
  if (soft && cpu) throw std::runtime_error("Choose --soft or --cpu-ray");
  if (soft) o.backend = RenderBackendKind::Software;
  if (cpu) o.backend = RenderBackendKind::CpuRayTracing;
  if (o.identity.empty()) throw std::runtime_error("--id must not be empty");
  if (o.height <= 0.f || o.height > 100.f) throw std::runtime_error("--height must be in (0,100] meters");
  if (o.width < 64 || o.width > 7680 || o.image_height < 64 || o.image_height > 7680)
    throw std::runtime_error("Image dimensions must be in 64..7680 pixels");
  if (o.fps < .1f || o.fps > 240.f) throw std::runtime_error("--fps must be in 0.1..240");
  if (o.spp < 1 || o.spp > 64) throw std::runtime_error("--spp must be in 1..64");
  if (o.bounces < 1 || o.bounces > 16) throw std::runtime_error("--bounces must be in 1..16");
  if (std::abs(o.elevation) >= 90.f) throw std::runtime_error("--elevation must be between -90 and 90 degrees");
  if (std::abs(o.attention_yaw) > .9f || std::abs(o.attention_pitch) > .4f)
    throw std::runtime_error("Attention must be yaw -0.9..0.9 and pitch -0.4..0.4 radians");
  if (o.pose == "bind" && (o.attention_yaw != 0.f || o.attention_pitch != 0.f))
    throw std::runtime_error("Attention controls require a non-bind pose");
  if(o.idle_time_override>=0 && o.pose=="bind")
    throw std::runtime_error("--idle-time requires a non-bind pose");
  if(o.speed_override>=0 && o.pose!="walk" && o.pose!="run" && o.pose!="crouchwalk")
    throw std::runtime_error("--speed requires walk, run or crouch-walk");
  const std::string poses[]{"bind", "idle", "walk", "run", "talk", "crouch", "crouchwalk"};
  if (std::find(std::begin(poses), std::end(poses), o.pose) == std::end(poses))
    throw std::runtime_error("Unknown pose: " + o.pose);
  if ((!o.capture.empty() || !o.sequence.empty()) && !o.frames)
    throw std::runtime_error("Capture requires a positive --frames limit");
  if (!o.report.empty()) {
    const auto report = std::filesystem::weakly_canonical(o.report);
    if (!o.capture.empty() && report == std::filesystem::weakly_canonical(o.capture))
      throw std::runtime_error("--report must not overwrite --capture");
    if (!o.sequence.empty()) {
      const auto relative = report.lexically_relative(std::filesystem::weakly_canonical(o.sequence));
      if (!relative.empty() && *relative.begin() != "..")
        throw std::runtime_error("--report must be outside the frame sequence directory");
    }
  }
  o.yaw = std::remainder(o.yaw, 360.f);
  o.phase -= std::floor(o.phase);
  return o;
}

std::string json_string(const std::string& value) {
  std::ostringstream out;
  out << '"';
  for (const unsigned char c : value) {
    if (c == '"' || c == '\\') out << '\\' << c;
    else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
    else out << c;
  }
  out << '"';
  return out.str();
}

void json_vec(std::ostream& out, const Vec3& v) { out << '[' << v.x << ',' << v.y << ',' << v.z << ']'; }

CharacterAnimationState read_state() {
  const char* path=std::getenv("FURY_REPLAY_STATE");
  if(!path)throw std::runtime_error("Expected exact production state file");
  std::ifstream in(path);int frame{},visible{};CharacterAnimationState state{};
  auto&s=state.sample;
  in>>frame>>visible>>s.phase_cycles>>s.idle_time>>s.speed>>s.stride_length>>s.move_weight>>s.turn_rate>>s.crouch_weight>>s.attention_yaw>>s.attention_pitch>>s.attention_weight>>s.talk_weight>>s.seed>>state.previous_yaw>>state.initialized>>state.gait_started>>state.support_fraction;
  if(!in)throw std::runtime_error("Incomplete production state");
  read_gait_extension(in,state);
  return state;
}
CharacterAnimationSample animation_sample(const Options&,const CharacterModel&,double) {return read_state().sample;}

} // namespace

int main(int argc, char** argv) {
  using namespace fury;
  try {
    for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--help") { help(); return 0; }
    const Options o = parse_options(argc, argv);
    if(o.clay&&o.vertex_material)throw std::invalid_argument("Choose either clay or vertex-material diagnostic");
    if(o.cass_no_glasses && (o.identity != "NpcFence" || o.role != CharacterRole::Fence || o.legacy))
      throw std::invalid_argument("--cass-no-glasses requires the individual NpcFence/Fence profile");
    CharacterProfile profile;
    if(o.cass_no_glasses) profile = make_cass_glasses_removed_diagnostic(o.height, o.lod);
    else if (o.legacy) profile.model = make_character_model(o.height, o.role, character_seed(o.identity), o.lod);
    else profile = make_character_profile(o.height, o.role, o.identity, o.lod);
    auto& model = profile.model;
    // Generic/legacy meshes have full-range UVs, not character-atlas regions.
    // Match their production presentation material exactly; an atlas here
    // would alter the comparison baseline rather than reveal the old asset.
    Material material;
    material.albedo = {1.f, 1.f, 1.f};
    material.roughness = .72f;
    material.metallic = 0.f;
    material.texture = TextureSlot::None;
    if (profile.individualized && !o.vertex_material) material = profile.material;
    if (o.clay) {
      // Material albedo alone cannot suppress the mesh's vertex color tint.
      // Change only this diagnostic copy, retaining the actual geometry/rig.
      for (auto& vertex : model.bind_mesh.vertices) vertex.color = {1.f, 1.f, 1.f};
      material = Material{};
      material.albedo = {.52f, .52f, .52f};
      material.roughness = .8f;
    }

    const bool preserve_cutout = (o.clay || o.vertex_material) &&
      profile.material.alpha_cutoff >= 0.f && profile.material.textures &&
      profile.material.textures->base_color.valid();
    if (preserve_cutout) {
      // Neutral diagnostics must keep production visibility. Removing a hair
      // mask would turn every strand card into an opaque rectangle and confound
      // a material-versus-geometry comparison.
      auto visibility = std::make_shared<MaterialTextures>();
      visibility->base_color = profile.material.textures->base_color;
      visibility->base_color_mips = profile.material.textures->base_color_mips;
      auto neutralize = [](RgbaImage& image) {
        for (std::size_t i = 0; i < image.pixels.size(); i += 4)
          image.pixels[i] = image.pixels[i+1] = image.pixels[i+2] = 255;
      };
      neutralize(visibility->base_color);
      for (auto& mip : visibility->base_color_mips) neutralize(mip);
      visibility->source = "Diagnostic neutral reflectance with production alpha";
      material.textures = std::move(visibility);
      material.alpha_cutoff = profile.material.alpha_cutoff;
    }

    AppConfig config;
    config.window.title = "Fury Characterlab - Diagnostic Engine Stage";
    config.window.width = static_cast<int>(o.width);
    config.window.height = static_cast<int>(o.image_height);
    config.preferred_backend = o.backend;
    config.prefer_opengl = o.backend == RenderBackendKind::OpenGL;
    config.capture_mouse = false;
    config.enable_collision = false;
    config.cull_distance = 0.f;
    config.log_fps = false;
    config.show_hud = false;
    config.clear_color = {107, 110, 114, 255};
    config.max_frames = o.frames;
    config.fixed_timestep = o.frames ? 1.f / o.fps : 0.f;
    config.freeze_render_time = true;
    config.capture_path = o.capture;
    config.capture_sequence_directory = o.sequence;
    Application app(std::move(config));
    if (!app.init()) return EXIT_FAILURE;
    const bool ray_renderer = app.renderer().backend_kind() == RenderBackendKind::CpuRayTracing;
    if (ray_renderer) {
      auto settings = app.renderer().settings();
      settings.samples_per_pixel = o.spp;
      settings.max_bounces = o.bounces;
      if (!app.renderer().configure(settings))
        throw std::runtime_error("CPU ray renderer rejected sampling settings");
    }

    Lighting light;
    light.sun_direction = normalize({-.55f, -1.f, -.8f});
    light.sun_color = {1.f, 1.f, 1.f};
    light.sun_intensity = 1.8f;
    light.ambient = {.24f, .24f, .24f};
    light.fog_color = {107.f/255.f, 110.f/255.f, 114.f/255.f};
    light.fog_start = 1e6f;
    light.fog_end = 2e6f;
    light.point_light_count = 0;
    light.enable_bloom = false;
    light.enable_reflections = false;
    light.ao_strength = .35f;
    light.shadow_strength = .6f;
    app.renderer().set_lighting(light);

    auto* actor_mesh = app.scene().add_mesh(model.bind_mesh);
    Entity actor;
    actor.name = o.identity;
    actor.mesh = actor_mesh;
    actor.material = material;
    actor.transform.position = {0.f, o.height * .5f, 0.f};
    app.scene().add_entity(std::move(actor));
    Entity ground;
    ground.name = "Diagnostic neutral ground";
    ground.mesh = app.scene().add_mesh(make_plane(o.height * 20.f, o.height * 20.f, {1.f, 1.f, 1.f}));
    ground.material.albedo = {.23f, .23f, .23f};
    ground.material.roughness = .95f;
    app.scene().add_entity(std::move(ground));

    const float aspect = float(o.width) / float(o.image_height);
    const float distance = o.distance > 0.f ? o.distance :
      o.height * (o.face ? .46f : 1.85f) * std::max(1.f, .6f / aspect);
    const Vec3 target{0.f, o.height * (o.face ? .925f : .5f), 0.f};
    const float yaw = radians(o.yaw), elevation = radians(o.elevation);
    const Vec3 eye = target + Vec3{std::sin(yaw)*std::cos(elevation), std::sin(elevation),
                                  std::cos(yaw)*std::cos(elevation)} * distance;
    auto fixed_camera = [&]() {
      auto& camera = app.camera();
      camera.position = eye;
      const Vec3 direction = normalize(target - eye);
      camera.yaw = std::atan2(direction.z, direction.x);
      camera.pitch = std::asin(direction.y);
      camera.snap_look();
      camera.fly_mode = true;
      camera.third_person = false;
      camera.velocity = {};
      camera.fov_y_degrees = o.face ? 30.f : 35.f;
      camera.near_plane = std::max(.00001f, o.height * .003f);
      camera.far_plane = std::max(o.height * 50.f, distance * 2.f);
    };
    fixed_camera();
    const bool animated = o.pose != "bind" && (o.animate || !o.sequence.empty() || !o.frames);
    unsigned submitted_frames = 0;
    double time = 0.0, sampled_time = 0.0;
    app.on_update = [&](float dt, const InputState&) {
      fixed_camera();
      sampled_time = animated ? time : 0.0;
      const auto pose = o.pose == "bind" ? CharacterPose{} :
        sample_character_animation(model.rig, read_state());
      apply_character_pose(model, pose, *actor_mesh);
      ++submitted_frames;
      time += o.frames ? 1.0 / o.fps : double(dt);
    };

    std::cout << "Diagnostic engine stage only; not game-world proof. Actor faces +Z.\n";
    const auto start = std::chrono::steady_clock::now();
    int result = app.run();
    if (o.frames && submitted_frames != o.frames) result = EXIT_FAILURE;
    const double wall_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const auto stats = app.renderer().statistics();
    const auto settings = app.renderer().settings();
    Vec3 low{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec3 high{-low.x, -low.y, -low.z};
    for (const auto& v : actor_mesh->vertices) {
      const Vec3 p = v.position + Vec3{0.f, o.height * .5f, 0.f};
      low = {std::min(low.x,p.x), std::min(low.y,p.y), std::min(low.z,p.z)};
      high = {std::max(high.x,p.x), std::max(high.y,p.y), std::max(high.z,p.z)};
    }
    const auto sample = animation_sample(o, model, sampled_time);
    std::ostringstream report;
    report << std::setprecision(9) << "{\n  \"capture_kind\": \"diagnostic_exact_production_state_replay\",\n"
      "  \"game_world_proof\": false,\n  \"source_sha256\": " << json_string(build_source_fingerprint())
      << ",\n  \"backend\": " << json_string(app.renderer().backend_name())
      << ",\n  \"trace_mode\": " << json_string(ray_renderer ?
          (settings.trace_mode == TraceMode::PathTraced ? "path" : "ray") : "raster")
      << ",\n  \"spp\": " << (ray_renderer ? std::to_string(settings.samples_per_pixel) : "null")
      << ",\n  \"bounces\": " << (ray_renderer ? std::to_string(settings.max_bounces) : "null")
      << ",\n  \"identity\": " << json_string(o.identity)
      << ",\n  \"role\": " << json_string(character_role_name(o.role))
      << ",\n  \"seed\": " << model.seed
      << ",\n  \"individualized\": " << (profile.individualized ? "true" : "false")
      << ",\n  \"legacy_profile\": " << (o.legacy ? "true" : "false")
      << ",\n  \"glasses_removed_diagnostic\": " << (o.cass_no_glasses ? "true" : "false")
      << ",\n  \"material_mode\": " << json_string(o.clay ? "clay_geometry_only" :
          o.vertex_material ? "vertex_material_reference" : profile.individualized ? "character_surface" : "legacy_vertex_color")
      << ",\n  \"material_roughness\": " << material.roughness
      << ",\n  \"material_metallic\": " << material.metallic
      << ",\n  \"material_has_texture_maps\": " << (material.textures ? "true" : "false")
      << ",\n  \"diagnostic_alpha_mask_preserved\": " << (preserve_cutout ? "true" : "false")
      << ",\n  \"lod\": " << json_string(o.lod == CharacterLod::Near ? "near" : "far")
      << ",\n  \"profile_lod_distance\": " << profile.lod_distance
      << ",\n  \"height_meters\": " << o.height
      << ",\n  \"vertices\": " << actor_mesh->vertices.size()
      << ",\n  \"triangles\": " << actor_mesh->indices.size()/3
      << ",\n  \"influences\": " << model.influences.size()
      << ",\n  \"pose\": " << json_string(o.pose)
      << ",\n  \"idle_time_seconds\": " << sample.idle_time
      << ",\n  \"idle_time_overridden\": " << (o.idle_time_override>=0?"true":"false")
      << ",\n  \"speed_meters_per_second\": " << sample.speed
      << ",\n  \"speed_overridden\": " << (o.speed_override>=0?"true":"false")
      << ",\n  \"attention_yaw_radians\": " << o.attention_yaw
      << ",\n  \"attention_pitch_radians\": " << o.attention_pitch
      << ",\n  \"attention_weight\": " << ((o.attention_yaw != 0.f || o.attention_pitch != 0.f) ? 1 : 0)
      << ",\n  \"phase_cycles_initial\": " << o.phase
      << ",\n  \"phase_cycles_final\": " << sample.phase_cycles
      << ",\n  \"animated\": " << (animated ? "true" : "false")
      << ",\n  \"final_sample_seconds\": " << sampled_time
      << ",\n  \"simulated_fps\": " << o.fps
      << ",\n  \"frames_requested\": " << o.frames
      << ",\n  \"frames_submitted\": " << submitted_frames
      << ",\n  \"image_width\": " << app.window().width()
      << ",\n  \"image_height\": " << app.window().height()
      << ",\n  \"face_framing\": " << (o.face ? "true" : "false")
      << ",\n  \"camera_yaw_degrees\": " << o.yaw
      << ",\n  \"camera_elevation_degrees\": " << o.elevation
      << ",\n  \"camera_distance_meters\": " << distance
      << ",\n  \"camera_position\": "; json_vec(report, eye);
    report << ",\n  \"camera_target\": "; json_vec(report, target);
    report << ",\n  \"sun_direction\": "; json_vec(report, light.sun_direction);
    report << ",\n  \"posed_world_bounds_min\": "; json_vec(report, low);
    report << ",\n  \"posed_world_bounds_max\": "; json_vec(report, high);
    report << ",\n  \"wall_seconds\": " << wall_seconds
      << ",\n  \"last_cpu_frame_ms\": " << stats.cpu_frame_ms
      << ",\n  \"software_ray_tracing\": " << (stats.software_ray_tracing ? "true" : "false")
      << ",\n  \"accumulated_frames\": " << stats.accumulated_frames
      << ",\n  \"validation_errors\": " << stats.validation_errors
      << ",\n  \"exit_code\": " << result << "\n}\n";
    std::cout << report.str();
    if (!o.report.empty()) {
      const auto parent = std::filesystem::path(o.report).parent_path();
      if (!parent.empty()) std::filesystem::create_directories(parent);
      std::ofstream file(o.report);
      file << report.str();
      file.close();
      if (!file) throw std::runtime_error("Writing report failed: " + o.report);
    }
    return result;
  } catch (const std::exception& error) {
    std::cerr << "Fury Characterlab: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
