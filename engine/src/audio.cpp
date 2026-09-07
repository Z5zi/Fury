#include "fury/audio.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "fury/log.hpp"

#if defined(FURY_HAS_SDL_MIXER) && FURY_HAS_SDL_MIXER
#include <SDL.h>
#include <SDL_mixer.h>
#endif

namespace fury {
namespace {

float cl01(float v) {
  return std::clamp(v, 0.f, 1.f);
}

class NullAudio final : public Audio {
 public:
  bool init() override {
    Log::info("Audio: null backend (silent cues OK; logged once each)");
    return true;
  }
  void shutdown() override {}
  void play_cue(const char* cue_name) override {
    if (!cue_name || !cue_name[0]) {
      return;
    }
    if (m_logged.insert(cue_name).second) {
      Log::info(std::string("Audio cue (silent, once): ") + cue_name +
                (m_muted ? " [muted]" : ""));
    }
  }
  const char* backend_name() const override { return "null"; }

  void set_muted(bool muted) override {
    if (m_muted == muted) {
      return;
    }
    m_muted = muted;
    Log::info(m_muted ? "Audio muted (F8)" : "Audio unmuted (F8)");
  }
  bool muted() const override { return m_muted; }
  void toggle_mute() override { set_muted(!m_muted); }

  void set_ambience(float day_vol, float night_vol, float rain_vol) override {
    m_day = cl01(day_vol);
    m_night = cl01(night_vol);
    m_rain = cl01(rain_vol);
  }
  float ambience_day() const override { return m_day; }
  float ambience_night() const override { return m_night; }
  float ambience_rain() const override { return m_rain; }

 private:
  bool m_muted{false};
  float m_day{1.f};
  float m_night{0.f};
  float m_rain{0.f};
  std::unordered_set<std::string> m_logged;
};

#if defined(FURY_HAS_SDL_MIXER) && FURY_HAS_SDL_MIXER

/// Tiny in-memory RIFF/WAVE (PCM 16-bit mono) for Mix_LoadWAV_RW — no OGG assets.
std::vector<std::uint8_t> make_pcm_wav(int sample_rate, float duration_sec,
                                       float freq_hz, float amp,
                                       float freq_end_hz = -1.f) {
  if (freq_end_hz < 0.f) {
    freq_end_hz = freq_hz;
  }
  const int n = (std::max)(1, static_cast<int>(sample_rate * duration_sec));
  const int data_bytes = n * 2;
  std::vector<std::uint8_t> buf(44 + static_cast<std::size_t>(data_bytes));
  auto wr32 = [&](std::size_t off, std::uint32_t v) {
    buf[off] = static_cast<std::uint8_t>(v & 0xff);
    buf[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xff);
    buf[off + 2] = static_cast<std::uint8_t>((v >> 16) & 0xff);
    buf[off + 3] = static_cast<std::uint8_t>((v >> 24) & 0xff);
  };
  auto wr16 = [&](std::size_t off, std::uint16_t v) {
    buf[off] = static_cast<std::uint8_t>(v & 0xff);
    buf[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xff);
  };
  std::memcpy(buf.data(), "RIFF", 4);
  wr32(4, 36 + static_cast<std::uint32_t>(data_bytes));
  std::memcpy(buf.data() + 8, "WAVEfmt ", 8);
  wr32(16, 16);  // PCM fmt chunk size
  wr16(20, 1);   // PCM
  wr16(22, 1);   // mono
  wr32(24, static_cast<std::uint32_t>(sample_rate));
  wr32(28, static_cast<std::uint32_t>(sample_rate * 2));
  wr16(32, 2);   // block align
  wr16(34, 16);  // bits
  std::memcpy(buf.data() + 36, "data", 4);
  wr32(40, static_cast<std::uint32_t>(data_bytes));

  std::int16_t* samples =
      reinterpret_cast<std::int16_t*>(buf.data() + 44);
  const float two_pi = 6.28318530718f;
  for (int i = 0; i < n; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(sample_rate);
    const float u = static_cast<float>(i) / static_cast<float>((std::max)(1, n - 1));
    const float f = freq_hz + (freq_end_hz - freq_hz) * u;
    // Simple attack/decay envelope
    float env = 1.f;
    if (u < 0.08f) {
      env = u / 0.08f;
    } else if (u > 0.7f) {
      env = (1.f - u) / 0.3f;
    }
    env = cl01(env);
    const float s = std::sin(two_pi * f * t) * amp * env;
    const int v = static_cast<int>(s * 32767.f);
    samples[i] = static_cast<std::int16_t>(std::clamp(v, -32767, 32767));
  }
  return buf;
}

Mix_Chunk* load_wav_chunk(const std::vector<std::uint8_t>& wav) {
  SDL_RWops* rw = SDL_RWFromConstMem(wav.data(), static_cast<int>(wav.size()));
  if (!rw) {
    return nullptr;
  }
  return Mix_LoadWAV_RW(rw, 1);
}

class SdlMixerAudio final : public Audio {
 public:
  bool init() override {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
      Log::warn(std::string("SDL_INIT_AUDIO failed: ") + SDL_GetError() +
                " — falling back to silent cues");
      m_ok = false;
      return true;
    }
    if (Mix_OpenAudio(22050, AUDIO_S16SYS, 1, 1024) != 0) {
      Log::warn(std::string("SDL_mixer open failed: ") + Mix_GetError() +
                " — falling back to silent cues");
      m_ok = false;
      return true;
    }
    Mix_AllocateChannels(16);
    m_ok = true;

    // Procedural beeps (no external sample bank / OGG)
    auto load = [&](const char* name, float freq, float dur, float amp,
                    float freq_end = -1.f) -> Mix_Chunk* {
      auto wav = make_pcm_wav(22050, dur, freq, amp, freq_end);
      Mix_Chunk* c = load_wav_chunk(wav);
      if (!c) {
        Log::warn(std::string("Audio: failed to load procedural cue '") + name +
                  "': " + Mix_GetError());
      }
      return c;
    };
    m_footstep = load("footstep", 160.f, 0.045f, 0.35f);
    m_breach = load("heist_breach", 90.f, 0.12f, 0.55f, 40.f);
    m_impact = load("impact", 220.f, 0.07f, 0.5f, 80.f);
    m_start = load("heist_start", 440.f, 0.09f, 0.4f, 660.f);
    m_success = load("heist_success", 523.f, 0.22f, 0.45f, 784.f);
    m_siren = load("siren", 680.f, 0.35f, 0.4f, 920.f);
    m_radio = load("radio_tick", 880.f, 0.05f, 0.32f, 1200.f);

    apply_master_volume();
    Log::info("Audio: SDL_mixer backend (procedural PCM beeps)");
    return true;
  }

  void shutdown() override {
    free_chunks();
    if (m_ok) {
      Mix_CloseAudio();
      m_ok = false;
    }
    if (SDL_WasInit(SDL_INIT_AUDIO)) {
      SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
  }

  void play_cue(const char* cue_name) override {
    if (!cue_name) {
      return;
    }
    if (!m_ok || m_muted) {
      if (m_logged.insert(cue_name).second) {
        Log::info(std::string("Audio cue (silent, once): ") + cue_name +
                  (m_muted ? " [muted]" : " [mixer down]"));
      }
      return;
    }
    Mix_Chunk* chunk = chunk_for(cue_name);
    if (chunk) {
      const int vol = static_cast<int>(MIX_MAX_VOLUME * master_gain());
      Mix_VolumeChunk(chunk, vol);
      Mix_PlayChannel(-1, chunk, 0);
    } else if (m_logged.insert(cue_name).second) {
      Log::info(std::string("Audio cue (no sample): ") + cue_name);
    }
  }

  const char* backend_name() const override {
    return m_ok ? "sdl_mixer" : "sdl_mixer(silent)";
  }

  void set_muted(bool muted) override {
    if (m_muted == muted) {
      return;
    }
    m_muted = muted;
    apply_master_volume();
    Log::info(m_muted ? "Audio muted (F8)" : "Audio unmuted (F8)");
  }
  bool muted() const override { return m_muted; }
  void toggle_mute() override { set_muted(!m_muted); }

  void set_ambience(float day_vol, float night_vol, float rain_vol) override {
    m_day = cl01(day_vol);
    m_night = cl01(night_vol);
    m_rain = cl01(rain_vol);
    apply_master_volume();
  }
  float ambience_day() const override { return m_day; }
  float ambience_night() const override { return m_night; }
  float ambience_rain() const override { return m_rain; }

 private:
  Mix_Chunk* chunk_for(const char* name) const {
    if (std::strcmp(name, "footstep") == 0) {
      return m_footstep;
    }
    if (std::strcmp(name, "heist_breach") == 0) {
      return m_breach;
    }
    if (std::strcmp(name, "impact") == 0) {
      return m_impact;
    }
    if (std::strcmp(name, "heist_start") == 0) {
      return m_start;
    }
    if (std::strcmp(name, "heist_success") == 0) {
      return m_success;
    }
    if (std::strcmp(name, "siren") == 0) {
      return m_siren;
    }
    if (std::strcmp(name, "radio_tick") == 0) {
      return m_radio;
    }
    return nullptr;
  }

  float master_gain() const {
    // Soft ambience blend — day/night/rain hooks scale master even for SFX.
    const float amb =
        0.55f * m_day + 0.40f * m_night + 0.35f * m_rain;
    return cl01(0.40f + 0.60f * cl01(amb));
  }

  void apply_master_volume() {
    if (!m_ok) {
      return;
    }
    if (m_muted) {
      Mix_Volume(-1, 0);
      return;
    }
    Mix_Volume(-1, static_cast<int>(MIX_MAX_VOLUME * master_gain()));
  }

  void free_chunks() {
    auto free_one = [](Mix_Chunk*& c) {
      if (c) {
        Mix_FreeChunk(c);
        c = nullptr;
      }
    };
    free_one(m_footstep);
    free_one(m_breach);
    free_one(m_impact);
    free_one(m_start);
    free_one(m_success);
    free_one(m_siren);
    free_one(m_radio);
  }

  bool m_ok{false};
  bool m_muted{false};
  float m_day{1.f};
  float m_night{0.f};
  float m_rain{0.f};
  Mix_Chunk* m_footstep{nullptr};
  Mix_Chunk* m_breach{nullptr};
  Mix_Chunk* m_impact{nullptr};
  Mix_Chunk* m_start{nullptr};
  Mix_Chunk* m_success{nullptr};
  Mix_Chunk* m_siren{nullptr};
  Mix_Chunk* m_radio{nullptr};
  std::unordered_set<std::string> m_logged;
};

#endif  // FURY_HAS_SDL_MIXER

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
