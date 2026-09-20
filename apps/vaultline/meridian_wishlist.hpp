#pragma once

/// ChatGPT Remaining Top 8 — Harbor Metro / Meridian Mutual AAA wishlist.
/// Harbor Metro · HMPD · Meridian Mutual only (no GTA IP).

#include <fury/audio.hpp>
#include <fury/camera.hpp>
#include <fury/heist.hpp>
#include <fury/npc.hpp>
#include <fury/renderer.hpp>
#include <fury/scene.hpp>
#include <fury/security.hpp>
#include <fury/traffic.hpp>

#include <cstddef>
#include <string>

namespace meridian {

/// World-visible mission presentation states (distinct from HeistPhase).
enum class MissionWorldState {
  PreHeist = 0,  ///< civilians, normal lights, open lobby
  Alarm = 1,     ///< strobes, shutters, guards relocate, HMPD cue
  Escape = 2,    ///< street density, blockers, response vehicles
};

const char* mission_world_state_name(MissionWorldState s);

struct WishlistController {
  MissionWorldState world_state{MissionWorldState::PreHeist};
  bool aftermath_spawned{false};
  bool badge_unlocked{false};
  bool vault_unlocked{false};
  float vault_seq_t{0.f};       ///< 0..1 locking / open staged sequence
  int vault_seq_stage{-1};      ///< last logged stage index
  const char* last_audio_zone{""};
  float audio_zone_timer{0.f};
  bool profile_pending{false};
  bool cinematic_active{false};
  int cinematic_beat{-1};
  float cinematic_t{0.f};
  int cinematic_shots_written{0};
  bool smoke_scripted{false};
  float smoke_script_t{0.f};

  /// Spawn nav landmarks, security depth, vault machine, aftermath (hidden),
  /// shutters/blockers (hidden until alarm/escape). Call once after Meridian spawn.
  void spawn(fury::Scene& scene);

  /// Register MM cameras / badge / terminals into SecurityNet.
  void register_security(fury::Scene& scene, fury::SecurityNet& security);

  /// Drive world from heist phase + alarm flag. Logs transitions clearly.
  void sync_from_heist(fury::Scene& scene, fury::NpcSystem& npcs,
                       fury::TrafficSystem& traffic, fury::HeistPhase phase,
                       bool alarm_active);

  /// Force a specific world state (smoke / debug).
  void apply_world_state(fury::Scene& scene, fury::NpcSystem& npcs,
                         fury::TrafficSystem& traffic, MissionWorldState next);

  /// Advance vault locking / open machine sequence from heist phase.
  void update_vault_machine(fury::Scene& scene, fury::HeistPhase phase, float dt);

  /// Zone beds + cues via existing Audio API (procedural / silent OK).
  void update_zone_audio(fury::Audio& audio, const fury::Vec3& player_pos,
                         float dt, bool alarm_active);

  /// Try badge reader / maintenance panel near player. Returns true if consumed.
  bool try_security_interact(fury::Scene& scene, const fury::Vec3& player_pos);

  /// Dump frame/entity/memory profile to log (+ optional docs path).
  void dump_profile(fury::Scene& scene, const fury::Renderer& renderer,
                    float fps, float frame_ms, const char* out_md_path = nullptr);

  /// Scripted cinematic beats → PPM under artifacts/meridian_cinematics/.
  /// Returns true while capture still running.
  bool update_cinematic(fury::Camera& cam, fury::Renderer& renderer, float dt,
                        bool write_shots);

  /// Soft-smoke wishlist script: force pre→alarm→escape, profile, cinematics.
  /// Returns true when smoke should quit.
  bool update_smoke_script(fury::Scene& scene, fury::NpcSystem& npcs,
                           fury::TrafficSystem& traffic, fury::Camera& cam,
                           fury::Renderer& renderer, fury::Audio& audio,
                           float dt);
};

inline constexpr const char* kLogWishlistSpawned =
    "Wishlist: Meridian AAA props spawned (nav/security/vault/aftermath)";
inline constexpr const char* kLogMissionStatePrefix = "Mission world state -> ";
inline constexpr const char* kLogVaultMachinePrefix = "Vault machine stage -> ";
inline constexpr const char* kLogAudioZonePrefix = "Audio zone bed -> ";
inline constexpr const char* kLogProfilePrefix = "[profile] ";
inline constexpr const char* kLogCinematicPrefix = "Cinematic beat -> ";
inline constexpr const char* kLogAftermath = "Wishlist: alarm aftermath layered";
inline constexpr const char* kLogSecurityDepth =
    "Wishlist: security depth active (cams/badge/terminals/locked gate)";
inline constexpr const char* kLogNavReadability =
    "Wishlist: navigation readability (landmarks/door frames/light guidance)";

}  // namespace meridian
