# Bounded renderer frame sequences

`fury::AppConfig::capture_sequence_directory` records every actual rendered frame
to binary RGB PPM (P6), including the current HUD when `show_hud` is enabled.
Capture reads the active renderer after scene/HUD drawing and before presentation;
it does not change the selected backend or substitute a screenshot of a preview.
There is no image interpolation, synthetic motion, or offline re-render in this
capture path.

## API and output contract

```cpp
fury::AppConfig config;
config.max_frames = 48;
config.fixed_timestep = 1.f / 12.f;
config.capture_sequence_directory = "out/motion/frames";
config.capture_path = "out/motion/last.ppm"; // Optional, outside frames/.
```

- A sequence requires `max_frames > 0`. Both `init()` and `run()` reject an
  unbounded request. No sequence directory is created for that invalid request.
- The sequence destination must be new or empty. Parent directories are created
  as needed. A nonempty directory is rejected rather than mixing stale frames
  into a shorter rerun or replacing a previous sequence.
- Files start at `frame_000001.ppm`, continue in rendered order, and end at the
  actual number of rendered frames. Padding has at least six digits, increasing
  to the digit count of `max_frames` for runs over 999,999 frames.
- Each file contains tightly packed, top-left-origin RGB8 at that frame's real
  framebuffer dimensions. Capturing does not rescale or vertically flip it.
- The sequence bound and output destinations are fixed when `run()` starts.
  Changing those config fields from frame callbacks cannot turn recording into
  an unbounded disk-writing run. Other ordinary runtime config remains mutable.
- `capture_path` still records the last bounded frame. With a sequence, it must
  be outside the sequence directory, and its bytes match the last sequence
  image. It uses the same readback rather than requesting a second render.
- A normal early quit can produce fewer than `max_frames` images. No missing
  frames are fabricated, and `capture_path` is not written unless the bounded
  last frame was reached, preserving the previous behavior.
- Invalid paths, unavailable readback, invalid framebuffer dimensions, and
  directory/open/write/close errors are logged and return failure. Earlier
  successful images (and any partial failed file) remain available for diagnosis;
  failure does not silently skip an image and continue.
- PPM is uncompressed: estimate roughly `width * height * 3 * max_frames` bytes,
  plus small headers. A positive frame limit is a bound, not a disk-space
  guarantee. Choose a practical resolution/frame count and a fresh destination.

## Encode after capture

For the example's fixed simulation step of 1/12 second, an offline encode can use:

```sh
ffmpeg -framerate 12 -start_number 1 -i out/motion/frames/frame_%06d.ppm \
  -c:v libx264 -pix_fmt yuv420p out/motion/capture.mp4
```

Use a new output filename or resolve any existing-file prompt yourself. For
limits needing more than six digits, change the input pattern's width to match.
All input frames need matching dimensions for an ordinary video encode, so keep
the window size fixed during capture. YUV420 output also needs even dimensions.

Label the result **actual rendered frames, 12 fps simulated cadence, encoded
offline**. This is not evidence of 12 real-time rendered fps: capture I/O and
rendering may take much longer than the simulated timestep. Measure wall time
separately when reporting throughput. `fixed_timestep` controls simulation and
render time; it does not enforce real-time pacing. `freeze_render_time` freezes
shader animation time only and does not generally freeze gameplay updates.

## Regression checks

`fury_application_capture_tests` (`application_capture` in CTest) uses SDL's dummy
video driver. It covers new/empty and invalid destinations, unbounded requests,
mutable-config bounds, read/write failures, HUD inclusion, ordinary last-frame
capture, early quit, and deterministic fixed-step motion. On both software raster
and CPU ray backends it compares every PPM pixel against an independent live
renderer readback, requires geometry-driven frame differences, and checks that
CPU ray readback/presentation does not render the same frame twice.
