#include "fury/audio.hpp"
#include <memory>
#include <string>

#include "fury/log.hpp"

#if defined(FURY_HAS_SDL_MIXER) && FURY_HAS_SDL_MIXER
#include <SDL.h>
#include <SDL_mixer.h>
#endif


namespace fury {
namespace {

class NullAudio final : public Audio {
 public:
  bool init() override {
    Log::info("Audio: null backend (silent cues OK)");
    return true;
  }
  void shutdown() override {}
  void play_cue(const char* cue_name) override {
    if (cue_name) {
      Log::info(std::string("Audio cue (silent): ") + cue_name);
    }
  }
  const char* backend_name() const override { return "null"; }
};

#if defined(FURY_HAS_SDL_MIXER) && FURY_HAS_SDL_MIXER
class SdlMixerAudio final : public Audio {
 public:
  bool init() override {
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) != 0) {
      Log::warn(std::string("SDL_mixer open failed: ") + Mix_GetError() +
                " — falling back to silent cues");
      m_ok = false;
      return true;  // still usable as silent
    }
    m_ok = true;
    Log::info("Audio: SDL_mixer backend");
    return true;
  }
  void shutdown() override {
    if (m_ok) {
      Mix_CloseAudio();
      m_ok = false;
    }
  }
  void play_cue(const char* cue_name) override {
    // No baked sample assets yet — hook only (log + optional future Mix_PlayChannel).
    Log::info(std::string("Audio cue: ") + (cue_name ? cue_name : "?") +
              (m_ok ? " [mixer ready, no sample]" : " [silent]"));
  }
  const char* backend_name() const override {
    return m_ok ? "sdl_mixer" : "sdl_mixer(silent)";
  }

 private:
  bool m_ok{false};
};
#endif

}  // namespace

std::unique_ptr<Audio> create_null_audio() {
  return std::make_unique<NullAudio>();
}

std::unique_ptr<Audio> create_audio() {
#if defined(FURY_HAS_SDL_MIXER) && FURY_HAS_SDL_MIXER
  return std::make_unique<SdlMixerAudio>();
#else
  return create_null_audio();
#endif
}

}  // namespace fury
