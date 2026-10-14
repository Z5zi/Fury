# Pax CPU cost and world LOD

A coordinated window held other artists and the main build CPU-idle while
24 fresh processes ran (eight cases, three repetitions each) on 6 October
2026, 23:43:13–23:44:07 UTC. Each run had a new working directory and an
assets symlink. CPU threads were limited to two, SDL dummy video/audio and
the null audio backend. Raster omitted FURY_TRACE_MODE; path explicitly used
path mode, 1 sample per pixel and 2 bounces. Every report had zero validation
errors and the final source fingerprint.

Portraits render two frames. Populated scenes render four frames at a fixed
12 Hz simulation cadence. The table reports the last renderer frame, and
separately the measured startup-inclusive whole-application time. It is not
a live FPS measurement. Raster and path resolutions differ, so their columns
are not equal-quality renderer comparisons.

| Case | Resolution | Last renderer median ms (range) | Whole application median s |
|---|---|---:|---:|
| portrait-legacy raster | 480×640 | 42.82 (41.77–43.60) | 0.106 |
| portrait-legacy path | 240×320 | 71.54 (67.58–89.86) | 0.158 |
| portrait-pax raster | 480×640 | 192.11 (187.66–194.34) | 0.970 |
| portrait-pax path | 240×320 | 121.60 (117.53–153.29) | 0.885 |
| world-frozen raster | 480×360 | 287.86 (277.37–309.56) | 3.479 |
| world-frozen path | 320×240 | 321.38 (302.56–322.37) | 4.068 |
| world-live raster | 480×360 | 245.14 (240.11–247.99) | 3.343 |
| world-live path | 320×240 | 444.36 (443.26–459.97) | 4.648 |

The portrait A/B uses the same binary's original generic profile versus the
new individualized profile. This captures the added geometry and material
work, including a meaningful raster cost increase. It does not isolate each
subsystem. The world rows are **absolute current-scene costs**: no preserved
pre-Pax world binary was available. They contain the fixed private Owen-base
roster with Pax, not the later combined roster.

Live path reported 1,384 BLAS and 4 TLAS builds over four frames, compared
with 1,243 BLAS and 1 TLAS for the frozen case. Frozen accumulated four frames;
live accumulated one because the scene changed. The camera and visible
population also differ, so this is not a controlled estimate of animation
overhead alone. It does demonstrate why a static portrait understates the
work of an animated populated scene. A single reciprocal of these last-frame
times must not be presented as measured gameplay FPS.

Raw reports, command arguments, the three individual timings and images are
in `cpu-benchmark-final`. The reproducible driver is
`scripts/benchmark_pax_cpu.py`. Mesa llvmpipe capture times are separate and
are not physical-GPU evidence.

## Actual 12 m transition

The first front-facing 12 m camera landed inside an existing building. Those
failed views remain in the evidence and are not used as visual LOD proof.
The final pair uses orbit 90 degrees and diagnostic elevation 12 degrees,
with the original scene intact, at 960×720 CPU raster resolution. The actual
camera-to-primary-AABB distances, computed by Fury's own visibility function
from the captured camera positions and real model bounds, are:

- 11.9636981418 m: Near, 38,752 triangles
- 12.0363023254 m: Far, 15,940 triangles

The clear full frames and explicitly labeled 2.5× nearest-neighbor crops
show Pax at the real threshold. The opaque scalp, bun, vest body and bands
retain their main silhouette and color; the Far hand/cloth detail is coarser.
The same atlas surface and tint are used on the strips in both LODs, so no
unrelated material mode changes at the switch. This is a finite camera pair,
not a claim that every motion or lighting condition has imperceptible LOD.

The populated raster totals are 528,406 / 505,822 submitted triangles;
those totals also reflect other visibility changes (555 / 563 instances).
Their difference must not be labeled Pax's exact polygon reduction. The
isolated 38,752 / 15,940 selection above uses the production visibility
function and captured cameras, with the reproduction helper included in
the evidence as source only.
