# Pax Wren — final individual pass 01

Final art checkpoint: **6 October 2026, 23:52:04 UTC**. Dedicated work:
**2:00:47**, after the explicit exclusions in [WORK_LOG](WORK_LOG.md).
[CHECKPOINT_01](CHECKPOINT_01.md) is the preserved 22:36 historical failure
snapshot; its pending band/vest crossings were corrected before this final pass.
Subsequent sanitizer, height-matrix and documentation work does not add art time.
This is a tested development checkpoint, not finished AAA art or GTA VI parity.

## Delivered model and unchanged gameplay

Pax remains `NpcCivD`, Dock, 1.72 m, seed `ec850fd2`, with the existing palette.
The separate CC0 anatomical recipe provides a source-correspondent head/neck,
connected hands and fitted wrists. The pass adds a broad-waisted sewn shirt,
closed layered vest and fitted bands, cargo detail, shaped boots, and opaque
gray gathered/bun hair. Near/Far totals are **38,752 / 15,940 triangles**.
See [provenance](ANATOMY_PROVENANCE.md), [geometry](GEOMETRY_REVIEW.md) and
[visual limitations](VISUAL_REVIEW.md).

Spawn, 56 m route, DayOnly schedule, speed, collider, dialogue and missions are
unchanged. Diagnostic crouch-walk/attention controls are separately owned tooling.
The captured private roster is the fixed Owen-base plus Pax, not a later combined
roster. Capture fingerprint:
`afa63a7d11527084bd92ee001ef88160c2fdb8ad45fa43597d7df413866e105f`.

## Validation

- Optimized and ASan/UBSan: **78 checks each** across 1.35/1.72/2.10 m × Near/Far;
  1,398 motion/attention samples and 288 contact poses per build
- Finite geometry, source head/hand correspondence, sockets, opaque scalp,
  physical-shell topology and sampled garment/body contacts passed. The final
  band/vest crossings are zero; a penetration negative control is detected
- Canonical Near/Far dumps remained byte-identical after the test-only matrix
  extension. All model/source hashes remained unchanged
- Retained regressions: 18 character/NPC/renderer/application tests, two software
  OpenGL tests and the NPC runtime test passed. Both importers passed `--check`
- Post-reset optimized recheck: 78/78, exit 0. Prior sanitizer source, binary and
  evidence hashes were reverified. **Leak checking is unvalidated**: LeakSanitizer
  explicitly failed under ptrace; only leak detection was disabled afterward

[Height-matrix results](../../validation/characters/pax/height-matrix-20261007/README.md),
[sanitizer limits](../../validation/characters/pax/sanitizer-final-20261006/README.md),
[fresh recheck](../../validation/characters/pax/resume-validation-20261007/optimized-result.json),
and [curated evidence index](../../validation/characters/pax/README.md).
Finite tests do not prove every possible pose. Stage floor alignment is not a
new gameplay foot-IK feature. Combined-tree checks must be rerun after integration.

## Linked actual-pixel evidence

| Review | Preserved evidence |
|---|---|
| Matched baseline → final | [Comparison](../../images/characters/pax/matched-before-after.png) |
| All-side material body | [Near](../../images/characters/pax/near-material-body-sheet.png) · [Far](../../images/characters/pax/far-material-body-sheet.png) |
| All-side material head | [Near](../../images/characters/pax/near-material-head-sheet.png) · [Far](../../images/characters/pax/far-material-head-sheet.png) |
| All-side clay body/head | [Near body](../../images/characters/pax/near-clay-body-sheet.png) · [Far body](../../images/characters/pax/far-clay-body-sheet.png) · [Near head](../../images/characters/pax/near-clay-head-sheet.png) · [Far head](../../images/characters/pax/far-clay-head-sheet.png) |
| Idle/walk/run/crouch/crouch-walk | [Near](../../images/characters/pax/near-poses-sheet.png) · [Far](../../images/characters/pax/far-poses-sheet.png) |
| Top/under/attention | [Special views](../../images/characters/pax/special-review-sheet.png) |
| Actual 12 m world LOD | [Clear threshold sheet](../../images/characters/pax/lod-clear-sheet.png) |
| Moving original world | [Route sheet](../../images/characters/pax/live-route-sheet.png) · [Continuous 30 s video](../../videos/characters/pax/live-route.mp4) |
| Talk presentation | [Sheet](../../images/characters/pax/live-talk-presentation-sheet.png) · [Continuous 4 s video](../../videos/characters/pax/live-talk-presentation.mp4) |
| Backend diagnostics | [Stage sampler](../../images/characters/pax/backend-stage-sampler.png) · [Moving-world sampler](../../images/characters/pax/backend-world-sampler.png) |

All images originate in Fury captures. The two backend samplers only paste and
label native-size pixels; resolutions, framing and sampling differ. No images
were retouched. The videos retain all 360/48 encoded frames at 12 fps, verified
by decoding. Encoding cadence is not measured live FPS. The route records 55.995 m
travel and other agents moving; a car and another actor occlude some views.
Neither occlusion nor these finite captures establishes full route clearance.
Talk mode is not a manual Q/dialogue-input test.

The production LOD function selects Near/Far at 11.963698/12.036302 m; principal
silhouette is retained in this pair. Earlier occluded LOD attempts are excluded
from the presentation sheet and clearly retained as failed attempts in logs.

## Costs and remaining visible defects

Portrait CPU raster medians at 480×640 rose **42.82 → 192.11 ms**. Live-world
CPU path at 320×240 was **444.36 ms**. These are renderer cost samples, not
realtime FPS or combined-roster measurements. [PERFORMANCE](PERFORMANCE.md)
records the resolutions, three-run ranges, startup costs and limitations.
OpenGL uses Mesa llvmpipe; no physical-GPU performance was established. Its zero
renderer-time/triangle audit fields are unimplemented, not zero work.

Vest shoulder/armhole cuts remain jagged/angular, cloth rigid, hair cap-like with
repetitive roots and a faceted Far bun, skin smooth/waxy, shoes chunky, and fingers
fixed in a static curl. There is no facial performance, finger animation or
cloth/hair simulation. Path diagnostics remain noisy. Numerical passes do not
resolve those art limitations. [INTEGRATION](INTEGRATION.md) describes surgical
registration; preserve other actors and reconfigure before combined captures.

## Combined-source acceptance, 7 October 2026

The shared seven-character source now passes **47 registered suites** (43
raster/runtime/source and four actual llvmpipe), plus the diagnostic-stage
attention/crouch-walk checks and both Pax importers. No timeout was increased;
`npc_runtime` completed in 151.51 seconds under its existing 240-second limit.
Current runtime fingerprint:
`44e70ba992cdf7444e755e5b54b66706278d489cb85e3395db8fa5c519aa8e06`.

Fresh stage captures match the private accepted images pixel-for-pixel: four
Nell body/face/LOD samples and all six matched Pax samples. Eight integrated
Nell world angle pairs retain exactly equal gameplay-state sidecars, with the
same approved corrected route/world in both variants. Fresh Pax world front,
right and rear captures preserve real surroundings and are separately recorded.
Private and combined fingerprints are intentionally different; the actual
[combined capture manifest](../../validation/characters/pax/integrated-captures/MANIFEST.json)
records every command, source fingerprint and pixel check. These checks preserve
the visible limitations stated above and do not establish finished AAA art.
