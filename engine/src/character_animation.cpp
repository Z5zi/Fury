#include "fury/character_animation.hpp"

#include <cmath>

namespace fury {
namespace animation_detail {
namespace generic {
float character_stride_length(float, float, float);
float character_stance_fraction(float, float, float);
void advance_character_animation(CharacterAnimationState&, const CharacterAnimationInput&, float);
CharacterPose sample_character_animation(const CharacterRig&, const CharacterAnimationSample&);
CharacterPose sample_character_animation(const CharacterRig&, const CharacterAnimationState&);
}
namespace equipped_crouch {
void advance_character_animation(CharacterAnimationState&, const CharacterAnimationInput&, float);
CharacterPose sample_character_animation(const CharacterRig&, const CharacterAnimationSample&);
CharacterPose sample_character_animation(const CharacterRig&, const CharacterAnimationState&);
}
}
namespace {
CharacterAnimationProfile canonical_profile(CharacterAnimationProfile profile) {
  return profile == CharacterAnimationProfile::EquippedCrouchExperimental ? profile :
      CharacterAnimationProfile::Generic;
}
}
const char* character_animation_profile_name(CharacterAnimationProfile profile) {
  return canonical_profile(profile) == CharacterAnimationProfile::EquippedCrouchExperimental ?
      "equipped_crouch_experimental" : "generic";
}
float character_stride_length(float height, float speed, float crouch) {
  return animation_detail::generic::character_stride_length(height, speed, crouch);
}
float character_stance_fraction(float height, float speed, float crouch) {
  return animation_detail::generic::character_stance_fraction(height, speed, crouch);
}
void advance_character_animation(CharacterAnimationState& state,
                                 const CharacterAnimationInput& input,
                                 float height) {
  advance_character_animation(state, input, height, state.profile);
}
void advance_character_animation(CharacterAnimationState& state,
                                 const CharacterAnimationInput& input,
                                 float height, CharacterAnimationProfile profile) {
  // Match the numerical kernels' freeze contract, including profile changes.
  if (!std::isfinite(input.delta_time) || input.delta_time <= 0.f) return;
  profile = canonical_profile(profile);
  if (state.profile != profile) {
    const auto seed = state.sample.seed;
    const auto idle_time = state.sample.idle_time;
    state = {};
    state.sample.seed = seed;
    state.sample.idle_time = idle_time;
    state.profile = profile;
  }
  if (profile == CharacterAnimationProfile::EquippedCrouchExperimental)
    animation_detail::equipped_crouch::advance_character_animation(state, input, height);
  else
    animation_detail::generic::advance_character_animation(state, input, height);
}
CharacterPose sample_character_animation(const CharacterRig& rig,
                                         const CharacterAnimationSample& sample) {
  return animation_detail::generic::sample_character_animation(rig, sample);
}
CharacterPose sample_character_animation(const CharacterRig& rig,
                                         const CharacterAnimationSample& sample,
                                         CharacterAnimationProfile profile) {
  if (canonical_profile(profile) == CharacterAnimationProfile::EquippedCrouchExperimental)
    return animation_detail::equipped_crouch::sample_character_animation(rig, sample);
  return animation_detail::generic::sample_character_animation(rig, sample);
}
CharacterPose sample_character_animation(const CharacterRig& rig,
                                         const CharacterAnimationState& state) {
  if (canonical_profile(state.profile) == CharacterAnimationProfile::EquippedCrouchExperimental)
    return animation_detail::equipped_crouch::sample_character_animation(rig, state);
  return animation_detail::generic::sample_character_animation(rig, state);
}
} // namespace fury
