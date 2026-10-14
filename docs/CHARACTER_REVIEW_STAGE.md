# Character review stage

`fury_characterlab` is a **diagnostic engine stage**, not a Vaultline world,
gameplay capture, or proof of live NPC presentation. It uses the production Fury
`Application`, renderer, `make_character_profile`, character surface material,
and skeletal pose functions. Pair its images with separate actual-game captures
when reviewing a character release.

Build the `fury_characterlab` target with `FURY_BUILD_CHARACTERLAB=ON`. The binary
is `build/apps/characterlab/fury_characterlab` (platform/configuration suffixes
may apply). `--help` prints all controls.

## Stable comparison setup

- Default actor: identity `NpcCivA`, role `Commuter`, height **1.75 meters**, near
  LOD, bind pose. `--height` changes actor height; `--image-height` changes pixels.
  `--role` accepts enum names such as `CrewScout` as well as `crew_scout`.
- Profile geometry and individualized profile materials come from the current
  engine. Other identities/roles use the existing generic seeded character with
  its prior production material: white albedo, roughness 0.72, metallic zero,
  and no texture maps. Generic meshes use full-range UVs, so applying the
  character atlas to them would misrepresent the baseline. `--legacy-profile` explicitly bypasses individual
  profiles and uses `make_character_model`, the articulated generic baseline.
  This flag does not mean the older box-based `make_humanoid` mesh.
- Actor orientation is fixed: +Y up, +Z forward. Its origin is always at world
  `(0, height/2, 0)` above the same neutral ground. Poses are never lifted or
  reframed to hide intersections. Walking/running samples animate in place;
  this is not a foot-sliding or gameplay navigation test.
- Lighting is fixed in world space for every pose, side, material, and LOD: a
  white front/top/+X-side key, neutral ambient, no point lights, no bloom, no
  reflections, and no visible fog. The key does not follow the camera. The back
  is deliberately seen under the same rig, rather than individually relit.
- `--vertex-material` keeps the exact profile geometry and vertex colors but
  substitutes white reflectance, roughness0.72 and a nonmetal material. For masked hair, it retains only the production base-map alpha and cutoff so card visibility stays identical.
  This labeled diagnostic isolates material response from anatomy. It cannot
  combine with `--clay`; it does not change production game materials.
- `--clay` removes atlas reflectance/normal/roughness detail and vertex color on the diagnostic copy, while preserving production alpha masks and their mips. It
  retains geometry, normals, rig and pose, with uniform neutral rough material.
  Compare it with the actual material at the same angle to separate form from
  surface detail. Clay is explicitly an override, not a production material.
- Body framing has a fixed height-relative target and distance. `--face` selects
  a fixed head framing, independent of profile and pose. `--distance` overrides
  the orbit radius in meters. The camera is reset each frame; mouse/keyboard
  input cannot change a review angle. Very narrow images receive a fixed aspect
  adjustment. Keep dimensions identical for before/after comparisons.
- No HUD is drawn, including in interactive use. `--no-hud` is accepted for
  shared capture scripts. Escape/window close ends interactive use.

## All-side body and face captures

The default backend is OpenGL, with the engine's normal logged software fallback
if unavailable. `--soft` explicitly requests software raster; `--cpu-ray` requests
the production CPU ray/path renderer. The JSON report names the actual backend.
Choose one backend and keep its environment settings identical across a set.
`--spp 1..64` and `--bounces 1..16` configure the CPU ray renderer after startup
(defaults: 1 sample per pixel per frame, 4 maximum bounces). These options do not
change raster rendering. Reports record the actual renderer settings as `spp`
and `bounces`, or `null` for a raster backend, plus the actual `trace_mode`.

For headless software capture on Linux, use `SDL_VIDEODRIVER=dummy`. OpenGL
requires a display; use the normal display or `xvfb-run` where available.

```sh
SDL_VIDEODRIVER=dummy build/apps/characterlab/fury_characterlab \
  --soft --id NpcCivA --role Commuter --height 1.75 \
  --width 720 --image-height 960 --pose bind --yaw 0 --elevation 0 \
  --frames 1 --capture out/character-stage/front.ppm \
  --report out/character-stage/front.json --no-hud
```

Repeat with the same flags and unique output names for these yaw angles:

| View | Camera yaw | World side |
| --- | ---: | --- |
| Front | 0 | +Z |
| Front three-quarter | 45 | +X, +Z |
| Right rig side | 90 | +X |
| Rear three-quarter | 135 | +X, -Z |
| Back | 180 | -Z |
| Opposite rear three-quarter | -135 | -X, -Z |
| Left rig side | -90 | -X |
| Opposite three-quarter | -45 | -X, +Z |

Side names refer to this engine's rig naming. Prefer the world-axis labels if
discussing anatomical left/right could be ambiguous. Keep elevation at zero for
the principal comparisons; add a separately labeled `--elevation 20` view when
top form or shoulder construction needs examination. Elevation is restricted to
the open interval (-90, 90) to keep the camera up vector valid.

Repeat the set with `--face`, then with `--clay`. Repeat with
`--legacy-profile` for the prior articulated baseline, or `--lod far` to examine
the real far mesh. Do not silently change camera/lighting for an individual pose
or baseline. Fixed face framing may crop a crouching head; the body view is the
appropriate fixed-camera comparison for that pose.

## Deterministic pose and motion captures

`--pose` accepts `bind`, `idle`, `walk`, `run`, `talk`, `crouch`, or `crouch-walk`.
`--phase` is normalized to [0,1). It is the initial gait-cycle phase; for idle
and talking motion the same value is also the initial time offset in seconds.
Walking uses speed `height * 0.85` meters/second; running uses `height * 2.6`.
The engine computes stride length from that speed and height. `crouch-walk` uses
the existing crouched gait at `height * 0.55` m/s; it does not add a new gameplay animation.

`--attention-yaw -0.9..0.9` and `--attention-pitch -0.4..0.4` take **radians**,
matching the existing animation limits. Nonzero values enable attention for any
non-bind pose, including bounded idle/talk stills. Bind geometry stays literal
and rejects nonzero attention. Omitted or explicit zero values preserve the
previous pose state and pixels. Nonfinite and out-of-range inputs are rejected.
Reports include both angles and attention weight. These controls only sample the
existing diagnostic rig and do not manufacture dialogue input.

Bounded stills hold exactly the requested sample for every frame. Thus
`--cpu-ray --spp 16 --bounces 4 --frames 8 --capture ...` can accumulate a
stationary pose with explicit sampling settings. A sequence
automatically advances the sample: frame 1 is at time zero; frame N is at
`(N - 1) / fps`, with gait phase increasing by `time * speed / stride_length`.
`--animate` also enables progression without a sequence. Interactive runs
advance with measured elapsed time. Bind geometry remains stationary in all
these modes.

```sh
SDL_VIDEODRIVER=dummy build/apps/characterlab/fury_characterlab \
  --soft --pose walk --phase 0 --yaw 45 --width 480 --image-height 640 \
  --frames 24 --fps 12 --capture-sequence out/character-walk/frames \
  --capture out/character-walk/last.ppm --report out/character-walk/report.json
```

Captures use the existing bounded `AppConfig` path, with no offline re-render,
interpolated images, or separate preview renderer. See [FRAME_CAPTURE.md](FRAME_CAPTURE.md)
for the PPM format, naming, failure behavior, disk use, and encoding commands.
The frame-sequence directory must be new or empty; the last-frame capture and
JSON report must be outside it, with distinct paths. Capture flags require a
positive `--frames`. `--fps` accepts 0.1 through 240. The simulated FPS
does not claim real-time rendering throughput.

The optional `--report` JSON includes diagnostic provenance, source fingerprint,
actual backend and ray sampling settings, profile/LOD/material identity, material
roughness/metallic/map presence, vertex and triangle counts,
initial/final animation sample, camera, fixed light direction, final posed world
bounds, requested/submitted frame counts, wall time, and renderer validation
errors. A bounded run interrupted before its frame limit returns failure.
Reports and captures are files on the caller's selected paths; use unique report
and still filenames when preserving a comparison set.
