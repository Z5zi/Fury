#include "fury/audio_mixer.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>

namespace fury {
namespace {
float finite_sample(float value) { return std::isfinite(value) ? value : 0.f; }
float unit(float value) { return std::clamp(finite_sample(value), 0.f, 1.f); }
bool fail(std::string* error, const std::string& message) {
  if (error) *error = message;
  return false;
}
}  // namespace

CpuAudioMixer::CpuAudioMixer(int output_sample_rate)
    : m_sample_rate(output_sample_rate >= 8000 && output_sample_rate <= 192000
                        ? output_sample_rate : 48000) {}

CpuAudioMixer::VoiceId CpuAudioMixer::play(const AudioClip& clip, float gain,
                                          float pan, bool loop, float pitch) {
  if (clip.samples.empty() || clip.samples.size() % 2 || clip.sample_rate <= 0 ||
      clip.sample_rate > 384000 || !std::isfinite(gain) || !std::isfinite(pan) ||
      !std::isfinite(pitch) || pitch < 0.125f || pitch > 8.f) return 0;
  Voice* selected = nullptr;
  for (auto& voice : m_voices) {
    if (!voice.clip) { selected = &voice; break; }
    if (!voice.loop && (!selected || voice.id < selected->id)) selected = &voice;
  }
  if (!selected) return 0;
  pan = std::clamp(pan, -1.f, 1.f);
  const float amplitude = unit(gain);
  *selected = Voice{&clip, m_next_id++, 0.0,
                    static_cast<double>(clip.sample_rate) * pitch / m_sample_rate,
                    amplitude * (pan > 0.f ? 1.f - pan : 1.f),
                    amplitude * (pan < 0.f ? 1.f + pan : 1.f), loop};
  if (m_next_id == 0) m_next_id = 1;
  return selected->id;
}

void CpuAudioMixer::stop(VoiceId id) {
  for (auto& voice : m_voices) if (voice.id == id) voice = Voice{};
}
void CpuAudioMixer::stop_all() { for (auto& voice : m_voices) voice = Voice{}; }
bool CpuAudioMixer::playing(VoiceId id) const {
  for (const auto& voice : m_voices) if (voice.clip && voice.id == id) return true;
  return false;
}
std::size_t CpuAudioMixer::active_voices() const {
  std::size_t count = 0;
  for (const auto& voice : m_voices) if (voice.clip) ++count;
  return count;
}
void CpuAudioMixer::set_master_gain(float gain) { m_master = unit(gain); }

void CpuAudioMixer::render(float* stereo, std::size_t frames) {
  if (!stereo || frames > std::numeric_limits<std::size_t>::max() / 2) return;
  std::fill_n(stereo, frames * 2, 0.f);
  for (auto& voice : m_voices) {
    if (!voice.clip) continue;
    const auto& data = voice.clip->samples;
    const auto length = voice.clip->frames();
    for (std::size_t frame = 0; frame < frames; ++frame) {
      const auto index = static_cast<std::size_t>(voice.position);
      const auto next = index + 1 < length ? index + 1 : (voice.loop ? 0 : index);
      const auto fraction = static_cast<float>(voice.position - index);
      const float left = finite_sample(data[index * 2]);
      const float right = finite_sample(data[index * 2 + 1]);
      stereo[frame * 2] += (left + (finite_sample(data[next * 2]) - left) * fraction) * voice.left;
      stereo[frame * 2 + 1] += (right + (finite_sample(data[next * 2 + 1]) - right) * fraction) * voice.right;
      voice.position += voice.step;
      if (voice.position >= static_cast<double>(length)) {
        if (voice.loop) voice.position = std::fmod(voice.position, static_cast<double>(length));
        else { voice = Voice{}; break; }
      }
    }
  }
  const float gain = m_muted ? 0.f : m_master;
  for (std::size_t i = 0; i < frames * 2; ++i)
    stereo[i] = std::clamp(finite_sample(stereo[i] * gain), -1.f, 1.f);
}

bool load_audio_wav(const std::string& path, AudioClip& clip, std::string* error) {
  SDL_AudioSpec spec{};
  Uint8* bytes = nullptr;
  Uint32 length = 0;
  if (!SDL_LoadWAV(path.c_str(), &spec, &bytes, &length)) return fail(error, SDL_GetError());
  // SDL owns the source buffer; conversion and copies finish before releasing it.
  struct WavOwner { Uint8* data; ~WavOwner() { SDL_FreeWAV(data); } } owner{bytes};
  if (spec.freq <= 0 || spec.freq > 384000 || spec.channels == 0 || length == 0)
    return fail(error, "Empty or unsupported WAV format");
  const unsigned source_frame_bytes = spec.channels * (SDL_AUDIO_BITSIZE(spec.format) / 8);
  if (!source_frame_bytes || length % source_frame_bytes)
    return fail(error, "WAV has an incomplete PCM frame");
  SDL_AudioCVT cvt{};
  if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq,
                       AUDIO_F32SYS, 2, spec.freq) < 0) return fail(error, SDL_GetError());
  if (length > static_cast<Uint32>(std::numeric_limits<int>::max()) || cvt.len_mult <= 0 ||
      length > static_cast<std::size_t>(std::numeric_limits<int>::max()) / cvt.len_mult)
    return fail(error, "WAV is too large to convert");
  std::vector<Uint8> converted(static_cast<std::size_t>(length) * cvt.len_mult);
  std::memcpy(converted.data(), bytes, length);
  cvt.buf = converted.data();
  cvt.len = static_cast<int>(length);
  if (SDL_ConvertAudio(&cvt) < 0) return fail(error, SDL_GetError());
  if (cvt.len_cvt <= 0 || cvt.len_cvt % (2 * sizeof(float)))
    return fail(error, "WAV conversion did not produce complete stereo frames");
  AudioClip result;
  result.sample_rate = spec.freq;
  result.samples.resize(static_cast<std::size_t>(cvt.len_cvt) / sizeof(float));
  std::memcpy(result.samples.data(), converted.data(), static_cast<std::size_t>(cvt.len_cvt));
  for (auto& value : result.samples) value = std::clamp(finite_sample(value), -1.f, 1.f);
  clip = std::move(result);
  if (error) error->clear();
  return true;
}

bool write_audio_wav(const std::string& path, const float* stereo,
                     std::size_t frames, int sample_rate, std::string* error) {
  if ((!stereo && frames) || sample_rate <= 0 || sample_rate > 384000 ||
      frames > (std::numeric_limits<std::uint32_t>::max() - 36) / 4)
    return fail(error, "Invalid PCM buffer, sample rate, or RIFF size");
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) return fail(error, "Cannot open WAV output: " + path);
  auto put16 = [&](std::uint16_t value) {
    const char bytes[] = {static_cast<char>(value), static_cast<char>(value >> 8)};
    out.write(bytes, 2);
  };
  auto put32 = [&](std::uint32_t value) {
    const char bytes[] = {static_cast<char>(value), static_cast<char>(value >> 8),
                          static_cast<char>(value >> 16), static_cast<char>(value >> 24)};
    out.write(bytes, 4);
  };
  const auto size = static_cast<std::uint32_t>(frames * 4);
  out.write("RIFF", 4); put32(36 + size); out.write("WAVEfmt ", 8); put32(16);
  put16(1); put16(2); put32(static_cast<std::uint32_t>(sample_rate));
  put32(static_cast<std::uint32_t>(sample_rate * 4)); put16(4); put16(16);
  out.write("data", 4); put32(size);
  for (std::size_t i = 0; i < frames * 2; ++i) {
    const float value = std::clamp(finite_sample(stereo[i]), -1.f, 1.f);
    const auto pcm = static_cast<std::int16_t>(std::lround(value * 32767.f));
    put16(static_cast<std::uint16_t>(pcm));
  }
  out.close();
  if (!out) return fail(error, "Failed to write WAV output: " + path);
  if (error) error->clear();
  return true;
}
}  // namespace fury
