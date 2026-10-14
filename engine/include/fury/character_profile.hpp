#pragma once
#include "fury/character_animation.hpp"
#include "fury/mesh.hpp"
#include <string_view>
namespace fury {
/// A reviewed individual asset plus its renderer material. Unknown identities
/// retain the previously validated generic role/seed model without alteration.
struct CharacterProfile {
  CharacterModel model;
  Material material;
  float lod_distance{0.f};
  bool individualized{false};
  CharacterAnimationProfile animation_profile{CharacterAnimationProfile::Generic};
};
CharacterProfile make_character_profile(float height, CharacterRole role,
                                        std::string_view identity,
                                        CharacterLod lod);
} // namespace fury
