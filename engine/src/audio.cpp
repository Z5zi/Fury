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

  void set_master_volume(float vol) override { m_master = cl01(vol); }
  float master_volume() const override { return m_master; }

  void set_ambience(float day_vol, float night_vol, float rain_vol) override {
    m_day = cl01(day_vol);
    m_night = cl01(night_vol);
    m_rain = cl01(rain_vol);
  }
  float ambience_day() const override { return m_day; }
  float ambience_night() const override { return m_night; }
  float ambience_rain() const override { return m_rain; }

  void set_music_intensity(float intensity) override {
    const float prev = m_music;
    m_music = cl01(intensity);
    // Log ambient vs chase band transitions once each direction.
    const int band = m_music < 0.35f ? 0 : (m_music < 0.70f ? 1 : 2);
    if (band != m_music_band) {
      m_music_band = band;
      const char* name =
          band == 0 ? "ambient idle" : (band == 1 ? "tension" : "chase");
      Log::info(std::string("Music intensity band -> ") + name + " (" +
                std::to_string(m_music) + ")");
      (void)prev;
    }
  }
  float music_intensity() const override { return m_music; }
  void update(float /*dt*/) override {}

 private:
  bool m_muted{false};
  float m_master{1.f};
  float m_day{1.f};
  float m_night{0.f};
  float m_rain{0.f};
  float m_music{0.f};
  int m_music_band{-1};
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

/// Low rumble + noise burst for storm lightning cue (procedural, no OGG).
std::vector<std::uint8_t> make_thunder_wav(int sample_rate, float duration_sec,
                                           float amp) {
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
  wr32(16, 16);
  wr16(20, 1);
  wr16(22, 1);
  wr32(24, static_cast<std::uint32_t>(sample_rate));
  wr32(28, static_cast<std::uint32_t>(sample_rate * 2));
  wr16(32, 2);
  wr16(34, 16);
  std::memcpy(buf.data() + 36, "data", 4);
  wr32(40, static_cast<std::uint32_t>(data_bytes));

  std::int16_t* samples = reinterpret_cast<std::int16_t*>(buf.data() + 44);
  const float two_pi = 6.28318530718f;
  std::uint32_t rng = 0xC0FFEEu;
  float lp = 0.f;
  for (int i = 0; i < n; ++i) {
    const float u = static_cast<float>(i) / static_cast<float>((std::max)(1, n - 1));
    const float t = static_cast<float>(i) / static_cast<float>(sample_rate);
    rng = rng * 1664525u + 1013904223u;
    const float noise = (static_cast<float>(rng >> 8) / 16777215.f) * 2.f - 1.f;
    lp = lp * 0.92f + noise * 0.08f;
    float env = 1.f;
    if (u < 0.04f) {
      env = u / 0.04f;
    } else if (u > 0.35f) {
      env = (1.f - u) / 0.65f;
      env = env * env;
    }
    env = cl01(env);
    const float rumble = std::sin(two_pi * (48.f - 22.f * u) * t) * 0.55f +
                         std::sin(two_pi * (72.f - 30.f * u) * t) * 0.28f;
    const float crack = (u < 0.12f) ? noise * (1.f - u / 0.12f) * 0.55f : 0.f;
    const float s = (rumble + lp * 0.85f + crack) * amp * env;
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
    m_fail = load("heist_fail", 392.f, 0.28f, 0.42f, 196.f);
    m_siren = load("siren", 680.f, 0.35f, 0.4f, 920.f);
    m_radio = load("radio_tick", 880.f, 0.05f, 0.32f, 1200.f);
    m_complication = load("complication", 740.f, 0.11f, 0.48f, 310.f);
    m_enforcer = load("enforcer_spawn", 110.f, 0.32f, 0.50f, 55.f);
    // Dynamic music layers — soft ambient pulse vs faster chase tick
    m_music_idle = load("music_idle", 196.f, 0.07f, 0.18f, 220.f);
    m_music_chase = load("music_chase", 330.f, 0.045f, 0.22f, 520.f);
    {
      auto wav = make_thunder_wav(22050, 0.85f, 0.62f);
      m_thunder = load_wav_chunk(wav);
      if (!m_thunder) {
        Log::warn(std::string("Audio: failed to load procedural cue 'thunder': ") +
                  Mix_GetError());
      }
    }

    apply_master_volume();
    Log::info("Audio: SDL_mixer backend (procedural PCM beeps + dynamic music stub)");
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

  void set_master_volume(float vol) override {
    m_master = cl01(vol);
    apply_master_volume();
  }
  float master_volume() const override { return m_master; }

  void set_ambience(float day_vol, float night_vol, float rain_vol) override {
    m_day = cl01(day_vol);
    m_night = cl01(night_vol);
    m_rain = cl01(rain_vol);
    apply_master_volume();
  }
  float ambience_day() const override { return m_day; }
  float ambience_night() const override { return m_night; }
  float ambience_rain() const override { return m_rain; }

  void set_music_intensity(float intensity) override {
    m_music = cl01(intensity);
    const int band = m_music < 0.35f ? 0 : (m_music < 0.70f ? 1 : 2);
    if (band != m_music_band) {
      m_music_band = band;
      const char* name =
          band == 0 ? "ambient idle" : (band == 1 ? "tension" : "chase");
      Log::info(std::string("Music intensity band -> ") + name + " (" +
                std::to_string(m_music) + ")");
    }
  }
  float music_intensity() const override { return m_music; }

  void update(float dt) override {
    if (!m_ok || m_muted || dt <= 0.f) {
      return;
    }
    // Tempo: ambient idle ~0.9s between soft pulses; chase ~0.18s.
    const float interval = 0.92f - 0.74f * m_music;
    m_music_accum += dt;
    if (m_music_accum < interval) {
      return;
    }
    m_music_accum = 0.f;

    Mix_Chunk* layer = (m_music < 0.42f) ? m_music_idle : m_music_chase;
    if (!layer) {
      return;
    }
    // Soft bed under SFX — scales with intensity + master/ambience gain.
    const float bed = (0.12f + 0.38f * m_music) * master_gain();
    Mix_VolumeChunk(layer, static_cast<int>(MIX_MAX_VOLUME * cl01(bed)));
    Mix_PlayChannel(-1, layer, 0);

    // Near chase: occasional second tick for denser pattern.
    if (m_music > 0.72f && m_music_chase) {
      Mix_VolumeChunk(m_music_chase,
                      static_cast<int>(MIX_MAX_VOLUME * cl01(bed * 0.7f)));
      // Slightly delayed second hit via immediate soft play of idle underlay.
      if (m_music_idle) {
        Mix_VolumeChunk(m_music_idle,
                        static_cast<int>(MIX_MAX_VOLUME * cl01(bed * 0.35f)));
        Mix_PlayChannel(-1, m_music_idle, 0);
      }
    }
  }

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
    if (std::strcmp(name, "heist_fail") == 0) {
      return m_fail;
    }
    if (std::strcmp(name, "siren") == 0) {
      return m_siren;
    }
    if (std::strcmp(name, "radio_tick") == 0) {
      return m_radio;
    }
    if (std::strcmp(name, "thunder") == 0) {
      return m_thunder;
    }
    if (std::strcmp(name, "complication") == 0) {
      return m_complication;
    }
    if (std::strcmp(name, "enforcer_spawn") == 0) {
      return m_enforcer;
    }
    return nullptr;
  }

  float master_gain() const {
    // Soft ambience blend — day/night/rain hooks scale master even for SFX.
    const float amb =
        0.55f * m_day + 0.40f * m_night + 0.35f * m_rain;
    return cl01(m_master * (0.40f + 0.60f * cl01(amb)));
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
    free_one(m_fail);
    free_one(m_siren);
    free_one(m_radio);
    free_one(m_thunder);
    free_one(m_complication);
    free_one(m_enforcer);
    free_one(m_music_idle);
    free_one(m_music_chase);
  }

  bool m_ok{false};
  bool m_muted{false};
  float m_master{1.f};
  float m_day{1.f};
  float m_night{0.f};
  float m_rain{0.f};
  float m_music{0.f};
  float m_music_accum{0.f};
  int m_music_band{-1};
  Mix_Chunk* m_footstep{nullptr};
  Mix_Chunk* m_breach{nullptr};
  Mix_Chunk* m_impact{nullptr};
  Mix_Chunk* m_start{nullptr};
  Mix_Chunk* m_success{nullptr};
  Mix_Chunk* m_fail{nullptr};
  Mix_Chunk* m_siren{nullptr};
  Mix_Chunk* m_radio{nullptr};
  Mix_Chunk* m_thunder{nullptr};
  Mix_Chunk* m_complication{nullptr};
  Mix_Chunk* m_enforcer{nullptr};
  Mix_Chunk* m_music_idle{nullptr};
  Mix_Chunk* m_music_chase{nullptr};
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
