#include "fury/character_animation.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace fury { namespace animation_detail { namespace generic {
namespace {
constexpr double kTau = 6.283185307179586476925286766559;
constexpr float kPi = 3.14159265358979323846f;

float finite(float value, float fallback = 0.f) {
  return std::isfinite(value) ? value : fallback;
}
float clamp(float value, float lo, float hi) {
  return (std::max)(lo, (std::min)(hi, finite(value, lo)));
}
float saturate(float value) { return clamp(value, 0.f, 1.f); }
float mix(float a, float b, float t) { return a + (b - a) * t; }
float smooth(float a, float b, float value) {
  const float t = saturate((value - a) / (b - a));
  return t * t * (3.f - 2.f * t);
}
float smooth5(float t) {
  return t * t * t * (t * (t * 6.f - 15.f) + 10.f);
}
float wrap_angle(float value) {
  return static_cast<float>(std::remainder(static_cast<double>(finite(value)), kTau));
}
double wrap_cycle(double value) {
  if (!std::isfinite(value)) return 0.;
  value = std::fmod(value, 1.);
  return value < 0. ? value + 1. : value;
}
float oscillate(double time, double frequency, double phase = 0.) {
  // Bound unusual stateless timestamps before multiplying, without changing
  // any normal game-session clock. All phases are evaluated in double precision.
  if (!std::isfinite(time)) time = 0.;
  time = (std::max)(-1.e12, (std::min)(1.e12, time));
  return static_cast<float>(std::sin(std::remainder(time * frequency + phase, kTau)));
}
float body_height(float height) {
  return clamp(finite(height, 1.75f), .001f, 1000.f);
}
float run_blend(float height, float speed, float crouch) {
  return smooth(1.35f, 2.45f, speed / height) * (1.f - .8f * crouch);
}
float filter(float previous, float target, float response, float dt) {
  return mix(finite(previous, target), target, -std::expm1(-response * dt));
}
Vec3 safe_normalize(Vec3 value, Vec3 fallback) {
  const float d2 = dot(value, value);
  return std::isfinite(d2) && d2 > 1.e-14f ? value * (1.f / std::sqrt(d2)) : fallback;
}
bool valid_rig(const CharacterRig& rig) {
  if (!std::isfinite(rig.height) || rig.height < .001f || rig.height > 1000.f) return false;
  for (std::size_t i = 0; i < character_bone_count; ++i) {
    const auto& joint = rig.joints[i];
    if (!std::isfinite(joint.bind_position.x) || !std::isfinite(joint.bind_position.y) ||
        !std::isfinite(joint.bind_position.z) || joint.parent >= static_cast<int>(i) ||
        joint.parent < -1) return false;
    if (length(joint.bind_position) > rig.height * 4.f) return false;
  }
  for (CharacterBone joint : {CharacterBone::LeftShin, CharacterBone::LeftFoot,
                               CharacterBone::RightShin, CharacterBone::RightFoot}) {
    const auto& j = rig.joints[bone_index(joint)];
    if (j.parent < 0 || length(j.bind_position - rig.joints[j.parent].bind_position) < rig.height * .001f)
      return false;
  }
  return true;
}

// Shortest proper rotation between two vectors; no scale, shear, or allocation.
Mat4 align_vector(Vec3 from, Vec3 to) {
  from = safe_normalize(from, {0.f, -1.f, 0.f});
  to = safe_normalize(to, from);
  const float c = clamp(dot(from, to), -1.f, 1.f);
  Mat4 result = Mat4::identity();
  if (c > .9999999f) return result;
  if (c < -.999999f) {
    Vec3 axis = safe_normalize(cross(from, {1.f, 0.f, 0.f}), {0.f, 0.f, 1.f});
    const float a[3] = {axis.x, axis.y, axis.z};
    for (int col = 0; col < 3; ++col)
      for (int row = 0; row < 3; ++row)
        result.at(col, row) = 2.f * a[row] * a[col] - (row == col ? 1.f : 0.f);
    return result;
  }
  const Vec3 axis = cross(from, to);
  const float v[3] = {axis.x, axis.y, axis.z};
  const float skew[3][3] = {{0.f, -axis.z, axis.y},
                           {axis.z, 0.f, -axis.x},
                           {-axis.y, axis.x, 0.f}};
  const float factor = 1.f / (1.f + c);
  for (int col = 0; col < 3; ++col)
    for (int row = 0; row < 3; ++row)
      result.at(col, row) = c * (row == col ? 1.f : 0.f) +
                             skew[row][col] + v[row] * v[col] * factor;
  return result;
}
Mat4 skin_matrix(Vec3 bind_origin, Vec3 pose_origin, const Mat4& rotation) {
  return translate(pose_origin) * rotation * translate(-bind_origin);
}
struct Foot {
  Vec3 ankle;
  float pitch{0.f};
};
Foot foot_offset(float height, const CharacterAnimationSample& sample,
                 bool left, float stride, float duty, float moving, float running,
                 float crouch, float spacing) {
  const float cycle = static_cast<float>(wrap_cycle(sample.phase_cycles + (left ? 0. : .5)));
  float z = stride * (duty * .5f - cycle);
  float lift = 0.f;
  float pitch = 0.f;
  if (cycle > duty) {
    const float u = (cycle - duty) / (1.f - duty);
    // Matches stance dz/dphase = -stride at BOTH ends. Quintic recovery
    // avoids a toe-off velocity reversal/jump and has continuous acceleration.
    z += stride * smooth5(u);
    const float sine = std::sin(kPi * u);
    const float arc = sine * sine;
    lift = height * mix(.047f, .092f, running) * (1.f - .25f * crouch) * arc;
    // The unloaded foot follows the recovering shin: plantarflex just after
    // toe-off, pass through neutral at midswing, then prepare a level landing.
    // The squared-sine envelope has zero slope at both support boundaries.
    // Support itself retains its level-sole contract.
    pitch = .65f * running * arc * std::cos(kPi * u) -
            .11f * (1.f - running) * arc;
  }
  Foot result;
  result.ankle = Vec3{0.f, lift * moving, z * spacing};
  result.pitch = pitch * moving;
  return result;
}
Foot foot_target(const CharacterRig& rig, const CharacterAnimationSample& sample,
                 bool left, float stride, float duty, float moving, float running,
                 float crouch, float spacing) {
  auto result = foot_offset(rig.height, sample, left, stride, duty, moving, running, crouch, spacing);
  const auto bone = left ? CharacterBone::LeftFoot : CharacterBone::RightFoot;
  result.ankle += rig.joints[bone_index(bone)].bind_position;
  return result;
}

void solve_leg(const CharacterRig& rig, CharacterPose& pose, bool left, const Foot& target) {
  const auto thigh = bone_index(left ? CharacterBone::LeftThigh : CharacterBone::RightThigh);
  const auto shin = bone_index(left ? CharacterBone::LeftShin : CharacterBone::RightShin);
  const auto foot = bone_index(left ? CharacterBone::LeftFoot : CharacterBone::RightFoot);
  const Vec3 bind_hip = rig.joints[thigh].bind_position;
  const Vec3 bind_knee = rig.joints[shin].bind_position;
  const Vec3 bind_ankle = rig.joints[foot].bind_position;
  const float upper = length(bind_knee - bind_hip);
  const float lower = length(bind_ankle - bind_knee);
  const Vec3 hip = transform_point(pose.skin_matrices[bone_index(CharacterBone::Pelvis)], bind_hip);
  const Vec3 chord = target.ankle - hip;
  const float requested = length(chord);
  const float reach = clamp(requested, std::fabs(upper - lower) + rig.height * .00001f,
                           upper + lower - rig.height * .00001f);
  const Vec3 direction = safe_normalize(chord, {0.f, -1.f, 0.f});
  const Vec3 ankle = hip + direction * reach;
  const float along = (upper * upper - lower * lower + reach * reach) / (2.f * reach);
  const float bend = std::sqrt((std::max)(0.f, upper * upper - along * along));
  // The anatomical knee bends toward +Z; it never flips with small turns.
  const Vec3 knee_forward = safe_normalize(Vec3{0.f, 0.f, 1.f} - direction * direction.z,
                                         {0.f, 1.f, 0.f});
  const Vec3 knee = hip + direction * along + knee_forward * bend;
  pose.skin_matrices[thigh] = skin_matrix(bind_hip, hip, align_vector(bind_knee - bind_hip, knee - hip));
  pose.skin_matrices[shin] = skin_matrix(bind_knee, knee, align_vector(bind_ankle - bind_knee, ankle - knee));
  // A planted sole stays level as hip/knee motion is absorbed by the ankle.
  pose.skin_matrices[foot] = skin_matrix(bind_ankle, ankle, rotate_x(target.pitch));
}
}  // namespace

float character_stride_length(float height, float speed, float crouch_weight) {
  height = body_height(height);
  speed = clamp(speed, 0.f, 20.f);
  const float crouch = saturate(crouch_weight);
  return height * clamp(.43f + .23f * speed / height, .48f, 1.25f) * (1.f - .48f * crouch);
}
float character_stance_fraction(float height, float speed, float crouch_weight) {
  height = body_height(height);
  return mix(.62f, .47f, run_blend(height, clamp(speed, 0.f, 20.f), saturate(crouch_weight)));
}

void advance_character_animation(CharacterAnimationState& state,
                                 const CharacterAnimationInput& input,
                                 float height) {
  const float dt = clamp(input.delta_time, 0.f, .25f);
  if (dt <= 0.f) return;
  height = body_height(height);
  auto& s = state.sample;
  s.phase_cycles = wrap_cycle(s.phase_cycles);
  const auto previous_sample = s;
  if (!std::isfinite(s.idle_time) || std::fabs(s.idle_time) > 1.e12) s.idle_time = 0.;
  const float speed = clamp(input.travel_speed, 0.f, 20.f);
  const float yaw = wrap_angle(input.yaw);
  const float crouch = saturate(input.crouch_weight);
  if (!state.initialized) {
    state.previous_yaw = yaw;
    s.speed = speed;
    s.stride_length = character_stride_length(height, speed, crouch);
    state.initialized = true;
  }
  const float measured_turn = std::isfinite(input.turn_rate) ? input.turn_rate :
      wrap_angle(yaw - finite(state.previous_yaw, yaw)) / dt;
  state.previous_yaw = yaw;
  s.speed = filter(s.speed, speed, 10.f, dt);
  s.crouch_weight = filter(s.crouch_weight, crouch, 9.f, dt);
  s.move_weight = filter(s.move_weight, saturate(input.move_weight) * smooth(.015f, .12f, speed), 12.f, dt);
  s.turn_rate = filter(s.turn_rate, clamp(measured_turn, -6.f, 6.f), 8.f, dt);
  s.attention_weight = filter(s.attention_weight, saturate(input.attention_weight), 9.f, dt);
  s.attention_yaw = filter(s.attention_yaw, clamp(wrap_angle(input.attention_yaw), -.9f, .9f), 9.f, dt);
  s.attention_pitch = filter(s.attention_pitch, clamp(input.attention_pitch, -.4f, .4f), 9.f, dt);
  s.talk_weight = filter(s.talk_weight, saturate(input.talk_weight), 7.f, dt);
  const float distance = clamp(input.distance_delta, 0.f, (std::max)(height * .75f, 20.f * dt));
  bool interrupted_settle = false;
  unsigned interrupted_support = 0;
  if (state.recovering_step) {
    state.recovery_time += dt;
    if (state.recovery_time >= .20f) state.recovering_step = false;
  }
  if (distance > 0.f && (state.settling_feet || state.settled_feet)) {
    interrupted_settle = state.settling_feet;
    interrupted_support = state.settling_lift[0] <= state.settling_lift[1] ? 0u : 1u;
    state.settling_feet = state.settled_feet = false;
    state.gait_started = false;
  }
  if (distance > 0.f) {
    const float new_stride = character_stride_length(height, s.speed, s.crouch_weight);
    const float new_support = character_stance_fraction(height, s.speed, s.crouch_weight);
    if (!state.gait_started) {
      // Both feet have zero longitudinal offset here: left is mid-support,
      // right is at swing midpoint. Fade in clearance without sliding apart.
      state.support_fraction = new_support;
      s.phase_cycles = state.support_fraction * .5;
      s.stride_length = character_stride_length(height, speed, s.crouch_weight);
      if (interrupted_settle) {
        const float z = state.settling_z[interrupted_support];
        s.stride_length = (std::max)(s.stride_length,
            2.f * std::fabs(z) / (std::max)(.3f, state.support_fraction));
        s.stride_length = (std::min)(s.stride_length, height * 1.25f);
        s.phase_cycles = wrap_cycle(state.support_fraction * .5 - z / s.stride_length -
                                     (interrupted_support == 0 ? 0. : .5));
      }
      state.gait_started = true;
    } else {
      const float old_stride = s.stride_length;
      const float old_support = state.support_fraction;
      const float left_cycle = static_cast<float>(wrap_cycle(s.phase_cycles));
      const float right_cycle = static_cast<float>(wrap_cycle(s.phase_cycles + .5));
      s.stride_length = filter(s.stride_length, new_stride, 7.f, dt);
      state.support_fraction = filter(state.support_fraction, new_support, 7.f, dt);
      // When cadence or support duration changes, reparameterize the active
      // support interval around the same ankle. Otherwise a crouched restart
      // jumps both feet when a retained running duty changes to walking duty.
      const bool left_support = left_cycle < old_support;
      const bool right_support = right_cycle < old_support;
      if (left_support || right_support) {
        const bool anchor_left = left_support && (!right_support || left_cycle < right_cycle);
        const float cycle = anchor_left ? left_cycle : right_cycle;
        const float offset = old_stride * (old_support * .5f - cycle);
        const double anchored_cycle = state.support_fraction * .5 - offset / s.stride_length;
        s.phase_cycles = wrap_cycle(anchored_cycle - (anchor_left ? 0. : .5));
      }
    }
  }
  // Stride is a travel metric, never a free-running walk timer. An immobile
  // talking actor cannot accidentally march because a clock keeps advancing.
  s.phase_cycles = wrap_cycle(s.phase_cycles + static_cast<double>(distance) /
                              (std::max)(height * .05f, s.stride_length));
  const float posture = smooth(0.f, 1.f, s.crouch_weight);
  const float running = run_blend(height, s.speed, posture);
  if (interrupted_settle) {
    state.recovery_time = 0.f;
    state.recovering_step = true;
    state.recovery_foot = 1u - interrupted_support;
    for (unsigned i = 0; i < 2; ++i) {
      const auto target = foot_offset(height, s, i == 0, s.stride_length,
          state.support_fraction, s.move_weight, running, posture, 1.f);
      state.recovery_z[i] = state.settling_z[i] - distance - target.ankle.z;
      state.recovery_lift[i] = state.settling_lift[i] - target.ankle.y;
      state.recovery_pitch[i] = state.settling_pitch[i];
    }
  }
  if (distance == 0.f && s.crouch_weight > .15f && s.move_weight < .15f &&
      state.gait_started && !state.settling_feet && !state.settled_feet &&
      !state.recovering_step) {
    const float old_posture = smooth(0.f, 1.f, previous_sample.crouch_weight);
    const float old_running = run_blend(height, previous_sample.speed, old_posture);
    for (unsigned i = 0; i < 2; ++i) {
      const auto target = foot_offset(height, previous_sample, i == 0,
          previous_sample.stride_length, state.support_fraction,
          previous_sample.move_weight, old_running, old_posture, 1.f);
      state.settling_start_z[i] = state.settling_z[i] = target.ankle.z;
      state.settling_start_lift[i] = state.settling_lift[i] = target.ankle.y;
      state.settling_start_pitch[i] = state.settling_pitch[i] = target.pitch;
    }
    if ((std::max)(std::fabs(state.settling_z[0]), std::fabs(state.settling_z[1])) > height * .025f) {
      state.settling_feet = true;
      state.settling_index = 0;
      state.settling_time = 0.f;
      state.settling_first = state.settling_lift[0] > height * .001f ? 0u :
          state.settling_lift[1] > height * .001f ? 1u :
          std::fabs(state.settling_z[0]) >= std::fabs(state.settling_z[1]) ? 0u : 1u;
    }
  }
  if (state.settling_feet && distance == 0.f) {
    state.settling_time += dt;
    const float u = saturate(state.settling_time / .30f);
    const float blend = smooth5(u);
    const unsigned active = (state.settling_first + state.settling_index) % 2u;
    state.settling_z[active] = mix(state.settling_start_z[active], 0.f, blend);
    const float sine = std::sin(kPi * u);
    state.settling_lift[active] = mix(state.settling_start_lift[active], 0.f, blend) +
                                 height * .035f * sine * sine;
    state.settling_pitch[active] = mix(state.settling_start_pitch[active], 0.f, blend);
    if (state.settling_index == 0) {
      const unsigned support = 1u - active;
      state.settling_lift[support] = mix(state.settling_start_lift[support], 0.f, blend);
      state.settling_pitch[support] = mix(state.settling_start_pitch[support], 0.f, blend);
    }
    if (u >= 1.f) {
      state.settling_z[active] = state.settling_lift[active] = state.settling_pitch[active] = 0.f;
      state.settling_time = 0.f;
      const unsigned next = 1u - active;
      state.settling_start_lift[next] = state.settling_lift[next];
      state.settling_start_pitch[next] = state.settling_pitch[next];
      if (++state.settling_index == 2) {
        state.settling_feet = false;
        state.settled_feet = true;
        state.gait_started = false;
      }
    }
  }
  s.idle_time += dt;
}

static CharacterPose sample_character_animation_impl(const CharacterRig& rig,
                                         const CharacterAnimationSample& input,
                                         float foot_spacing, float support_fraction,
                                         const CharacterAnimationState* state = nullptr) {
  if (!valid_rig(rig)) return CharacterPose{};
  CharacterAnimationSample s = input;
  const float height = rig.height;
  const float moving = saturate(s.move_weight);
  const float crouch = smooth(0.f, 1.f, s.crouch_weight);
  const float speed = clamp(s.speed, 0.f, 20.f);
  const float turning = clamp(s.turn_rate, -6.f, 6.f);
  const float attending = saturate(s.attention_weight);
  const float talking = saturate(s.talk_weight);
  const float running = run_blend(height, speed, crouch);
  const float stride = std::isfinite(s.stride_length) && s.stride_length > 0.f ?
      clamp(s.stride_length, height * .05f, height * 1.3f) : character_stride_length(height, speed, crouch);
  const float duty = support_fraction > 0.f ? clamp(support_fraction, .4f, .7f) :
      character_stance_fraction(height, speed, crouch);
  const float spacing = foot_spacing >= 0.f ? saturate(foot_spacing) : moving;
  s.phase_cycles = wrap_cycle(s.phase_cycles);
  const double phase = s.phase_cycles * kTau;
  const double personality = static_cast<double>(s.seed % 65521u) * (kTau / 65521.);
  const double time = std::isfinite(s.idle_time) ? s.idle_time : 0.;
  const float breathing = oscillate(time, 1.25 + (s.seed % 11u) * .017, personality);
  const float weight_shift = oscillate(time, .47, personality + .4);
  const float gait_wave = oscillate(phase, 1., kPi * .5);
  const float gait_sine = oscillate(phase, 1.);
  const float idle = 1.f - .65f * moving;
  std::array<Vec3, character_bone_count> angles{};
  auto& pelvis = angles[bone_index(CharacterBone::Pelvis)];
  pelvis = {crouch * .09f, moving * .042f * gait_wave,
            moving * .012f * gait_sine + idle * .009f * weight_shift};
  auto& spine = angles[bone_index(CharacterBone::Spine)];
  spine = {crouch * .21f + moving * mix(.023f, .085f, running),
           -.55f * pelvis.y + turning * .009f, -turning * .010f * moving};
  auto& chest = angles[bone_index(CharacterBone::Chest)];
  chest = {.009f * breathing + crouch * .08f,
           -.38f * pelvis.y + attending * clamp(s.attention_yaw, -.9f, .9f) * .16f,
           idle * .004f * oscillate(time, .63, personality)};
  auto& neck = angles[bone_index(CharacterBone::Neck)];
  neck = {-crouch * .10f, turning * .016f, 0.f};
  auto& head = angles[bone_index(CharacterBone::Head)];
  head = {-crouch * .17f + attending * clamp(s.attention_pitch, -.4f, .4f) * .82f +
              idle * .014f * oscillate(time, .73, personality + 1.1),
          attending * clamp(s.attention_yaw, -.9f, .9f) * .72f + turning * .021f +
              idle * .025f * oscillate(time, .37, personality + .8),
          idle * .012f * oscillate(time, .51, personality + 2.1)};

  const float arm_swing = mix(.32f, .64f, running) * moving * (1.f - .35f * crouch);
  const float arm_wave = oscillate(phase, 1., kPi * .38);
  const float gesture_wave = .5f + .5f * oscillate(time, 2.15, personality);
  const float gesture = gesture_wave * gesture_wave;
  const bool left_lead = (s.seed & 1u) != 0u;
  for (bool left : {true, false}) {
    const float side = left ? -1.f : 1.f;
    const float swing = (left ? 1.f : -1.f) * arm_swing * arm_wave;
    const float lead = left == left_lead ? 1.f : .54f;
    const float talk = talking * lead;
    auto& upper = angles[bone_index(left ? CharacterBone::LeftUpperArm : CharacterBone::RightUpperArm)];
    auto& elbow = angles[bone_index(left ? CharacterBone::LeftForearm : CharacterBone::RightForearm)];
    auto& hand = angles[bone_index(left ? CharacterBone::LeftHand : CharacterBone::RightHand)];
    upper = {swing - crouch * .17f - talk * (.21f + .16f * gesture),
             side * talk * .06f * oscillate(time, 1.4, personality),
             side * (.025f + talk * (.12f + .07f * gesture))};
    elbow = {-.12f - moving * mix(.15f, .55f, running) -
                 (std::max)(0.f, -swing) * .2f - talk * (.52f + .23f * gesture),
             0.f, side * .025f};
    hand = {talk * .12f * oscillate(time, 2.15, personality + .55),
            side * talk * .10f, side * talk * .07f * gesture};
  }

  Foot left = foot_target(rig, s, true, stride, duty, moving, running, crouch, spacing);
  Foot right = foot_target(rig, s, false, stride, duty, moving, running, crouch, spacing);
  if (state) {
    for (unsigned i = 0; i < 2; ++i) {
      auto& target = i == 0 ? left : right;
      const auto foot = bone_index(i == 0 ? CharacterBone::LeftFoot : CharacterBone::RightFoot);
      if (state->settling_feet || state->settled_feet) {
        target.ankle = rig.joints[foot].bind_position +
                       Vec3{0.f, state->settling_lift[i], state->settling_z[i]};
        target.pitch = state->settling_pitch[i];
      } else if (state->recovering_step) {
        const float u = saturate(state->recovery_time / .20f);
        const float weight = 1.f - smooth5(u);
        target.ankle.z += state->recovery_z[i] * weight;
        target.ankle.y = (std::max)(rig.joints[foot].bind_position.y,
                                   target.ankle.y + state->recovery_lift[i] * weight);
        target.pitch = mix(state->recovery_pitch[i], target.pitch, 1.f - weight);
        if (i == state->recovery_foot || std::fabs(state->recovery_z[i]) > height * .0001f) {
          const float sine = std::sin(kPi * u);
          target.ankle.y += height * .035f * sine * sine;
        }
        const float clearance = target.ankle.y - rig.joints[foot].bind_position.y;
        target.pitch *= mix(smooth(0.f, height * .06f, clearance), 1.f, 1.f - weight);
      }
    }
    if (state->recovering_step) {
      // The old recovery offset and the new gait can point in opposite
      // directions after an abrupt sprint restart. Bound the *unloaded*
      // recovery reach before IK rather than driving the common pelvis into
      // the ground or stretching an anatomical segment.
      const float separation = std::fabs(left.ankle.z - right.ankle.z);
      const float maximum = height * .60f;
      if (separation > maximum) {
        const bool free_left = left.ankle.y > rig.joints[bone_index(CharacterBone::LeftFoot)].bind_position.y + height * .0001f;
        const bool free_right = right.ankle.y > rig.joints[bone_index(CharacterBone::RightFoot)].bind_position.y + height * .0001f;
        const float sign = left.ankle.z > right.ankle.z ? 1.f : -1.f;
        if (free_left && free_right) {
          const float center = .5f * (left.ankle.z + right.ankle.z);
          left.ankle.z = center + sign * maximum * .5f;
          right.ankle.z = center - sign * maximum * .5f;
        } else if (free_left) left.ankle.z = right.ankle.z + sign * maximum;
        else if (free_right) right.ankle.z = left.ankle.z - sign * maximum;
      }
    }
  }
  const float foot_separation = std::fabs(left.ankle.z - right.ankle.z);
  const float crouch_back = (std::min)(height * .052f + .5f * foot_separation,
                                      (std::max)(0.f, height * .46f - foot_separation));
  // Pelvis sway and knee flexion, not body scaling. Crouch deliberately changes
  // posture at the original actor center/height, so leg/head proportions survive.
  Vec3 root{height * (.0035f * idle * weight_shift + .0045f * moving * gait_sine),
            -height * (.004f + .285f * crouch + .008f * moving),
            .5f * (left.ankle.z + right.ankle.z) - crouch_back * crouch};
  CharacterPose pose = make_character_pose(rig, angles, root);
  // Lower the pelvis only as far as required by the farther foot. This preserves
  // exact ground support across the entire stride without overextending a leg.
  float drop = 0.f;
  for (bool is_left : {true, false}) {
    const auto thigh = bone_index(is_left ? CharacterBone::LeftThigh : CharacterBone::RightThigh);
    const auto shin = bone_index(is_left ? CharacterBone::LeftShin : CharacterBone::RightShin);
    const auto foot = bone_index(is_left ? CharacterBone::LeftFoot : CharacterBone::RightFoot);
    const Vec3 hip = transform_point(pose.skin_matrices[bone_index(CharacterBone::Pelvis)], rig.joints[thigh].bind_position);
    const Vec3 ankle = is_left ? left.ankle : right.ankle;
    const float reach = length(rig.joints[shin].bind_position - rig.joints[thigh].bind_position) +
                        length(rig.joints[foot].bind_position - rig.joints[shin].bind_position) - height * .001f;
    const float dx = ankle.x - hip.x, dz = ankle.z - hip.z;
    const float allowed = std::sqrt((std::max)(0.f, reach * reach - dx * dx - dz * dz));
    const float need = hip.y - ankle.y - allowed;
    const float difference = drop - need;
    const float softness = height * .0025f;
    drop = .5f * (drop + need + std::sqrt(difference * difference + softness * softness));
  }
  if (drop > 0.f) {
    // A common object-space translation leaves all FK rotations unchanged.
    for (auto& matrix : pose.skin_matrices) matrix.at(3, 1) -= drop;
  }
  if (crouch > 0.f) {
    // A staggered squat cannot sink the hip below the leading knee without
    // folding the thigh back through the pelvis and clothing. Keep the feet
    // planted and use the deepest posture that preserves the leg's reach.
    float available = height;
    for (bool is_left : {true, false}) {
      const auto thigh = bone_index(is_left ? CharacterBone::LeftThigh : CharacterBone::RightThigh);
      const auto shin = bone_index(is_left ? CharacterBone::LeftShin : CharacterBone::RightShin);
      const auto foot = bone_index(is_left ? CharacterBone::LeftFoot : CharacterBone::RightFoot);
      const Vec3 hip = transform_point(pose.skin_matrices[bone_index(CharacterBone::Pelvis)], rig.joints[thigh].bind_position);
      const Vec3 ankle = is_left ? left.ankle : right.ankle;
      const float reach = length(rig.joints[shin].bind_position - rig.joints[thigh].bind_position) +
                          length(rig.joints[foot].bind_position - rig.joints[shin].bind_position) - height * .001f;
      const float dx = ankle.x - hip.x, dz = ankle.z - hip.z;
      const float allowed = std::sqrt((std::max)(0.f, reach * reach - dx * dx - dz * dz));
      available = (std::min)(available, ankle.y + allowed - hip.y);
    }
    auto folded = [&](float raise) {
      for (bool is_left : {true, false}) {
        const auto thigh = bone_index(is_left ? CharacterBone::LeftThigh : CharacterBone::RightThigh);
        const auto shin = bone_index(is_left ? CharacterBone::LeftShin : CharacterBone::RightShin);
        const auto foot = bone_index(is_left ? CharacterBone::LeftFoot : CharacterBone::RightFoot);
        const float upper = length(rig.joints[shin].bind_position - rig.joints[thigh].bind_position);
        const float lower = length(rig.joints[foot].bind_position - rig.joints[shin].bind_position);
        Vec3 hip = transform_point(pose.skin_matrices[bone_index(CharacterBone::Pelvis)], rig.joints[thigh].bind_position);
        hip.y += raise;
        const Vec3 chord = (is_left ? left.ankle : right.ankle) - hip;
        const float reach = clamp(length(chord), std::fabs(upper - lower) + height * .00001f,
                                  upper + lower - height * .00001f);
        const Vec3 direction = safe_normalize(chord, {0.f, -1.f, 0.f});
        const float along = (upper * upper - lower * lower + reach * reach) / (2.f * reach);
        const float bend = std::sqrt((std::max)(0.f, upper * upper - along * along));
        const Vec3 forward = safe_normalize(Vec3{0.f, 0.f, 1.f} - direction * direction.z,
                                            {0.f, 1.f, 0.f});
        const Vec3 knee_offset = direction * along + forward * bend;
        const Vec3 calf = knee_offset - chord;
        const float pitch = is_left ? left.pitch : right.pitch;
        if (knee_offset.y > -height * .015f * crouch ||
            std::atan2(calf.z, calf.y) - pitch > .88f)
          return true;
      }
      return false;
    };
    if (available > 0.f && folded(0.f)) {
      float lo = 0.f, hi = available;
      for (int i = 0; i < 16; ++i) {
        const float mid = .5f * (lo + hi);
        if (folded(mid)) lo = mid; else hi = mid;
      }
      for (auto& matrix : pose.skin_matrices) matrix.at(3, 1) += hi;
    }
  }
  solve_leg(rig, pose, true, left);
  solve_leg(rig, pose, false, right);
  return pose;
}

CharacterPose sample_character_animation(const CharacterRig& rig,
                                         const CharacterAnimationSample& sample) {
  return sample_character_animation_impl(rig, sample, -1.f, -1.f);
}

CharacterPose sample_character_animation(const CharacterRig& rig,
                                         const CharacterAnimationState& state) {
  return sample_character_animation_impl(rig, state.sample, state.gait_started ? 1.f : 0.f,
                                          state.support_fraction, &state);
}

}}}  // namespace fury::animation_detail::generic
