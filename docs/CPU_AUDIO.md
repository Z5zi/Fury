# CPU audio and offline validation

Fury can now play its authored Meridian audio and procedural cues with SDL2 alone.
SDL_mixer remains optional. This changes the no-SDL_mixer build from a silent null
backend into actual CPU mixing and SDL audio output.

## Playback selection

- Default: SDL_mixer when compiled in; otherwise the SDL2 CPU mixer
- `FURY_AUDIO_BACKEND=cpu`: explicitly select the SDL2 CPU path
- `FURY_AUDIO_BACKEND=null`: explicitly silent, cue-logging backend
- `FURY_AUDIO_BACKEND=mixer`: select SDL_mixer if compiled in; otherwise CPU
- `FURY_AUDIO_ASSET_DIR=/path/to/assets/audio/meridian`: override the authored bank

Without an override, asset lookup checks the executable's `assets/audio/meridian`
directory, followed by the existing common working-directory layouts. Missing or
invalid authored cues retain the existing procedural fallback where one exists.
Zone beds and authored-only effects cannot play when their WAV files are absent.
An unavailable audio device leaves the game usable and reports
`sdl_cpu(silent)` rather than claiming successful output.

## CPU playback implementation

- Float stereo mixing with 32 bounded voices at 48 kHz by default. SDL may
  negotiate a different device rate; source WAVs retain their original rate
- Linear interpolation for source-rate conversion, including interpolation across
  loop boundaries. Pitch control is available in `CpuAudioMixer`
- Independent per-voice gain and stereo balance; master/ambience gain applied once
  after summing voices. Summed peaks saturate in `[-1,1]`
- Authored WAV loading through SDL's WAV decoder and PCM converter, without any
  SDL_mixer dependency. Original mono Meridian cues become stereo
- A zone bed loops continuously; repeating the same zone cue preserves its phase,
  while a zone change replaces the old bed. Hum/traffic aliases have independent
  voice gains, so they do not change the underlying bed's volume
- Voice pressure steals the oldest one-shot, preserving looping beds. If every
  voice is looping, a new voice is rejected
- Mute immediately silences existing voices while advancing sample time. New
  one-shots are suppressed while muted; zone changes remain current on unmute
- Audio callbacks perform no heap allocation, file I/O or logging. The application
  uses SDL device locking to change mixer state; clips remain immutable and alive
  until the device callback has stopped
- Shutdown is idempotent, stops the device before destroying samples and releases
  only the SDL audio subsystem reference acquired by this backend

The existing `Audio` interface and cue names are unchanged. The optional
SDL_mixer path also gains destructor cleanup, repeated-init protection, owned SDL
subsystem accounting and safe little-endian procedural WAV encoding.

Linear interpolation is a lightweight resampler, not a band-limited mastering
resampler. Hard limiting prevents overflow but can distort heavily overlapping
loud cues. These choices keep the path small and dependency-light.

## Deterministic offline output

`create_offline_audio(sample_rate)` provides the same cue bank and mixer without
opening an audio device. Set controls and play cues through the existing `Audio`
API, call `update(dt)` to schedule music, then call `render(stereo, frames)` to
advance sample time. The output is interleaved float stereo. The standalone
`CpuAudioMixer` also supports custom immutable `AudioClip` inputs.

`write_audio_wav` writes portable stereo PCM16 WAV files with validated sizes,
saturated peaks and nonfinite samples replaced by silence. `load_audio_wav`
preserves the destination on failure and supplies a diagnostic string.

Build with tests enabled, then run:

```sh
ctest --test-dir build --output-on-failure -R audio_cpu
./build/tests/fury_audio_tests --write-wav /tmp/fury-cpu-audio.wav
```

The diagnostic runs the suite and writes an eight-second scene containing zone
changes, footsteps, radio, breach/vault effects, an alarm, thunder, music and a
success cue. Its scene master is 0.65 to retain headroom; the diagnostic prints
the float mix's peak, RMS and saturated-frame count and requires zero saturated
frames. The render portion opens no
audio device; the lifecycle tests explicitly select SDL's dummy driver. The
expected invalid-driver test emits a warning while checking the failure path.

Audio generation is repeatable within a build: the test also proves identical
sample output across different render block sizes. Floating-point/libm behavior
can differ between architectures, so this is not a cross-platform bit-identical
codec contract.

## What validation proves

Regression coverage checks fractional resampling, loop wrap and high-rate steps,
voice stealing and stale handles, gain linearity, pan, clipping, mute/unmute,
nonfinite controls/PCM, malformed/missing WAVs, PCM16 round trips, established
cue families and aliases, missing-asset fallback, repeated bed notifications,
offline reinitialization, music scheduling and SDL ownership/cleanup. When built
with SDL_mixer, the suite additionally captures its postmix output to verify
nonzero procedural/authored PCM, mute/unmute, zero master gain, repeated init,
destructor cleanup and failure-path ownership.

The dummy driver verifies callback execution and lifecycle without physical
audio hardware. Offline nonzero PCM and a valid WAV file establish that mixing
works; neither establishes audible playback, speaker routing, latency or listening
quality on a user's machine. Those require a real output device and a listening
check.
