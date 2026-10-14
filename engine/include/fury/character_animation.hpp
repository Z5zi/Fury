#pragma once

#include "fury/character_model.hpp"

#include <cstdint>
#include <array>
#include <limits>

namespace fury {

/// Motion policy selected by an asset factory, never by actor name or seed.
/// EquippedCrouchExperimental is a laboratory candidate, not a geometry gate.
enum class CharacterAnimationProfile : std::uint8_t {
  Generic = 0,
  EquippedCrouchExperimental = 1,
};
const char* character_animation_profile_name(CharacterAnimationProfile profile);


/// Read-only presentation inputs. Travel is the distance the actor actually
/// covered, after collision/path following, not its requested locomotion speed.
struct CharacterAnimationInput {
  float delta_time{0.f};
  float distance_delta{0.f};
  float travel_speed{0.f};
  float move_weight{0.f};
  float yaw{0.f};
  /// NaN asks the animator to derive the shortest-arc rate from yaw changes.
  float turn_rate{std::numeric_limits<float>::quiet_NaN()};
  /// Look direction is relative to actor facing, in radians.
  float attention_yaw{0.f};
  float attention_pitch{0.f};
  float attention_weight{0.f};
  float talk_weight{0.f};
  float crouch_weight{0.f};
};

/// Stateless pose request, also useful for deterministic render captures.
/// One gait cycle is two steps; phase is cycles, not radians. The normal stateful
/// path advances this from distance / stride_length, never an idle clock.
struct CharacterAnimationSample {
  double phase_cycles{0.0};
  double idle_time{0.0};
  float speed{0.f};
  /// Full gait-cycle travel in meters; <= 0 chooses character_stride_length.
  float stride_length{0.f};
  float move_weight{0.f};
  float turn_rate{0.f};
  float attention_yaw{0.f};
  float attention_pitch{0.f};
  float attention_weight{0.f};
  float talk_weight{0.f};
  float crouch_weight{0.f};
  std::uint32_t seed{0};
};

struct CharacterAnimationState {
  CharacterAnimationSample sample;
  float previous_yaw{0.f};
  bool initialized{false};
  /// Keep a comfortable staggered support pose on stop; never drag both feet
  /// back to a time-driven neutral stance. The swing foot settles vertically.
  bool gait_started{false};
  float support_fraction{.62f};
  /// A stopped crouch may make two deliberate unloaded recovery steps. This
  /// presentation state does not advance the distance-driven gait clock.
  bool settling_feet{false};
  bool settled_feet{false};
  unsigned settling_index{0};
  unsigned settling_first{0};
  float settling_time{0.f};
  std::array<float, 2> settling_z{};
  std::array<float, 2> settling_start_z{};
  std::array<float, 2> settling_start_lift{};
  std::array<float, 2> settling_start_pitch{};
  std::array<float, 2> settling_lift{};
  std::array<float, 2> settling_pitch{};
  /// An interrupted recovery rejoins locomotion through an unloaded arc.
  float recovery_time{0.f};
  bool recovering_step{false};
  unsigned recovery_foot{0};
  std::array<float, 2> recovery_z{};
  std::array<float, 2> recovery_lift{};
  std::array<float, 2> recovery_pitch{};
  // Experimental recovery data is appended; generic arithmetic is unchanged.
  float recovery_root_z{0.f};
  /// Bound motion policy: stateful sampling uses exactly the policy advanced.
  CharacterAnimationProfile profile{CharacterAnimationProfile::Generic};
};

/// Natural speed-dependent cycle travel. Slow steps shorten; a fast chase
/// smoothly changes to a run with shorter support and increased clearance.
float character_stride_length(float height, float speed, float crouch_weight = 0.f);
/// Fraction of each cycle spent in support: .62 walking to .47 running.
float character_stance_fraction(float height, float speed, float crouch_weight = 0.f);
/// Exponential filtering keeps start/stop, turning, attention and dialogue soft.
/// Nonfinite/negative dt freezes state; dt is bounded to .25s. Distance is
/// nonnegative and bounded against discontinuous teleports. No allocations.
void advance_character_animation(CharacterAnimationState& state,
                                 const CharacterAnimationInput& input,
                                 float height);
/// Explicit profile migration. A valid step changing policy resets recovery and
/// gait state while retaining seed/idle clock. Invalid dt freezes all state.
/// The height-only overload continues state.profile (Generic for a new state).
void advance_character_animation(CharacterAnimationState& state,
                                 const CharacterAnimationInput& input,
                                 float height, CharacterAnimationProfile profile);
/// Rigid bone transforms, analytical two-bone legs and counter-rotated ankles.
/// Ground is -rig.height/2. Original bone lengths and rig height are untouched.
/// Invalid rigs produce the identity pose rather than propagating NaNs.
CharacterPose sample_character_animation(const CharacterRig& rig,
                                         const CharacterAnimationSample& sample);
CharacterPose sample_character_animation(const CharacterRig& rig,
                                         const CharacterAnimationState& state);
/// Stateless callers must pass the asset's metadata to opt in. The two-argument
/// stateless API always remains Generic, independent of rig/role/seed/identity.
CharacterPose sample_character_animation(const CharacterRig& rig,
                                         const CharacterAnimationSample& sample,
                                         CharacterAnimationProfile profile);

}  // namespace fury
