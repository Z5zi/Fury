# Rina Holt: individual character review

## Result

Rina now has a separately authored, source-correspondent anatomical head and
hands, socket-fitted eyes, a complete close crop, sewn plum shirt, lined orange
work vest, connected trousers with the original left cargo pocket, and shaped
brown footwear. Identity `NpcCivE`, Dock role, 1.68 m height, deterministic
palette/appearance controls, spawn, route, schedule, speed, radius, dialogue and
gameplay remain unchanged. The study edits no other actor's model.

The actor is a substantial improvement over the generic baseline. It is not
AAA parity. Hair remains cap-like, eyes and fingers are static, materials lack
individual scanned skin/cloth detail, and there is no cloth/hair simulation.
The actual-world route still crosses pre-existing buildings and props; see
[the specific preserved limitation](WORLD_ROUTE_LIMITATION.md).

## Evidence and provenance

- Private exact source base: `9e21e9a1aae79fb7abd7a9cf3e177349581ef14d`
- Final runtime fingerprint: `b2234ce8a46584c2cf0d18fc7447bb0751537c1b60c476a3da80e618c569c992`
- Final source-input manifest verifies 158 runtime paths; 150 unchanged paths
  match the exact base byte for byte
- Independent field exports prove all 34 non-Rina actor/LOD cases unchanged,
  including bind geometry, rig, influences, palette and three sampled poses
- CC0 MPFB graphical data only, pinned to the documented upstream commit and
  21 exact byte/Git-blob hashes. No upstream addon code or proprietary reference
  pixels, geometry, textures, logos or character likenesses are shipped
- The conservative dedicated-work total is **2h 01m 54s**, as documented in
  [the work ledger](WORK_LOG.md)
  with packaging, recovery gaps, waits and benchmark coordination excluded.
  Elapsed time is not used as a visual quality claim

## Actual pixel review

The final source-matched set includes all eight views of head and whole body,
for Near/Far and both surface/clay material modes: 64 stills. It also includes
six pose modes at front/back oblique views, three-height extremes and attention,
eight 48-frame diagnostic animation sequences, CPU-path and actual software
OpenGL portraits/body views, and matching actual-game comparisons. Final surface
views are also supplied as lossless PNGs. Videos encode the actual submitted
frames at their recorded 12 Hz simulation cadence, not a claimed frame rate.

The [visual review](VISUAL_REVIEW.md) records rejected iterations and the exact
limitations visible in the final images. All render views retain the production
mesh, weights, materials and renderer; diagnostic-stage views are explicitly
separate from game-world evidence.

## Validation status

The exact frozen runtime source passes **7/7 serial ASan/UBSan tests** in
350.02 seconds: the full Rina geometry, contact and CPU-renderer audits, common
animation/skinning, NPC gameplay and presentation. Address and undefined-
behavior checks remained fatal-on-error. Leak detection was disabled only
because LeakSanitizer itself reports an unsupported ptrace environment; leak
freedom is not certified. The failed initial leak-enabled attempt is retained.

The ordinary aggregate suite passes **47/47 tests** in 239.43 seconds, with
no skipped tests. This includes independent 21-input CC0 reconstruction and
byte-identical head/hand regeneration; 1,368 pose/mesh/seam samples; 756
animated garment contact samples plus six bind cases and ten injected controls
per model; actual CPU raster, CPU ray and llvmpipe GL dirty/held/restore checks;
gameplay, full-world/NPC runtime, LOD, audio, assets and the other existing
individual characters. The 30/15/8 Hz presentation schedules preserve exact
held frames while simulation advances. Validation is Linux software-backend
coverage; hardware GPU and Windows/DXR certification remain outside this pass.

Final evidence integrity also passes: **153 source-matched reports, 7 exact
before/after game-state pairs and 1,224 submitted motion frames**. The 35-second
actual-world before/after patrols each travel exactly 61.4791092873 m, beyond
one complete unchanged circuit. All 10 encoded clips preserve the original
frame counts and 12 Hz simulation cadence. The five original diagnostic
default-pose captures also match the new zero-attention controls pixel for
pixel and retain the same stage state. Earlier failed art iterations remain
in the private study.

## Cost and delivery

Near/Far triangle counts are 38,616 / 15,927, within the 40,000 / 16,000 caps.
The 640² isolated portrait raster median rises from 51.9 to 231.6 ms; this close
view is materially more expensive. Near skinning is about 0.94 ms and dirty ray
geometry submission about 12.18 ms; Far is about 0.40 / 4.58 ms. Whole-scene
changes are small and noisy, and are not a speedup claim. See the detailed
[performance report](PERFORMANCE.md) for protocol, payload counts and limits.

The integration packet separates actor-owned files from a narrow shared patch.
Use [the integration instructions](INTEGRATION.md); do not overwrite the main project's
newer roster with this older private base. A later integrated build requires its
own fingerprint and final regression checks. No GitHub changes were published.

## Main integration status

Owned files are integrated into the accepted seven-character source, retaining
all earlier characters and approved world repairs. The new runtime fingerprint
is `8015066ee082524146d5888024447c1ed5198d719529a98c1484942ce8da856c`.
The combined source passes **55 registered suites**: 50 raster/runtime/source
checks and five actual llvmpipe checks, with no skips or relaxed limits.
All 32 freshly rendered all-side body/face images at both LODs are pixel-identical
to the private final model. Six current-world before/after views retain exactly
equal gameplay-state JSON sidecars. The comparison removes only Rina dispatch,
retaining all other seven models and approved world repairs in both variants.
The private 47-suite result remains separately labelled historical evidence.

After the executor interruption, the entire source input map and both executable
hashes were verified unchanged before retaining these results. No outage time
is added to the dedicated art ledger. See the
[combined capture proof](../../validation/characters/rina/integrated-captures/MANIFEST.json),
[post-interruption integrity](../../validation/characters/rina/integrated/POST_OUTAGE_INTEGRITY.json)
and [fresh world comparison](../../images/characters/rina/integrated-model-comparison.png).

## Review images and clips

- [Matched body before/after](../../images/characters/rina/near-body-before-after.jpg)
- [Matched face before/after](../../images/characters/rina/near-face-before-after.jpg)
- [Near all-side body](../../images/characters/rina/near-surface-body-turnaround.jpg)
- [Far all-side body](../../images/characters/rina/far-surface-body-turnaround.jpg)
- [Near all-side face](../../images/characters/rina/near-surface-face-turnaround.jpg)
- [Far all-side face](../../images/characters/rina/far-surface-face-turnaround.jpg)
- [Near poses](../../images/characters/rina/near-pose-sheet.jpg)
- [Far poses](../../images/characters/rina/far-pose-sheet.jpg)
- [World before/after](../../images/characters/rina/world-before-after.jpg)
- [Whole preserved patrol](../../videos/characters/rina/world/after-patrol.mp4)
- [Before patrol](../../videos/characters/rina/world/before-patrol.mp4)
- [Curated file hashes](../../validation/characters/rina/CURATED_MANIFEST.json)
