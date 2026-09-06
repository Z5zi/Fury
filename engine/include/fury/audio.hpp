#pragma once

#include <memory>
#include <string>

namespace fury {

/// Minimal audio façade. Cue names: "heist_start", "heist_success", …
class Audio {
 public:
  virtual ~Audio() = default;
  virtual bool init() = 0;
  virtual void shutdown() = 0;
  virtual void play_cue(const char* cue_name) = 0;
  virtual const char* backend_name() const = 0;
};

/// Always available — logs cues, plays silence.
std::unique_ptr<Audio> create_null_audio();

/// Optional SDL_mixer backend when FURY_HAS_SDL_MIXER=1; otherwise returns null audio.
std::unique_ptr<Audio> create_audio();

}  // namespace fury
