#pragma once

#include <memory>
#include <string>

namespace fury {

/// Minimal audio façade. Cue names: "heist_start", "heist_success",
/// "heist_breach" / "impact", "footstep", "siren", "thunder", …
/// Optional SDL_mixer plays tiny procedural PCM beeps; null backend stays silent
/// and logs each cue name once.
class Audio {
 public:
  virtual ~Audio() = default;
  virtual bool init() = 0;
  virtual void shutdown() = 0;
  virtual void play_cue(const char* cue_name) = 0;
  virtual const char* backend_name() const = 0;

  /// Master mute (F8). Silent backends still honor the flag for ambience hooks.
  virtual void set_muted(bool muted) = 0;
  virtual bool muted() const = 0;
  virtual void toggle_mute() = 0;

  /// Day / night / rain ambience volume hooks in [0,1] (even when silent).
  virtual void set_ambience(float day_vol, float night_vol, float rain_vol) = 0;
  virtual float ambience_day() const = 0;
  virtual float ambience_night() const = 0;
  virtual float ambience_rain() const = 0;
};

/// Always available — logs each cue once, plays silence.
std::unique_ptr<Audio> create_null_audio();

/// Optional SDL_mixer backend when FURY_HAS_SDL_MIXER=1; otherwise returns null audio.
std::unique_ptr<Audio> create_audio();

}  // namespace fury
