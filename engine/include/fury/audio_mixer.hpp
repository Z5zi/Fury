#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fury {

/// Immutable while playing. Samples are interleaved left/right floats at sample_rate.
struct AudioClip {
  int sample_rate{48000};
  std::vector<float> samples;
  std::size_t frames() const { return samples.size() / 2; }
};

/// Deterministic CPU mixer shared by the SDL callback and offline validation.
/// No allocation, file I/O, or locks in render(). Caller serializes all methods
/// and keeps clips alive and unchanged until their voices stop.
class CpuAudioMixer {
 public:
  using VoiceId = std::uint64_t;
  static constexpr std::size_t max_voices = 32;

  explicit CpuAudioMixer(int output_sample_rate = 48000);
  int sample_rate() const { return m_sample_rate; }
  /// Linear rate conversion; pitch must be in [0.125,8]. Gain in [0,1], pan [-1,1].
  /// Steals the oldest one-shot under pressure; never steals a looping bed.
  /// Returns zero for invalid clips/parameters or when every voice is looping.
  VoiceId play(const AudioClip& clip, float gain = 1.f, float pan = 0.f,
               bool loop = false, float pitch = 1.f);
  void stop(VoiceId id);
  void stop_all();
  bool playing(VoiceId id) const;
  std::size_t active_voices() const;
  void set_master_gain(float gain);
  float master_gain() const { return m_master; }
  void set_muted(bool muted) { m_muted = muted; }
  bool muted() const { return m_muted; }
  /// Writes exactly frames*2 floats in [-1,1]; clips summed peaks, never wraps.
  /// Muting silences output while advancing voices (no delayed burst on unmute).
  void render(float* stereo, std::size_t frames);

 private:
  struct Voice {
    const AudioClip* clip{nullptr};
    VoiceId id{0};
    double position{0.0};
    double step{1.0};
    float left{1.f}, right{1.f};
    bool loop{false};
  };
  int m_sample_rate;
  std::array<Voice, max_voices> m_voices{};
  VoiceId m_next_id{1};
  float m_master{1.f};
  bool m_muted{false};
};

/// SDL's WAV decoder/format converter handles PCM WAV input without SDL_mixer.
/// No audio device or SDL audio subsystem initialization is required.
/// Failure leaves clip unchanged and provides a diagnostic if error is non-null.
bool load_audio_wav(const std::string& path, AudioClip& clip,
                    std::string* error = nullptr);

/// Portable little-endian stereo PCM16 RIFF/WAVE for deterministic diagnostics.
/// Nonfinite samples are written as silence; peaks saturate rather than wrap.
bool write_audio_wav(const std::string& path, const float* stereo,
                     std::size_t frames, int sample_rate,
                     std::string* error = nullptr);

}  // namespace fury
