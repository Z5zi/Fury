#pragma once

#include "fury/quality.hpp"

namespace fury {

// The saved preference is distinct from the immutable groom used this run.
enum class PlayerHairPreference : int { Auto = 0, Original = 1, Balanced = 2 };
struct PlayerHairEnvironment {
  const char* source{};
  const char* tail{};
  const char* detail{};
  bool present() const { return source || tail || detail; }
};
struct PlayerHairConfig {
  bool source{};
  bool short_tail{};
  int detail_resolution{};
  bool environment_override{};
  bool actor_enabled{true};
  bool backend_supported{true};
};
PlayerHairEnvironment player_hair_environment();
bool player_hair_backend_supported(RenderBackendKind backend);
const char* player_hair_preference_name(PlayerHairPreference preference);
PlayerHairPreference cycle_player_hair_preference(PlayerHairPreference preference, int direction,
                                                 RenderBackendKind backend);
// Any present environment field selects the entire legacy bundle. Missing fields
// are zero, never merged with a preset. Validation precedes disable gates.
PlayerHairConfig resolve_player_hair(PlayerHairPreference preference, QualityLevel quality,
    RenderBackendKind actual_backend, bool npc_detail, bool individual_characters,
    PlayerHairEnvironment environment = {});
bool same_player_hair_groom(const PlayerHairConfig& a, const PlayerHairConfig& b);
// Call exactly once, after renderer fallback, before any model/surface creation.
// Legacy tools which never configure retain their cached environment-only default.
void configure_player_hair(const PlayerHairConfig& config);
const PlayerHairConfig& player_hair_configuration();
// Environment and disable gates remain frozen. Menu previews never read or mutate
// process environment, allocate a texture, or rebuild a model.
PlayerHairConfig preview_player_hair(const PlayerHairConfig& active,
    PlayerHairPreference preference, QualityLevel quality, RenderBackendKind actual_backend,
    const QualityLevel* restart_quality_override = nullptr);
const char* player_hair_groom_name(const PlayerHairConfig& config);

} // namespace fury
