# Rina: measured resource and rendering cost

These are shared-cloud CPU observations from the isolated before/after study.
The project paused other known heavy work for the 7 October 00:30–00:32 UTC
measurement window. The binaries and runtime source fingerprints are pinned
in the evidence manifest. No hardware GPU, shipping frame-rate or statistical
speedup claim is made. The study base already has Mira, Jon, Tessa and Owen;
these measurements do not certify the later fully integrated roster.

## Alternating process benchmark

Three before/after pairs per case; four submitted frames per fresh process.
Values below are medians of the final CPU frame. Portrait is 640×640, moving
street raster and metro are 640×360, and moving street path is 320×240 at one
sample/pixel and two bounces. Ray work uses two threads. Moving street cases
advance actual NPC simulation at a fixed 12 Hz capture cadence. All nine
populated-world before/after state pairs are exactly equal, including the
six moving pairs. Simulated capture cadence is not measured frame rate.

| Case | Before ms | After ms | Observed change |
| --- | ---: | ---: | ---: |
| portrait-raster | 51.858 | 231.600 | +346.6% |
| east-street-moving-population-raster | 618.762 | 590.742 | -4.5% |
| metro-population-raster | 284.351 | 268.312 | -5.6% |
| east-street-moving-population-path | 153.942 | 151.734 | -1.4% |

The isolated close portrait is materially more expensive: roughly 4.47× the
old generic image cost. Its anatomy, additional surfaces and textured material
all contribute. The small decreases in populated-world samples must not be
presented as a reliable speedup: three short repeats on shared cloud resources
are insufficient to isolate small changes from noise. Larger-scale target
hardware profiling remains necessary.

## Mesh, skinning and ray-geometry update audit

Eighty measured updates after eight warm-ups, separately for each model/LOD.
Each changed mesh causes one actual ray-geometry rebuild; repeating its pose
holds the same revision and cache with no new rebuild. These are production
engine calls. This utility times submission/BVH work, not a complete frame.

| Model | Vertices | Triangles | Pose sample µs | Skin µs | Dirty ray submission µs | Held submission µs |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| exact generic Rina constructor near | 2,216 | 2,248 | 2.829 | 44.437 | 512.458 | 0.501 |
| Rina individual near | 24,764 | 38,616 | 6.284 | 941.026 | 12184.500 | 1.252 |
| exact generic Rina constructor far | 1,044 | 1,068 | 2.353 | 22.343 | 217.642 | 0.451 |
| Rina individual far | 10,102 | 15,927 | 3.756 | 398.998 | 4584.400 | 0.656 |

Near/Far actor triangle budgets are 38,616 / 15,927, under the 40,000 /
16,000 caps. Actual skinning allocates no per-frame memory in the independent
geometry test. Geometry/influence payloads are:

| Model | Bind vertex/index payload bytes | Influence payload bytes |
| --- | ---: | ---: |
| exact generic Rina constructor near | 133,344 | 35,456 |
| Rina individual near | 1,652,064 | 396,224 |
| exact generic Rina constructor far | 62,928 | 16,704 |
| Rina individual far | 676,020 | 161,632 |

The existing shared character surface atlas contains 37,746,432
pixel bytes across four maps and 6 additional mip levels. Its width is
1536; it is shared with the other individualized characters and is not
an extra copy per Rina actor. Payload counts above exclude spare vector
capacity, posed copies, renderer buffers, BVH caches and allocator overhead;
they are not a total resident-memory measurement.

Evidence: `out/rina/bench-final/benchmark.json`,
`out/rina/bench-final/paired-state-verification.json` and
`out/rina/model-cost.json`. All raw paired reports and reproduction utilities
are retained in the final source/evidence deliverable.
