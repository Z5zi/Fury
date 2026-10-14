#include "fury/player_hair.hpp"

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>

namespace fury {
namespace {
struct FrozenHair { std::once_flag once; PlayerHairConfig config; };
FrozenHair& frozen_config() {
  static FrozenHair state;
  return state;
}
bool binary(const char* value, const char* error) {
  if (!value || !std::strcmp(value,"0")) return false;
  if (!std::strcmp(value,"1")) return true;
  throw std::invalid_argument(error);
}
void validate(const PlayerHairConfig& config) {
  if (config.short_tail && !config.source)
    throw std::invalid_argument("Short Player tail requires FURY_PLAYER_SOURCE_HAIR=1");
  if (config.detail_resolution != 0 && config.detail_resolution != 512 && config.detail_resolution != 1024)
    throw std::invalid_argument("FURY_PLAYER_HAIR_DETAIL must be 0, 512 or 1024");
  if (config.detail_resolution && !config.source)
    throw std::invalid_argument("Player hair detail requires FURY_PLAYER_SOURCE_HAIR=1");
}
}
PlayerHairEnvironment player_hair_environment() {
  return {std::getenv("FURY_PLAYER_SOURCE_HAIR"), std::getenv("FURY_PLAYER_SHORT_TAIL"),
          std::getenv("FURY_PLAYER_HAIR_DETAIL")};
}
bool player_hair_backend_supported(RenderBackendKind backend) {
  return backend == RenderBackendKind::Software || backend == RenderBackendKind::CpuRayTracing ||
         backend == RenderBackendKind::OpenGL;
}
const char* player_hair_preference_name(PlayerHairPreference preference) {
  switch (preference) {
    case PlayerHairPreference::Original: return "ORIGINAL";
    case PlayerHairPreference::Balanced: return "BALANCED";
    default: return "AUTO";
  }
}
PlayerHairPreference cycle_player_hair_preference(PlayerHairPreference preference, int direction,
                                                 RenderBackendKind backend) {
  // DX12 cannot select Balanced. A saved Balanced request survives loading and
  // uses Original until a supported renderer is active; do not erase the request.
  const int count = player_hair_backend_supported(backend) ? 3 : 2;
  const int current = static_cast<int>(preference);
  return static_cast<PlayerHairPreference>((current + (direction < 0 ? -1 : 1) + count) % count);
}
PlayerHairConfig resolve_player_hair(PlayerHairPreference preference, QualityLevel quality,
    RenderBackendKind actual_backend, bool npc_detail, bool individual_characters,
    PlayerHairEnvironment environment) {
  PlayerHairConfig result;
  result.environment_override = environment.present();
  result.actor_enabled = npc_detail && individual_characters;
  result.backend_supported = player_hair_backend_supported(actual_backend);
  if (result.environment_override) {
    result.source = binary(environment.source,"FURY_PLAYER_SOURCE_HAIR must be 0 or 1");
    result.short_tail = binary(environment.tail,"FURY_PLAYER_SHORT_TAIL must be 0 or 1");
    if (!environment.detail || !std::strcmp(environment.detail,"0")) result.detail_resolution = 0;
    else if (!std::strcmp(environment.detail,"512")) result.detail_resolution = 512;
    else if (!std::strcmp(environment.detail,"1024")) result.detail_resolution = 1024;
    else throw std::invalid_argument("FURY_PLAYER_HAIR_DETAIL must be 0, 512 or 1024");
    validate(result);
  } else if (result.backend_supported &&
      (preference == PlayerHairPreference::Balanced ||
       (preference == PlayerHairPreference::Auto && quality != QualityLevel::Low))) {
    result.source = result.short_tail = true;
    result.detail_resolution = 512;
  }
  if (!result.actor_enabled) {
    result.source = result.short_tail = false;
    result.detail_resolution = 0;
  }
  return result;
}
bool same_player_hair_groom(const PlayerHairConfig& a, const PlayerHairConfig& b) {
  return a.source == b.source && a.short_tail == b.short_tail &&
         a.detail_resolution == b.detail_resolution;
}
void configure_player_hair(const PlayerHairConfig& config) {
  validate(config);
  auto& frozen = frozen_config();
  bool applied = false;
  std::call_once(frozen.once, [&] { frozen.config = config; applied = true; });
  if (!applied) throw std::logic_error("Player hair is already frozen; restart before reconfiguring");
}
const PlayerHairConfig& player_hair_configuration() {
  auto& frozen = frozen_config();
  std::call_once(frozen.once, [&] {
    frozen.config = resolve_player_hair(PlayerHairPreference::Original,
        QualityLevel::Low, RenderBackendKind::Software, true, true, player_hair_environment());
  });
  return frozen.config;
}
PlayerHairConfig preview_player_hair(const PlayerHairConfig& active,
    PlayerHairPreference preference, QualityLevel quality, RenderBackendKind actual_backend,
    const QualityLevel* restart_quality_override) {
  if (active.environment_override) return active;
  return resolve_player_hair(preference, restart_quality_override ? *restart_quality_override : quality,
      actual_backend, active.actor_enabled, true);
}
const char* player_hair_groom_name(const PlayerHairConfig& config) {
  if (!config.actor_enabled) return "DISABLED";
  if (!config.source) return "ORIGINAL";
  if (config.short_tail && config.detail_resolution == 512) return "BALANCED";
  return "CUSTOM";
}
} // namespace fury
