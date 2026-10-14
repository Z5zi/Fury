#include "fury/audio.hpp"
#include "fury/audio_mixer.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <filesystem>

#include "fury/log.hpp"

#if defined(FURY_HAS_SDL_MIXER) && FURY_HAS_SDL_MIXER
#include <SDL_mixer.h>
#endif

namespace fury {
namespace {

float cl01(float v) {
  return std::isfinite(v) ? std::clamp(v, 0.f, 1.f) : 0.f;
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
    wr16(44 + static_cast<std::size_t>(i) * 2,
         static_cast<std::uint16_t>(static_cast<std::int16_t>(std::clamp(v, -32767, 32767))));
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
    wr16(44 + static_cast<std::size_t>(i) * 2,
         static_cast<std::uint16_t>(static_cast<std::int16_t>(std::clamp(v, -32767, 32767))));
  }
  return buf;
}

/// Resolve assets/audio/meridian/<file> from common cwd layouts.
std::string resolve_meridian_wav(const char* filename) {
  namespace fs = std::filesystem;
  if (const char* root = std::getenv("FURY_AUDIO_ASSET_DIR")) {
    const fs::path path = fs::path(root) / filename;
    std::error_code ec;
    return fs::is_regular_file(path, ec) ? path.string() : std::string{};
  }
  if (char* base = SDL_GetBasePath()) {
    const fs::path path = fs::path(base) / "assets/audio/meridian" / filename;
    SDL_free(base);
    std::error_code ec;
    if (fs::is_regular_file(path, ec)) return path.string();
  }
  const char* prefixes[] = {
      "assets/audio/meridian/",
      "../assets/audio/meridian/",
      "../../assets/audio/meridian/",
      "../../../assets/audio/meridian/",
      "/workspace/Fury/assets/audio/meridian/",
  };
  for (const char* pre : prefixes) {
    fs::path p = fs::path(pre) / filename;
    std::error_code ec;
    if (fs::is_regular_file(p, ec)) {
      return p.string();
    }
  }
  return {};
}

bool is_zone_bed_cue(const char* name) {
  return std::strcmp(name, "zone_lobby") == 0 ||
         std::strcmp(name, "zone_security") == 0 ||
         std::strcmp(name, "zone_vault") == 0 ||
         std::strcmp(name, "zone_alley") == 0;
}

/// One implementation for actual SDL2 callback playback and deterministic offline output.
class CpuAudio final : public OfflineAudio {
 public:
  explicit CpuAudio(bool offline, int sample_rate = 48000)
      : m_offline(offline), m_mixer(sample_rate) {}
  ~CpuAudio() override { shutdown(); }

  bool init() override {
    if (m_initialized) return true;
    m_initialized = true;
    if (!m_offline) {
      if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        Log::warn(std::string("Audio: SDL2 audio initialization failed: ") + SDL_GetError());
        return true;  // Application remains usable without an output device.
      }
      m_owns_audio_subsystem = true;
      SDL_AudioSpec desired{}, obtained{};
      desired.freq = m_mixer.sample_rate();
      desired.format = AUDIO_F32SYS;
      desired.channels = 2;
      desired.samples = 1024;
      desired.callback = &CpuAudio::callback;
      desired.userdata = this;
      m_device = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained,
                                    SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
      if (!m_device) {
        Log::warn(std::string("Audio: SDL2 CPU output unavailable (silent): ") + SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        m_owns_audio_subsystem = false;
        return true;
      }
      m_mixer = CpuAudioMixer(obtained.freq);
    }
    load_bank();
    m_ready = true;
    apply_master_volume();
    m_mixer.set_muted(m_muted);
    if (m_device) SDL_PauseAudioDevice(m_device, 0);
    Log::info(std::string("Audio: ") + backend_name() + " stereo " +
              std::to_string(m_mixer.sample_rate()) + " Hz, 32 voices (authored WAVs + procedural cues)");
    return true;
  }

  void shutdown() override {
    // Closing joins the callback before any referenced clip data is destroyed.
    if (m_device) { SDL_CloseAudioDevice(m_device); m_device = 0; }
    m_mixer.stop_all();
    m_bank.clear();
    m_bed = 0;
    m_bed_name.clear();
    m_music_accum = 0.0;
    m_ready = false;
    m_initialized = false;
    if (m_owns_audio_subsystem) {
      SDL_QuitSubSystem(SDL_INIT_AUDIO);
      m_owns_audio_subsystem = false;
    }
  }

  const char* backend_name() const override {
    if (m_offline) return "cpu_offline";
    return m_ready ? "sdl_cpu" : "sdl_cpu(silent)";
  }
  int sample_rate() const override { return m_mixer.sample_rate(); }
  void render(float* stereo, std::size_t frames) override {
    // Only offline callers control the clock; a realtime device owns its callback.
    if (m_offline) m_mixer.render(stereo, frames);
  }

  void play_cue(const char* name) override {
    if (!name || !*name) return;
    const auto it = m_bank.find(name);
    if (!m_ready || it == m_bank.end()) {
      if (m_logged.insert(name).second)
        Log::info(std::string("Audio cue (silent/no sample, once): ") + name);
      return;
    }
    const bool bed = is_zone_bed_cue(name);
    if (m_muted && !bed) return;
    DeviceLock lock(m_device);
    if (bed) {
      // Repeated zone notifications retain phase; a zone change swaps one bed.
      if (m_bed_name == name && m_mixer.playing(m_bed)) return;
      m_mixer.stop(m_bed);
      m_bed = m_mixer.play(*it->second, 1.f, 0.f, true);
      m_bed_name = name;
    } else {
      const bool hum = std::strstr(name, "_hum") || std::strcmp(name, "zone_alley_traffic") == 0;
      m_mixer.play(*it->second, hum ? 0.35f : 1.f);
    }
  }
  void set_muted(bool muted) override {
    m_muted = muted;
    DeviceLock lock(m_device);
    m_mixer.set_muted(muted);
  }
  bool muted() const override { return m_muted; }
  void toggle_mute() override { set_muted(!m_muted); }
  void set_master_volume(float volume) override {
    m_master = cl01(volume);
    apply_master_volume();
  }
  float master_volume() const override { return m_master; }
  void set_ambience(float day, float night, float rain) override {
    m_day = cl01(day); m_night = cl01(night); m_rain = cl01(rain);
    apply_master_volume();
  }
  float ambience_day() const override { return m_day; }
  float ambience_night() const override { return m_night; }
  float ambience_rain() const override { return m_rain; }
  void set_music_intensity(float intensity) override { m_music = cl01(intensity); }
  float music_intensity() const override { return m_music; }
  void update(float dt) override {
    if (!m_ready || !std::isfinite(dt) || dt <= 0.f) return;
    const double interval = 0.92 - 0.74 * m_music;
    m_music_accum += dt;
    if (m_music_accum < interval) return;
    // Preserve phase without enqueueing an unbounded burst after a long frame.
    m_music_accum = std::fmod(m_music_accum, interval);
    if (m_muted) return;
    DeviceLock lock(m_device);
    const auto& layer = m_bank.at(m_music < 0.42f ? "music_idle" : "music_chase");
    const float gain = 0.12f + 0.38f * m_music;
    m_mixer.play(*layer, gain);
    if (m_music > 0.72f) m_mixer.play(*m_bank.at("music_idle"), gain * 0.35f);
  }

 private:
  class DeviceLock {
   public:
    explicit DeviceLock(SDL_AudioDeviceID device) : m_device(device) {
      if (m_device) SDL_LockAudioDevice(m_device);
    }
    ~DeviceLock() { if (m_device) SDL_UnlockAudioDevice(m_device); }
   private:
    SDL_AudioDeviceID m_device;
  };
  static void callback(void* user, Uint8* stream, int length) {
    // No file access, allocation, logging, or application locks in the callback.
    std::memset(stream, 0, static_cast<std::size_t>(length));
    auto* self = static_cast<CpuAudio*>(user);
    self->m_mixer.render(reinterpret_cast<float*>(stream),
                        static_cast<std::size_t>(length) / (2 * sizeof(float)));
  }
  void apply_master_volume() {
    const float ambience = cl01(0.55f * m_day + 0.40f * m_night + 0.35f * m_rain);
    DeviceLock lock(m_device);
    m_mixer.set_master_gain(m_master * (0.40f + 0.60f * ambience));
  }
  void add_procedural(const char* name, const std::vector<std::uint8_t>& wav) {
    // Procedural generators above always write mono little-endian PCM16 at 22050 Hz.
    auto clip = std::make_shared<AudioClip>();
    clip->sample_rate = 22050;
    clip->samples.reserve(wav.size() - 44);
    for (std::size_t i = 44; i + 1 < wav.size(); i += 2) {
      const unsigned raw = wav[i] | (static_cast<unsigned>(wav[i + 1]) << 8);
      const int signed_sample = raw >= 32768 ? static_cast<int>(raw) - 65536 : static_cast<int>(raw);
      const float value = static_cast<float>(signed_sample) / 32768.f;
      clip->samples.push_back(value); clip->samples.push_back(value);
    }
    m_bank[name] = std::move(clip);
  }
  void load_bank() {
    auto tone = [&](const char* name, float hz, float duration, float amplitude, float end = -1.f) {
      add_procedural(name, make_pcm_wav(22050, duration, hz, amplitude, end));
    };
    tone("footstep", 160.f, 0.045f, 0.35f);
    tone("heist_breach", 90.f, 0.12f, 0.55f, 40.f);
    tone("impact", 220.f, 0.07f, 0.5f, 80.f);
    tone("heist_start", 440.f, 0.09f, 0.4f, 660.f);
    tone("heist_success", 523.f, 0.22f, 0.45f, 784.f);
    tone("heist_fail", 392.f, 0.28f, 0.42f, 196.f);
    tone("siren", 680.f, 0.35f, 0.4f, 920.f);
    tone("radio_tick", 880.f, 0.05f, 0.32f, 1200.f);
    tone("complication", 740.f, 0.11f, 0.48f, 310.f);
    tone("enforcer_spawn", 110.f, 0.32f, 0.50f, 55.f);
    tone("music_idle", 196.f, 0.07f, 0.18f, 220.f);
    tone("music_chase", 330.f, 0.045f, 0.22f, 520.f);
    add_procedural("thunder", make_thunder_wav(22050, 0.85f, 0.62f));
    for (const char* name : {"zone_lobby", "zone_security", "zone_vault", "zone_alley",
                            "phone_ring", "printer", "radio_blip", "vault_motor",
                            "metal_stress", "police_radio", "alarm_klaxon", "footstep"}) {
      const auto path = resolve_meridian_wav((std::string(name) + ".wav").c_str());
      if (path.empty()) continue;
      auto clip = std::make_shared<AudioClip>();
      std::string error;
      if (load_audio_wav(path, *clip, &error)) {
        m_bank[name] = std::move(clip);
        Log::info(std::string("Audio: loaded authored cue ") + name);
      } else Log::warn(std::string("Audio: cannot load ") + path + ": " + error);
    }
    if (m_bank.count("radio_blip")) m_bank["radio_tick"] = m_bank["radio_blip"];
    if (m_bank.count("alarm_klaxon")) m_bank["siren"] = m_bank["alarm_klaxon"];
    for (const char* zone : {"zone_lobby", "zone_security", "zone_vault", "zone_alley"})
      if (m_bank.count(zone)) m_bank[std::string(zone) + "_hum"] = m_bank[zone];
    if (m_bank.count("zone_alley")) m_bank["zone_alley_traffic"] = m_bank["zone_alley"];
  }

  bool m_offline;
  CpuAudioMixer m_mixer;
  SDL_AudioDeviceID m_device{0};
  bool m_initialized{false}, m_ready{false}, m_owns_audio_subsystem{false}, m_muted{false};
  float m_master{1.f}, m_day{1.f}, m_night{0.f}, m_rain{0.f}, m_music{0.f};
  double m_music_accum{0.0};
  CpuAudioMixer::VoiceId m_bed{0};
  std::string m_bed_name;
  std::unordered_map<std::string, std::shared_ptr<AudioClip>> m_bank;
  std::unordered_set<std::string> m_logged;
};

#if defined(FURY_HAS_SDL_MIXER) && FURY_HAS_SDL_MIXER

Mix_Chunk* load_wav_chunk(const std::vector<std::uint8_t>& wav) {
  SDL_RWops* rw = SDL_RWFromConstMem(wav.data(), static_cast<int>(wav.size()));
  if (!rw) {
    return nullptr;
  }
  return Mix_LoadWAV_RW(rw, 1);
}


Mix_Chunk* load_wav_file(const char* filename) {
  const std::string path = resolve_meridian_wav(filename);
  if (path.empty()) {
    return nullptr;
  }
  Mix_Chunk* c = Mix_LoadWAV(path.c_str());
  if (!c) {
    Log::warn(std::string("Audio: failed to load authored WAV '") + path +
              "': " + Mix_GetError());
  } else {
    Log::info(std::string("Audio: loaded authored cue ") + filename);
  }
  return c;
}

class SdlMixerAudio final : public Audio {
 public:
  ~SdlMixerAudio() override { shutdown(); }
  bool init() override {
    if (m_ok) return true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
      Log::warn(std::string("SDL_INIT_AUDIO failed: ") + SDL_GetError() +
                " — falling back to silent cues");
      m_ok = false;
      return true;
    }
    m_owns_audio_subsystem = true;
    if (Mix_OpenAudio(22050, AUDIO_S16SYS, 1, 1024) != 0) {
      Log::warn(std::string("SDL_mixer open failed: ") + Mix_GetError() +
                " — falling back to silent cues");
      SDL_QuitSubSystem(SDL_INIT_AUDIO);
      m_owns_audio_subsystem = false;
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

    // Authored Meridian Mutual WAVs (zone beds + one-shots). Prefer these over beeps.
    auto load_file = [&](const char* cue, const char* file) {
      Mix_Chunk* c = load_wav_file(file);
      if (c) {
        m_authored[cue] = c;
      }
    };
    load_file("zone_lobby", "zone_lobby.wav");
    load_file("zone_security", "zone_security.wav");
    load_file("zone_vault", "zone_vault.wav");
    load_file("zone_alley", "zone_alley.wav");
    load_file("phone_ring", "phone_ring.wav");
    load_file("printer", "printer.wav");
    load_file("radio_blip", "radio_blip.wav");
    load_file("vault_motor", "vault_motor.wav");
    load_file("metal_stress", "metal_stress.wav");
    load_file("police_radio", "police_radio.wav");
    load_file("alarm_klaxon", "alarm_klaxon.wav");
    // Prefer authored footstep when present.
    if (Mix_Chunk* fs = load_wav_file("footstep.wav")) {
      if (m_footstep) {
        Mix_FreeChunk(m_footstep);
      }
      m_footstep = fs;
    }
    // Aliases → authored banks
    if (m_authored.count("radio_blip")) {
      m_authored["radio_tick"] = m_authored["radio_blip"];
    }
    if (m_authored.count("alarm_klaxon")) {
      m_authored["siren"] = m_authored["alarm_klaxon"];
    }
    // Soft hums reuse zone beds at lower volume via same chunk pointers.
    for (const char* z : {"zone_lobby", "zone_security", "zone_vault", "zone_alley"}) {
      if (m_authored.count(z)) {
        m_authored[std::string(z) + "_hum"] = m_authored[z];
      }
    }
    if (m_authored.count("zone_alley")) {
      m_authored["zone_alley_traffic"] = m_authored["zone_alley"];
    }

    apply_master_volume();
    Log::info("Audio: SDL_mixer backend (authored Meridian WAVs + procedural fallback + music stub)");
    return true;
  }

  void shutdown() override {
    if (m_ok) Mix_HaltChannel(-1);
    free_chunks();
    if (m_ok) {
      Mix_CloseAudio();
      m_ok = false;
    }
    if (m_owns_audio_subsystem) {
      SDL_QuitSubSystem(SDL_INIT_AUDIO);
      m_owns_audio_subsystem = false;
    }
    m_music_accum = 0.f;
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
    if (!chunk) {
      if (m_logged.insert(cue_name).second) {
        Log::info(std::string("Audio cue (no sample): ") + cue_name);
      }
      return;
    }
    const int vol = static_cast<int>(MIX_MAX_VOLUME * master_gain());
    Mix_VolumeChunk(chunk, vol);
    if (is_zone_bed_cue(cue_name)) {
      // Looping zone ambience on a reserved channel; swap when zone changes.
      if (m_bed_channel >= 0) {
        Mix_HaltChannel(m_bed_channel);
        m_bed_channel = -1;
      }
      m_bed_channel = Mix_PlayChannel(-1, chunk, -1);
      if (m_logged.insert(std::string("bed:") + cue_name).second) {
        Log::info(std::string("Audio zone bed playing: ") + cue_name);
      }
      return;
    }
    // Soft hum refresh — one-shot at reduced volume (bed already looping).
    if (std::strstr(cue_name, "_hum") || std::strcmp(cue_name, "zone_alley_traffic") == 0) {
      Mix_VolumeChunk(chunk, static_cast<int>(vol * 0.35f));
      Mix_PlayChannel(-1, chunk, 0);
      return;
    }
    Mix_PlayChannel(-1, chunk, 0);
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
    if (!m_ok || m_muted || !std::isfinite(dt) || dt <= 0.f) {
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
    {
      const auto it = m_authored.find(name);
      if (it != m_authored.end() && it->second) {
        return it->second;
      }
    }
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
    if (m_bed_channel >= 0) {
      Mix_HaltChannel(m_bed_channel);
      m_bed_channel = -1;
    }
    // Authored map may alias the same Mix_Chunk* under multiple cue names.
    std::unordered_set<Mix_Chunk*> unique;
    for (auto& kv : m_authored) {
      if (kv.second) unique.insert(kv.second);
    }
    m_authored.clear();
    for (Mix_Chunk* c : unique) {
      Mix_FreeChunk(c);
    }
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

  bool m_owns_audio_subsystem{false};
  bool m_ok{false};
  bool m_muted{false};
  float m_master{1.f};
  float m_day{1.f};
  float m_night{0.f};
  float m_rain{0.f};
  float m_music{0.f};
  float m_music_accum{0.f};
  int m_music_band{-1};
  int m_bed_channel{-1};
  std::unordered_map<std::string, Mix_Chunk*> m_authored;
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

std::unique_ptr<Audio> create_cpu_audio() {
  return std::make_unique<CpuAudio>(false);
}

std::unique_ptr<OfflineAudio> create_offline_audio(int sample_rate) {
  return std::make_unique<CpuAudio>(true, sample_rate);
}

std::unique_ptr<Audio> create_audio() {
  if (const char* backend = std::getenv("FURY_AUDIO_BACKEND")) {
    if (std::strcmp(backend, "null") == 0) return create_null_audio();
    if (std::strcmp(backend, "cpu") == 0) return create_cpu_audio();
    if (std::strcmp(backend, "mixer") != 0)
      Log::warn(std::string("Audio: unknown FURY_AUDIO_BACKEND '") + backend + "'; using default");
  }
#if defined(FURY_HAS_SDL_MIXER) && FURY_HAS_SDL_MIXER
  return std::make_unique<SdlMixerAudio>();
#else
  return create_cpu_audio();
#endif
}

}  // namespace fury
