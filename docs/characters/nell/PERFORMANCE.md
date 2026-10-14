# Nell integrated CPU measurements

Measured 7 October 2026 during the coordinated quiet window after Rina's batch,
with other character builds/renders/sanitizers paused. Thirty-six runs executed
serially, two CPU workers, three alternating AB/BA/AB repetitions per case.
The after build contains Mira, Jon, Tessa, Owen, Lia, Nell and Pax. The before
build removes only Nell's exact-ID dispatch; it retains every other model,
Lia's position/visibility repairs and Nell's approved route/fixture corrections.
All six actual-world pairs have exactly equal gameplay-state JSON sidecars.

Both source sets were explicitly CMake-configured. Only the differing dispatch
and build-fingerprint objects were rebuilt for the baseline; hash-verified
unchanged objects were reused with the same compiler flags. Source maps differ
in exactly the two removed dispatch lines, and observed fingerprints match.

The values below are median **last renderer-frame** milliseconds, not a complete
main-loop frame distribution. Peak resident memory is the maximum Linux process
RSS among three runs. Stage captures are 480×640; the live Ashcourt case is
480×360. Raster uses eight frames; ray uses four at 1 spp/two bounces.

| Scenario | Backend | Generic ms | Nell ms | Generic peak MiB | Nell peak MiB |
|---|---|---:|---:|---:|---:|
| stage-near | raster | 35.41 | 97.31 | 15.88 | 58.22 |
| stage-near | ray | 120.65 | 196.82 | 22.36 | 80.23 |
| stage-far | raster | 36.58 | 95.01 | 15.59 | 55.12 |
| stage-far | ray | 117.54 | 167.22 | 21.79 | 65.74 |
| world | raster | 261.59 | 281.08 | 221.45 | 226.60 |
| world | ray | 822.61 | 744.23 | 337.37 | 352.36 |

Near stage raster cost rises about 2.75×, with retained source anatomy still
present in Far. The world raster increase is about 7.4% in this small fixed
sample. World ray ranges overlap substantially (generic 679.15–826.62 ms,
Nell 707.13–843.00 ms); its lower after median is **not evidence of a speedup**.
Startup-inclusive world medians rise from 4.76 to 6.92 seconds in raster and
5.92 to 8.19 seconds in ray. Geometry construction is outside the last-frame
renderer figure. No result supports real-time rendering or physical-GPU claims.

The helper preserves individual samples, commands, settings, executable hashes,
source maps, peak RSS and startup-inclusive elapsed time in
[benchmark.json](../../validation/characters/nell/integrated-benchmark/benchmark.json).
[Exact gameplay pairing](../../validation/characters/nell/integrated-benchmark/state-pair-verification.json)
and [source difference](../../validation/characters/nell/integrated-benchmark/SOURCE_DIFF.patch)
are separate checks. The source fingerprints are:

- Generic Nell: `4987eccd616e2bfe4695a3c44ab708123558983738837b3930d8641fd74ac3b5`
- Individual Nell: `44e70ba992cdf7444e755e5b54b66706278d489cb85e3395db8fa5c519aa8e06`

The 12 fps simulation/encoding cadence is unrelated to achieved live frame rate.
Ray timing excludes earlier BLAS preparation, and renderer timing excludes
main-loop skinning. These observations are specific to this shared cloud CPU
and bounded settings; they do not establish GTA VI parity.
