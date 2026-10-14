# Hale measured cost and limits

## Frozen-source shared-load observation

These are **shared-load observations, not an isolated performance comparison**. Other modeling/build/test jobs were not paused for the final source. Five fresh Release processes, each pinned to CPU 1, measured five repeats of 96 changing production poses per profile/LOD. Texture initialization and pose sampling occur outside the timed apply loop. The final runtime fingerprint is `81ccaa3e2cc3c614a5e8a6611681698b583e9942bbb7722ce380e9a363f07589`.

The five runs used 7.301 seconds wall time, 7.199 seconds user CPU and 0.097 seconds system CPU; peak child RSS was 47,152 KiB. Process order is fixed, and CPU frequency, shared cache contention and unrelated host/VM activity are uncontrolled. The raw JSON contains every repeat and per-process CPU/wall measurement.

| Profile | LOD | Median of process medians, ms/pose | Process-median range, ms | Construction median, ms |
|---|---|---:|---:|---:|
| Original generic guard | Near | 0.04879 | 0.04708–0.07013 | 0.189 |
| Original generic guard | Far | 0.02478 | 0.02307–0.03367 | 0.037 |
| Individual Hale | Near | 1.09230 | 1.06210–1.26471 | 146.197 |
| Individual Hale | Far | 0.44882 | 0.42015–0.46033 | 36.116 |

Individual Hale uses dual-quaternion skinning while the generic guard uses linear blend skinning. The observed change includes both geometry and skinning-path differences; it is a large cost increase, not an optimization claim. These are model-only CPU pose timings, not whole-frame FPS or GPU timings. The timed pose sequence also includes diagnostic stress poses; timing a pose does not certify its contact validity.

## Historical coordinated quiet measurement

Candidate 16, before the final ankle fitting and Far-row redistribution, was measured during a coordinator-confirmed pause of the other four passes and main. Its five-process medians were 1.07753 ms Near / 0.45307 ms Far, versus generic 0.05118/0.02492 ms. The process-median ranges were 1.01627–1.08974 / 0.41957–0.47103 ms. These figures are retained as **historical candidate-16 evidence**, not the frozen source's exact timing. That earlier run used 7.001 seconds wall time and 47,196 KiB peak child RSS.

## Frozen-source vector storage

| Profile | LOD | Logical vertex/index/influence bytes | Reserved vertex/index/influence capacity | One posed output mesh's reserved bytes |
|---|---|---:|---:|---:|
| Original generic guard | Near | 177,872 | 333,600 | 140,400 |
| Original generic guard | Far | 81,664 | 100,000 | 64,512 |
| Individual Hale | Near | 2,110,080 | 3,024,000 | 1,701,936 |
| Individual Hale | Far | 857,624 | 1,384,000 | 691,176 |

Reserved capacity is reported separately because the original model-construction reserves exceed live vector sizes. These sums are not a complete per-NPC RSS claim: they exclude allocator/container overhead, rig/cache structures, transient construction storage, renderer/GPU allocations and other retained engine instances. One initialized shared immutable material atlas contains 37,746,432 bytes (36 MiB); it is not duplicated for each Hale. Runtime pose allocation checks cover the tested changing poses after output mesh initialization, not model construction or every renderer operation.

The original 12 m nearest-render-bound LOD threshold is preserved. Final world captures retain automatic LOD selection and the original NPC scheduling. Diagnostic close-range Far images are explicitly forced, and may expose losses that would be less visible at normal distance.

Offscreen OpenGL is Mesa llvmpipe with shadows/reflections disabled by the logged software path. CPU ray/path captures and offline-encoded 12 fps simulated sequences do not establish real-time throughput. No hardware GPU, Windows DXR or AAA parity benchmark is claimed.
