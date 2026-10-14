#include "fury/audio.hpp"
#include "fury/audio_mixer.hpp"
#include "fury/log.hpp"

#include <SDL.h>
#if defined(FURY_HAS_SDL_MIXER) && FURY_HAS_SDL_MIXER
#include <SDL_mixer.h>
#endif

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace fury;
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
void close(float actual, float expected, const char* message, float tolerance = 1e-5f) {
  require(std::isfinite(actual) && std::fabs(actual - expected) <= tolerance, message);
}
float peak(const std::vector<float>& samples) {
  float value = 0.f;
  for (float sample : samples) { require(std::isfinite(sample), "PCM remains finite"); value = std::max(value, std::fabs(sample)); }
  return value;
}
double rms(const std::vector<float>& samples) {
  double sum = 0;
  for (float value : samples) sum += static_cast<double>(value) * value;
  return samples.empty() ? 0.0 : std::sqrt(sum / samples.size());
}
struct Environment {
  std::string name, previous;
  bool existed;
  Environment(const char* key, const std::string& value) : name(key), existed(std::getenv(key) != nullptr) {
    if (existed) previous = std::getenv(key);
    SDL_setenv(key, value.c_str(), 1);
  }
  ~Environment() {
    if (existed) SDL_setenv(name.c_str(), previous.c_str(), 1);
    else {
#if defined(_WIN32)
      _putenv_s(name.c_str(), "");
#else
      unsetenv(name.c_str());
#endif
    }
  }
};
struct Scratch {
  std::filesystem::path path;
  Scratch() {
    path = std::filesystem::temp_directory_path() /
           ("fury-audio-test-" + std::to_string(SDL_GetPerformanceCounter()));
    std::filesystem::create_directories(path);
  }
  ~Scratch() { std::error_code error; std::filesystem::remove_all(path, error); }
};

void mixer_tests() {
  AudioClip ramp{8000, {0.f, 0.f, 0.5f, 0.25f, 1.f, 0.5f}};
  CpuAudioMixer mixer(16000);
  const auto id = mixer.play(ramp);
  require(id != 0 && mixer.playing(id), "Valid clip creates a voice");
  float output[16]{};
  mixer.render(output, 8);
  const float expected[] = {0, 0, .25f, .125f, .5f, .25f, .75f, .375f, 1, .5f, 1, .5f, 0, 0, 0, 0};
  for (unsigned i = 0; i < 16; ++i) close(output[i], expected[i], "Fractional resampling and one-shot tail");
  require(!mixer.playing(id) && mixer.active_voices() == 0, "Finished voices retire");
  require(CpuAudioMixer(-1).sample_rate() == 48000, "Invalid output rate uses safe default");
  AudioClip empty;
  require(mixer.play(empty) == 0, "Empty clip rejected");
  AudioClip odd{8000, {0.f}};
  require(mixer.play(odd) == 0, "Incomplete stereo frame rejected");
  AudioClip bad_rate{0, {0.f, 0.f}};
  require(mixer.play(bad_rate) == 0, "Invalid source rate rejected");
  require(mixer.play(ramp, 1, 0, false, 0) == 0, "Invalid pitch rejected");
  require(mixer.play(ramp, std::numeric_limits<float>::quiet_NaN()) == 0, "NaN gain rejected");
  require(mixer.play(ramp, 1, std::numeric_limits<float>::infinity()) == 0, "Infinite pan rejected");
  mixer.render(nullptr, 1);

  AudioClip constant{16000, {.6f, -.6f, .6f, -.6f}};
  auto first = mixer.play(constant, 1, 0, true);
  auto second = mixer.play(constant, 1, 0, true);
  mixer.render(output, 2);
  close(output[0], 1.f, "Positive sums saturate"); close(output[1], -1.f, "Negative sums saturate");
  mixer.set_master_gain(.5f);
  mixer.render(output, 1);
  close(output[0], .6f, "Master gain applied once after voice sum");
  mixer.stop(second);
  mixer.render(output, 1);
  close(output[0], .3f, "Voice stop and linear master gain");
  mixer.stop(first);
  mixer.set_master_gain(1);
  mixer.play(constant, .5f, -1, true);
  mixer.render(output, 1);
  close(output[0], .3f, "Voice gain applied independently"); close(output[1], 0, "Hard-left pan");
  mixer.stop_all();
  mixer.play(constant, 1, 1, true);
  mixer.render(output, 1);
  close(output[0], 0, "Hard-right pan"); close(output[1], -.6f, "Right channel retained");
  mixer.set_muted(true);
  mixer.render(output, 8);
  for (float value : output) close(value, 0, "Mute silences active voices immediately");
  mixer.set_muted(false);
  mixer.render(output, 1);
  close(output[1], -.6f, "Loop resumes after unmute");
  mixer.stop_all();
  const auto muted_shot = mixer.play(constant);
  mixer.set_muted(true);
  mixer.render(output, 8);
  require(!mixer.playing(muted_shot), "Muted one-shots advance and expire");
  mixer.set_muted(false);
  mixer.play(constant, 1, 0, true);
  mixer.set_master_gain(std::numeric_limits<float>::quiet_NaN());
  mixer.render(output, 1);
  close(output[0], 0, "NaN master becomes silence");
  mixer.stop_all();
  mixer.set_master_gain(1);
  AudioClip invalid_samples{16000, {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}};
  mixer.play(invalid_samples, 1, 0, true);
  mixer.render(output, 1);
  close(output[0], 0, "NaN PCM sanitized"); close(output[1], 0, "Infinite PCM sanitized");
  mixer.stop_all();

  AudioClip loop{8000, {0, 0, 1, 1}};
  CpuAudioMixer looping(16000);
  looping.play(loop, 1, 0, true);
  looping.render(output, 8);
  for (int frame = 0; frame < 8; ++frame)
    close(output[frame * 2], frame % 4 == 2 ? 1.f : (frame % 2 ? .5f : 0.f), "Loop boundary interpolation wraps");
  looping.stop_all();
  looping.play(loop, 1, 0, true, 8);
  looping.render(output, 8);
  for (float value : output) close(value, 0, "Multi-wrap sample step remains in bounds");

  const auto bed = mixer.play(constant, 1, 0, true);
  const auto oldest = mixer.play(constant);
  for (std::size_t i = 2; i < CpuAudioMixer::max_voices; ++i) mixer.play(constant);
  require(mixer.active_voices() == CpuAudioMixer::max_voices, "Bounded voice pool filled");
  const auto newest = mixer.play(constant);
  require(newest && !mixer.playing(oldest) && mixer.playing(bed), "Saturation steals oldest one-shot, preserves bed");
  mixer.stop(oldest);
  require(mixer.playing(newest), "Stale handle cannot stop replacement voice");
  mixer.stop_all();
  for (std::size_t i = 0; i < CpuAudioMixer::max_voices; ++i) mixer.play(constant, 1, 0, true);
  require(mixer.play(constant) == 0, "All-looping pool rejects instead of stealing ambience");

  // Render block size must not affect interpolation, loop phase, or gain.
  AudioClip waveform{22050, {.1f, -.1f, .8f, .3f, -.4f, .5f, -.8f, -.2f, 0, 0}};
  CpuAudioMixer whole(48000), pieces(48000);
  whole.play(waveform, .4f, .2f, true, .9f); pieces.play(waveform, .4f, .2f, true, .9f);
  std::vector<float> a(2000), b(2000);
  whole.render(a.data(), 1000);
  std::size_t offset = 0;
  for (std::size_t frames : {1u, 13u, 257u, 2u, 727u}) { pieces.render(b.data() + offset * 2, frames); offset += frames; }
  require(a == b, "Identical sample output across callback block sizes");
}

void wav_tests(const Scratch& scratch) {
  const auto path = (scratch.path / "roundtrip.wav").string();
  const float data[] = {0, .5f, -.5f, 2, -2, std::numeric_limits<float>::quiet_NaN()};
  std::string error;
  require(write_audio_wav(path, data, 3, 32000, &error), "Write PCM16 WAV");
  require(std::filesystem::file_size(path) == 56, "WAV header data length");
  AudioClip clip;
  require(load_audio_wav(path, clip, &error), "Load PCM16 WAV without opening a device");
  require(clip.sample_rate == 32000 && clip.frames() == 3, "WAV sample rate and channels preserved");
  const float expected[] = {0, .5f, -.5f, 1, -1, 0};
  for (unsigned i = 0; i < 6; ++i) close(clip.samples[i], expected[i], "WAV endian, saturation, sanitization", 5e-5f);
  const auto previous = clip.samples;
  require(!load_audio_wav((scratch.path / "absent.wav").string(), clip, &error) && !error.empty(), "Missing WAV diagnosed");
  require(previous == clip.samples, "Failed WAV load leaves clip unchanged");
  std::ofstream malformed(scratch.path / "malformed.wav", std::ios::binary);
  malformed << "RIFF-not-a-wave"; malformed.close();
  require(!load_audio_wav((scratch.path / "malformed.wav").string(), clip, &error), "Malformed WAV rejected");
  require(!write_audio_wav(path, nullptr, 1, 48000, &error), "Invalid WAV buffer rejected");
  require(!write_audio_wav(path, data, 3, 0, &error), "Invalid WAV rate rejected");
  require(!write_audio_wav(path, data, std::numeric_limits<std::size_t>::max(), 48000, &error), "RIFF overflow rejected");
}

std::vector<float> cue_audio(const char* cue, float volume = 1.f) {
  auto audio = create_offline_audio();
  require(audio->init(), "Offline audio initializes");
  audio->set_master_volume(volume);
  audio->play_cue(cue);
  std::vector<float> result(48000 * 2);
  audio->render(result.data(), 48000);
  return result;
}

void facade_tests(const std::string& asset_dir) {
  Environment assets("FURY_AUDIO_ASSET_DIR", asset_dir);
  AudioClip authored;
  require(load_audio_wav(asset_dir + "/radio_blip.wav", authored), "Authored Meridian WAV bank present");
  require(authored.sample_rate == 22050 && authored.frames() == 13230, "Authored mono WAV decoded to stereo");
  auto audio = create_offline_audio();
  require(audio->init() && audio->init(), "Offline init idempotent");
  require(std::string(audio->backend_name()) == "cpu_offline", "Offline backend accurately identified");
  std::vector<float> buffer(96000);
  audio->play_cue(nullptr); audio->play_cue(""); audio->play_cue("unknown-cue");
  audio->render(buffer.data(), 48000);
  require(peak(buffer) == 0.f, "No cue/unknown cue produces silence");
  const auto full = cue_audio("radio_blip"), half = cue_audio("radio_blip", .5f);
  require(peak(full) > .001f, "Authored cue produces nonzero PCM");
  require(full == cue_audio("radio_tick"), "Authored radio alias preserved");
  require(cue_audio("siren") == cue_audio("alarm_klaxon"), "Authored siren alias preserved");
  for (std::size_t i = 0; i < full.size(); ++i) close(half[i], full[i] * .5f, "Facade volume is linear, applied once");
  for (const char* cue : {"footstep", "heist_breach", "impact", "heist_start", "heist_success", "heist_fail",
                          "thunder", "complication", "enforcer_spawn", "phone_ring", "printer", "vault_motor",
                          "metal_stress", "police_radio", "zone_lobby", "zone_security", "zone_vault", "zone_alley"})
    require(peak(cue_audio(cue)) > .0001f, "All established cue families render audio");

  auto untouched = create_offline_audio(); untouched->init();
  audio->play_cue("zone_lobby"); untouched->play_cue("zone_lobby");
  audio->render(buffer.data(), 48000);
  std::vector<float> reference(96000);
  untouched->render(reference.data(), 48000);
  audio->play_cue("zone_lobby");
  audio->render(buffer.data(), 48000); untouched->render(reference.data(), 48000);
  require(buffer == reference, "Repeated bed cue preserves loop phase");
  audio->set_muted(true); audio->play_cue("zone_vault");
  audio->render(buffer.data(), 48000); require(peak(buffer) == 0.f, "Muted bed change stays silent");
  audio->toggle_mute(); audio->render(buffer.data(), 48000);
  require(peak(buffer) > .001f, "Changed bed survives unmute");
  audio->set_master_volume(std::numeric_limits<float>::infinity());
  require(audio->master_volume() == 0.f, "Facade nonfinite gain sanitized");
  audio->set_ambience(-1, 2, std::numeric_limits<float>::quiet_NaN());
  require(audio->ambience_day() == 0 && audio->ambience_night() == 1 && audio->ambience_rain() == 0, "Ambience control bounds");
  audio->shutdown(); audio->shutdown();
  audio->render(buffer.data(), 48000); require(peak(buffer) == 0.f, "Shutdown retires all voices");
  require(audio->init(), "Offline backend reinitializes");
  audio->set_master_volume(1); audio->set_music_intensity(1);
  audio->update(std::numeric_limits<float>::quiet_NaN());
  audio->update(std::numeric_limits<float>::infinity());
  audio->render(buffer.data(), 48000); require(peak(buffer) == 0.f, "Invalid dt does not schedule music");
  audio->update(.2f); audio->render(buffer.data(), 48000);
  require(peak(buffer) > .0001f, "Intensity schedules procedural music");
  {
    Environment no_assets("FURY_AUDIO_ASSET_DIR", asset_dir + "/missing");
    require(peak(cue_audio("footstep")) > .001f, "Missing authored bank retains procedural fallback");
  }
}

void device_tests(const std::string& asset_dir) {
  Environment assets("FURY_AUDIO_ASSET_DIR", asset_dir);
  Environment driver("SDL_AUDIODRIVER", "dummy");
  require(SDL_InitSubSystem(SDL_INIT_AUDIO) == 0, "Dummy audio subsystem initializes");
  {
    auto audio = create_cpu_audio();
    require(audio->init() && audio->init(), "CPU output init idempotent");
    require(std::string(audio->backend_name()) == "sdl_cpu", "SDL2 CPU device actually opens");
    audio->play_cue("zone_lobby"); audio->play_cue("heist_start");
    SDL_Delay(40); // Exercise the SDL callback, not evidence of audible hardware playback.
    for (int i = 0; i < 40; ++i) {
      audio->set_master_volume((i % 10) / 10.f); audio->toggle_mute();
      audio->set_ambience(1, 0, .2f); audio->set_music_intensity(.9f);
      audio->play_cue("footstep"); audio->update(.03f);
    }
    audio->shutdown(); audio->shutdown();
    require(SDL_WasInit(SDL_INIT_AUDIO) != 0, "Shutdown retains caller-owned SDL subsystem reference");
    require(audio->init(), "Realtime backend reinitializes after shutdown");
  }
  require(SDL_WasInit(SDL_INIT_AUDIO) != 0, "Destructor releases only backend-owned SDL reference");
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
  require(SDL_WasInit(SDL_INIT_AUDIO) == 0, "No leaked SDL subsystem reference");
  {
    Environment selection("FURY_AUDIO_BACKEND", "cpu");
    auto selected = create_audio(); selected->init();
    require(std::string(selected->backend_name()) == "sdl_cpu", "Runtime CPU selection");
  }
  {
    Environment selection("FURY_AUDIO_BACKEND", "null");
    auto selected = create_audio(); selected->init();
    require(std::string(selected->backend_name()) == "null", "Explicit silent backend retained");
  }
  {
    Environment unavailable("SDL_AUDIODRIVER", "fury-invalid-test-driver");
    auto silent = create_cpu_audio();
    require(silent->init(), "Unavailable device leaves application usable");
    require(std::string(silent->backend_name()) == "sdl_cpu(silent)", "Unavailable device accurately diagnosed");
    silent->play_cue("impact"); silent->shutdown(); silent->shutdown();
  }
  require(SDL_WasInit(SDL_INIT_AUDIO) == 0, "Failure path leaks no SDL reference");
}

#if defined(FURY_HAS_SDL_MIXER) && FURY_HAS_SDL_MIXER
struct MixerCapture {
  ~MixerCapture() { if (Mix_QuerySpec(nullptr, nullptr, nullptr)) Mix_SetPostMix(nullptr, nullptr); }
  std::atomic<std::size_t> samples{0};
  std::atomic<int> peak{0};
  static void postmix(void* user, Uint8* stream, int length) {
    auto& capture = *static_cast<MixerCapture*>(user);
    const auto* pcm = reinterpret_cast<const Sint16*>(stream);
    int peak = 0;
    for (int i = 0; i < length / static_cast<int>(sizeof(Sint16)); ++i)
      peak = std::max(peak, std::abs(static_cast<int>(pcm[i])));
    capture.samples.fetch_add(static_cast<std::size_t>(length) / sizeof(Sint16));
    int previous = capture.peak.load();
    while (peak > previous && !capture.peak.compare_exchange_weak(previous, peak)) {}
  }
  void reset() {
    Mix_SetPostMix(nullptr, nullptr);  // Joins the previous callback before resetting.
    samples.store(0); peak.store(0);
    Mix_SetPostMix(&postmix, this);
  }
};
void mixer_device_tests(const std::string& asset_dir) {
  Environment assets("FURY_AUDIO_ASSET_DIR", asset_dir);
  Environment driver("SDL_AUDIODRIVER", "dummy");
  Environment selection("FURY_AUDIO_BACKEND", "mixer");
  require(SDL_InitSubSystem(SDL_INIT_AUDIO) == 0, "Caller initializes SDL before optional mixer");
  {
    auto audio = create_audio();
    require(audio->init() && audio->init(), "Optional SDL_mixer init idempotent");
    require(std::string(audio->backend_name()) == "sdl_mixer", "Optional backend selection opens mixer");
    int rate = 0, channels = 0;
    Uint16 format = 0;
    require(Mix_QuerySpec(&rate, &format, &channels) != 0 && rate == 22050 &&
            channels == 1 && format == AUDIO_S16SYS, "Optional mixer preserves established device format");
    MixerCapture capture;
    capture.reset();
    audio->play_cue("heist_start");
    SDL_Delay(180);
    require(capture.samples.load() > 0 && capture.peak.load() > 0, "SDL_mixer callback produces procedural PCM");
    capture.reset();
    audio->play_cue("zone_lobby"); audio->play_cue("radio_tick");
    SDL_Delay(120);
    require(capture.peak.load() > 0 && Mix_Playing(-1) > 0, "Optional authored bank and aliases play");
    audio->set_muted(true);
    capture.reset(); SDL_Delay(120);
    require(capture.samples.load() > 0 && capture.peak.load() == 0, "Optional mixer mute silences active bed");
    audio->toggle_mute();
    capture.reset(); SDL_Delay(120);
    require(capture.peak.load() > 0, "Optional mixer bed resumes after unmute");
    audio->set_master_volume(0);
    capture.reset(); SDL_Delay(120);
    require(capture.samples.load() > 0 && capture.peak.load() == 0, "Optional mixer master zero silences active voices");
    Mix_SetPostMix(nullptr, nullptr);
    audio->shutdown(); audio->shutdown();
    require(Mix_QuerySpec(nullptr, nullptr, nullptr) == 0, "Optional mixer repeated init did not leak open count");
    require(SDL_WasInit(SDL_INIT_AUDIO) != 0, "Optional mixer shutdown retains caller SDL reference");
    require(audio->init(), "Optional mixer reinitializes");
    audio->play_cue("zone_vault");
  }
  require(Mix_QuerySpec(nullptr, nullptr, nullptr) == 0, "Optional mixer destructor closes active output");
  require(SDL_WasInit(SDL_INIT_AUDIO) != 0, "Optional mixer destructor retains caller SDL reference");
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
  require(SDL_WasInit(SDL_INIT_AUDIO) == 0, "Optional mixer leaks no SDL reference");
  {
    Environment unavailable("SDL_AUDIODRIVER", "fury-invalid-test-driver");
    auto silent = create_audio();
    require(silent->init(), "Unavailable mixer leaves application usable");
    require(std::string(silent->backend_name()) == "sdl_mixer(silent)", "Unavailable mixer accurately diagnosed");
    silent->shutdown(); silent->shutdown();
  }
  require(SDL_WasInit(SDL_INIT_AUDIO) == 0, "Optional mixer failure leaks no SDL reference");
}
#endif

void preview(const std::string& path, const std::string& asset_dir) {
  Environment assets("FURY_AUDIO_ASSET_DIR", asset_dir);
  auto audio = create_offline_audio(48000); audio->init(); audio->set_master_volume(.65f);
  std::vector<float> pcm(48000 * 8 * 2);
  for (std::size_t tick = 0; tick < 8 * 60; ++tick) {
    if (tick == 0) { audio->play_cue("zone_lobby"); audio->play_cue("heist_start"); }
    if (tick % 30 == 15) audio->play_cue("footstep");
    if (tick == 60) audio->play_cue("radio_tick");
    if (tick == 120) { audio->play_cue("zone_security"); audio->play_cue("phone_ring"); }
    if (tick == 180) { audio->play_cue("heist_breach"); audio->play_cue("vault_motor"); }
    if (tick == 240) { audio->play_cue("zone_vault"); audio->play_cue("metal_stress"); }
    if (tick == 300) { audio->set_music_intensity(.9f); audio->play_cue("alarm_klaxon"); }
    if (tick == 360) { audio->play_cue("zone_alley"); audio->play_cue("thunder"); }
    if (tick == 420) audio->play_cue("heist_success");
    audio->update(1.f / 60.f);
    audio->render(pcm.data() + tick * 800 * 2, 800);
  }
  std::size_t saturated_frames = 0;
  for (std::size_t i = 0; i < pcm.size(); i += 2)
    if (std::fabs(pcm[i]) >= 1.f || std::fabs(pcm[i + 1]) >= 1.f) ++saturated_frames;
  require(saturated_frames == 0, "Preview scene retains headroom without saturated frames");
  std::string error;
  if (!write_audio_wav(path, pcm.data(), pcm.size() / 2, audio->sample_rate(), &error))
    throw std::runtime_error(error);
  std::cout << "Offline audio: " << path << ", stereo PCM16, 48000 Hz, 8 seconds, peak="
            << peak(pcm) << ", RMS=" << rms(pcm) << ", saturated_frames=" << saturated_frames << '\n';
}
}  // namespace

int main(int argc, char** argv) {
  try {
    Log::set_level(LogLevel::Warn);
#ifdef FURY_TEST_AUDIO_ASSETS
    const std::string asset_dir = FURY_TEST_AUDIO_ASSETS;
#else
    const std::string asset_dir = (std::filesystem::path(__FILE__).parent_path().parent_path() /
                                   "assets/audio/meridian").string();
#endif
    if (argc != 1 && (argc != 3 || std::string(argv[1]) != "--write-wav")) {
      std::cerr << "Usage: fury_audio_tests [--write-wav output.wav]\n"; return 2;
    }
    Scratch scratch;
    mixer_tests(); wav_tests(scratch); facade_tests(asset_dir); device_tests(asset_dir);
#if defined(FURY_HAS_SDL_MIXER) && FURY_HAS_SDL_MIXER
    mixer_device_tests(asset_dir);
    std::cout << "Optional SDL_mixer procedural/authored PCM, mute, gain and lifecycle checks passed\n";
#endif
    if (argc == 3) preview(argv[2], asset_dir);
    std::cout << "CPU mixing, WAV conversion, authored cues, offline rendering, and SDL dummy lifecycle checks passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Audio regression: " << error.what() << '\n'; return 1;
  }
}
