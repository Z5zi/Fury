#include "fury/character_animation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using namespace fury;
void require(bool condition, const std::string& description) {
  if (!condition) throw std::runtime_error(description);
}
Vec3 joint(const CharacterRig& rig, const CharacterPose& pose, CharacterBone bone) {
  const auto i = bone_index(bone);
  return transform_point(pose.skin_matrices[i], rig.joints[i].bind_position);
}
float maximum_difference(const CharacterPose& a, const CharacterPose& b) {
  float result = 0.f;
  for (std::size_t i = 0; i < character_bone_count; ++i)
    for (int j = 0; j < 16; ++j)
      result = (std::max)(result, std::fabs(a.skin_matrices[i].m[j] - b.skin_matrices[i].m[j]));
  return result;
}
void validate_pose(const CharacterRig& rig, const CharacterPose& pose) {
  for (std::size_t i = 0; i < character_bone_count; ++i) {
    const auto& m = pose.skin_matrices[i];
    for (float value : m.m) require(std::isfinite(value), "Every skin-matrix entry is finite");
    const Vec3 x{m.m[0], m.m[1], m.m[2]}, y{m.m[4], m.m[5], m.m[6]}, z{m.m[8], m.m[9], m.m[10]};
    require(std::fabs(length(x) - 1.f) < .0002f && std::fabs(length(y) - 1.f) < .0002f &&
            std::fabs(length(z) - 1.f) < .0002f, "Rigid bones never scale or crush anatomy");
    require(std::fabs(dot(x, y)) < .0002f && std::fabs(dot(y, z)) < .0002f &&
            std::fabs(dot(z, x)) < .0002f && dot(cross(x, y), z) > .9997f,
            "Bone rotations stay orthogonal with positive determinant");
    const int parent = rig.joints[i].parent;
    if (parent >= 0) {
      const Vec3 child = transform_point(m, rig.joints[i].bind_position);
      const Vec3 parent_position = transform_point(pose.skin_matrices[parent], rig.joints[parent].bind_position);
      const float bind_length = length(rig.joints[i].bind_position - rig.joints[parent].bind_position);
      require(std::fabs(length(child - parent_position) - bind_length) < rig.height * .0001f,
              "Every anatomical joint retains its original parent distance");
      const Vec3 connected = transform_point(pose.skin_matrices[parent], rig.joints[i].bind_position);
      require(length(child - connected) < rig.height * .0001f,
              "Parent bone ends remain connected to child bone origins");
    }
  }
}
void validate_ground(const CharacterModel& model, const CharacterPose& pose) {
  const float ground = -.5f * model.rig.height;
  for (std::size_t i = 0; i < model.bind_mesh.vertices.size(); ++i) {
    const auto& influence = model.influences[i];
    if (influence.first != bone_index(CharacterBone::LeftFoot) &&
        influence.first != bone_index(CharacterBone::RightFoot)) continue;
    const auto& source = model.bind_mesh.vertices[i].position;
    const Vec3 point = transform_point(pose.skin_matrices[influence.first], source);
    require(point.y >= ground - model.rig.height * .0001f, "Actual shoe vertices never penetrate level ground");
  }
}
void test_full_gaits() {
  std::size_t poses = 0;
  float maximum_clearance = 0.f;
  for (float height : {.9f, 1.7f, 1.95f, 2.2f}) {
    const auto model = make_character_model(height, CharacterRole::Commuter, 341);
    for (float speed : {.25f, 1.f, 2.2f, 3.4f, 6.5f}) {
      for (float crouch : {0.f, .5f, 1.f}) {
        CharacterAnimationSample sample;
        sample.move_weight = 1.f; sample.speed = speed; sample.crouch_weight = crouch;
        sample.seed = model.seed;
        const float duty = character_stance_fraction(height, speed, crouch);
        for (int step = 0; step <= 240; ++step) {
          sample.phase_cycles = step / 240.; sample.idle_time = 9. + step / 240.;
          const auto pose = sample_character_animation(model.rig, sample);
          validate_pose(model.rig, pose); validate_ground(model, pose);
          for (bool left : {true, false}) {
            const auto foot = left ? CharacterBone::LeftFoot : CharacterBone::RightFoot;
            const Vec3 ankle = joint(model.rig, pose, foot);
            const float phase = static_cast<float>(std::fmod(sample.phase_cycles + (left ? 0. : .5), 1.));
            if (phase < duty) {
              require(std::fabs(ankle.y - model.rig.joints[bone_index(foot)].bind_position.y) < height * .0001f,
                      "Stance ankle remains at its original ground height over a complete gait");
              const Vec3 up = transform_direction(pose.skin_matrices[bone_index(foot)], {0, 1, 0});
              require(length(up - Vec3{0, 1, 0}) < .0001f, "Support ankle counter-rotates to a level sole");
            } else {
              maximum_clearance = (std::max)(maximum_clearance,
                  ankle.y - model.rig.joints[bone_index(foot)].bind_position.y);
            }
            const Vec3 hip = joint(model.rig, pose, left ? CharacterBone::LeftThigh : CharacterBone::RightThigh);
            const Vec3 knee = joint(model.rig, pose, left ? CharacterBone::LeftShin : CharacterBone::RightShin);
            const Vec3 chord = ankle - hip;
            const Vec3 projection = hip + chord * (dot(knee - hip, chord) / dot(chord, chord));
            require(knee.z >= projection.z - height * .00001f, "Knees always bend anatomically forward");
          }
          ++poses;
        }
      }
    }
  }
  require(maximum_clearance > .1f, "Running swing visibly clears the ground");
  std::cout << "Validated " << poses << " grounded articulated gait poses\n";
}
void test_stance_travel() {
  const auto model = make_character_model(1.8f, CharacterRole::Security, 291);
  for (float speed : {.4f, 1.2f, 2.2f, 3.4f, 6.5f}) {
    CharacterAnimationSample sample;
    sample.move_weight = 1.f; sample.speed = speed;
    sample.stride_length = character_stride_length(model.rig.height, speed);
    const float duty = character_stance_fraction(model.rig.height, speed);
    for (double phase : {.05, .15, .30}) {
      const float travel = sample.stride_length * .01f;
      sample.phase_cycles = phase;
      const auto a = sample_character_animation(model.rig, sample);
      sample.phase_cycles += static_cast<double>(travel) / sample.stride_length;
      require(sample.phase_cycles < duty, "Travel test remains in support");
      const auto b = sample_character_animation(model.rig, sample);
      const Vec3 world_a = joint(model.rig, a, CharacterBone::LeftFoot);
      const Vec3 world_b = joint(model.rig, b, CharacterBone::LeftFoot) + Vec3{0, 0, travel};
      require(length(world_a - world_b) < .00001f,
              "A steady planted foot cancels actor travel instead of skating at a time frequency");
    }
  }
}
void test_continuity() {
  const auto model = make_character_model(1.8f, CharacterRole::Fence, 500);
  for (float speed : {1.f, 2.2f, 6.5f}) {
    CharacterAnimationSample sample;
    sample.move_weight = 1.f; sample.speed = speed; sample.idle_time = 10.;
    const float duty = character_stance_fraction(1.8f, speed);
    for (double boundary : {0., .5, static_cast<double>(duty), std::fmod(duty + .5, 1.)}) {
      sample.phase_cycles = boundary - 1.e-6;
      const auto before = sample_character_animation(model.rig, sample);
      sample.phase_cycles = boundary + 1.e-6;
      const auto after = sample_character_animation(model.rig, sample);
      require(maximum_difference(before, after) < .0015f, "Toe-off, touchdown and phase wrapping are continuous");
    }
  }
  CharacterAnimationState state;
  state.sample.seed = 881;
  CharacterAnimationInput input;
  input.delta_time = 1.f / 120.f;
  CharacterPose previous = sample_character_animation(model.rig, state);
  for (int i = 0; i < 960; ++i) {
    const bool active = i >= 180 && i < 700;
    input.talk_weight = active ? 1.f : 0.f;
    input.attention_weight = active ? 1.f : 0.f;
    input.attention_yaw = active ? .8f : -.4f;
    input.attention_pitch = .25f;
    input.turn_rate = active ? 2.f : -1.f;
    input.crouch_weight = i >= 400 && i < 650 ? 1.f : 0.f;
    advance_character_animation(state, input, model.rig.height);
    const auto pose = sample_character_animation(model.rig, state);
    require(maximum_difference(previous, pose) < .11f, "Idle/talk/attention/crouch transitions do not snap: frame " + std::to_string(i) + " delta " + std::to_string(maximum_difference(previous, pose)));
    validate_pose(model.rig, pose); validate_ground(model, pose);
    previous = pose;
  }
}
void test_state_and_determinism() {
  const auto model = make_character_model(1.72f, CharacterRole::CrewScout, 1919);
  CharacterAnimationState a, b;
  a.sample.seed = b.sample.seed = 456;
  CharacterAnimationInput input;
  input.delta_time = 1.f / 60.f;
  input.travel_speed = 2.2f; input.move_weight = 1.f;
  input.yaw = 3.13f;
  for (int step = 0; step < 600; ++step) {
    input.distance_delta = step < 300 ? input.travel_speed * input.delta_time : 0.f;
    input.travel_speed = step < 300 ? 2.2f : 0.f;
    input.move_weight = step < 300 ? 1.f : 0.f;
    input.yaw = step < 100 ? 3.13f : -3.13f;
    input.talk_weight = step > 400 ? 1.f : 0.f;
    const double old_phase = a.sample.phase_cycles;
    advance_character_animation(a, input, model.rig.height);
    advance_character_animation(b, input, model.rig.height);
    if (input.distance_delta == 0.f) require(a.sample.phase_cycles == old_phase, "No travel means no gait phase advance");
    const auto pa = sample_character_animation(model.rig, a), pb = sample_character_animation(model.rig, b);
    require(std::memcmp(pa.skin_matrices.data(), pb.skin_matrices.data(), sizeof(pa.skin_matrices)) == 0,
            "Identical input histories produce bit-identical poses");
    require(std::fabs(a.sample.turn_rate) < .2f, "Wrapping yaw at pi uses the short turn");
  }
  CharacterAnimationSample idle;
  idle.seed = 5; idle.idle_time = 3.;
  const auto first = sample_character_animation(model.rig, idle);
  idle.seed = 93002;
  require(maximum_difference(first, sample_character_animation(model.rig, idle)) > .003f,
          "Stable actor seeds vary breathing, weight shift and head motion");
  idle.seed = 5; idle.idle_time = 3. + 1. / 60.;
  require(maximum_difference(first, sample_character_animation(model.rig, idle)) < .003f, "Idle motion stays restrained frame to frame");
}
void test_crouch() {
  const auto model = make_character_model(1.8f, CharacterRole::Player, 9191);
  CharacterAnimationSample sample;
  const auto standing = sample_character_animation(model.rig, sample);
  sample.crouch_weight = 1.f;
  const auto crouched = sample_character_animation(model.rig, sample);
  validate_pose(model.rig, crouched); validate_ground(model, crouched);
  require(joint(model.rig, standing, CharacterBone::Head).y -
          joint(model.rig, crouched, CharacterBone::Head).y > .45f,
          "Crouch lowers the head by bending the body rather than rescaling it");
  for (auto foot : {CharacterBone::LeftFoot, CharacterBone::RightFoot})
    require(length(joint(model.rig, standing, foot) - joint(model.rig, crouched, foot)) < .0001f,
            "Crouching in place keeps both feet grounded");
}


void test_stopping_support() {
  const auto model = make_character_model(1.8f, CharacterRole::CrewTech, 762);
  for (float speed : {1.2f, 2.2f, 6.5f}) {
    for (int phase = 0; phase < 20; ++phase) {
      CharacterAnimationState state;
      state.initialized = state.gait_started = true;
      state.sample.speed = speed; state.sample.move_weight = 1.f;
      state.sample.stride_length = character_stride_length(1.8f, speed);
      state.sample.phase_cycles = phase / 20.;
      state.support_fraction = character_stance_fraction(1.8f, speed);
      const auto before = sample_character_animation(model.rig, state);
      CharacterAnimationInput input;
      input.delta_time = 1.f / 60.f; input.turn_rate = 0.f;
      for (int step = 0; step < 100; ++step) {
        advance_character_animation(state, input, 1.8f);
        const auto pose = sample_character_animation(model.rig, state);
        validate_pose(model.rig, pose); validate_ground(model, pose);
        for (auto foot : {CharacterBone::LeftFoot, CharacterBone::RightFoot}) {
          const auto a = joint(model.rig, before, foot), b = joint(model.rig, pose, foot);
          require(std::fabs(a.x - b.x) + std::fabs(a.z - b.z) < .0001f,
                  "Stopping never drags planted feet back toward a neutral pose");
        }
      }
    }
  }
}

void test_skin_update_and_distance_clock() {
  const auto model = make_character_model(1.8f, CharacterRole::Dock, 901);
  Mesh output = model.bind_mesh;
  const auto* vertices = output.vertices.data();
  const auto* indices = output.indices.data();
  const auto original_indices = output.indices;
  const auto identity = output.geometry_identity;
  output.gpu_vao = 71; output.gpu_vbo = 72; output.gpu_ibo = 73;
  output.gpu_uploaded = true;
  CharacterAnimationSample sample;
  sample.speed = 2.f; sample.move_weight = 1.f;
  for (int step = 0; step < 120; ++step) {
    sample.phase_cycles = step / 120.; sample.idle_time = step / 60.;
    const auto pose = sample_character_animation(model.rig, sample);
    require(apply_character_pose(model, pose, output), "Moving poses update skin vertices");
    require(!apply_character_pose(model, pose, output), "An unchanged sampled pose does not dirty geometry twice");
    require(output.vertices.data() == vertices && output.indices.data() == indices &&
            output.indices == original_indices, "Skin updates neither allocate nor rebuild topology");
    require(output.geometry_identity == identity && output.gpu_vao == 71 && output.gpu_vbo == 72 &&
            output.gpu_ibo == 73 && output.gpu_uploaded, "Animation preserves geometry identity and GPU handles");
    for (const auto& vertex : output.vertices)
      require(std::fabs(length(vertex.normal) - 1.f) < .0002f, "Animated surface normals stay normalized");
  }
  CharacterAnimationState fast_tick, slow_tick;
  CharacterAnimationInput input;
  input.travel_speed = 2.f; input.move_weight = 1.f; input.distance_delta = .031f;
  input.delta_time = 1.f / 120.f;
  advance_character_animation(fast_tick, input, 1.8f);
  input.delta_time = 1.f / 20.f;
  advance_character_animation(slow_tick, input, 1.8f);
  require(fast_tick.sample.phase_cycles == slow_tick.sample.phase_cycles,
          "The same measured travel advances the same phase regardless of timer frequency");
}

void test_extreme_inputs() {
  const auto model = make_character_model(1.8f, CharacterRole::Player, 810);
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float inf = std::numeric_limits<float>::infinity();
  CharacterAnimationInput input;
  for (float bad : {nan, inf, -inf, -1000.f, 1.e30f}) {
    CharacterAnimationState state;
    state.sample.speed = bad; state.sample.idle_time = bad; state.sample.phase_cycles = bad;
    state.sample.stride_length = bad; state.sample.move_weight = bad;
    input.delta_time = 1.f / 60.f; input.distance_delta = bad; input.travel_speed = bad;
    input.move_weight = bad; input.yaw = bad; input.turn_rate = bad;
    input.attention_yaw = input.attention_pitch = input.attention_weight = bad;
    input.talk_weight = input.crouch_weight = bad;
    advance_character_animation(state, input, 1.8f);
    validate_pose(model.rig, sample_character_animation(model.rig, state));
    CharacterAnimationSample direct;
    direct.phase_cycles = direct.idle_time = bad;
    direct.speed = direct.stride_length = direct.move_weight = bad;
    direct.turn_rate = direct.attention_yaw = direct.attention_pitch = direct.attention_weight = bad;
    direct.talk_weight = direct.crouch_weight = bad;
    validate_pose(model.rig, sample_character_animation(model.rig, direct));
  }
  CharacterAnimationState state;
  input = CharacterAnimationInput{};
  for (float bad_dt : {nan, inf, -inf, -1.f, 0.f}) {
    input.delta_time = bad_dt; input.distance_delta = 20.f;
    advance_character_animation(state, input, 1.8f);
    require(!state.initialized && state.sample.phase_cycles == 0. && state.sample.idle_time == 0.,
            "Invalid or nonpositive delta time freezes the state");
  }
  for (float height : {nan, inf, -1.f, 0.f}) {
    CharacterRig bad = model.rig; bad.height = height;
    require(maximum_difference(sample_character_animation(bad, CharacterAnimationSample{}), CharacterPose{}) == 0.f,
            "An invalid rig returns a safe bind pose");
  }
}
}  // namespace

int main() {
  try {
    test_full_gaits(); test_stance_travel(); test_continuity();
    test_state_and_determinism(); test_crouch(); test_stopping_support();
    test_skin_update_and_distance_clock(); test_extreme_inputs();
    std::cout << "Character animation tests passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Character animation test failure: " << e.what() << '\n';
    return 1;
  }
}
