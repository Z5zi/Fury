# GhostLoop measured cost

All runs used CPUs 3–4 only, with at most two computational jobs. The isolated
Release build is GCC 14, `-O3 -DNDEBUG`; the benchmark is pinned to CPU 4. These
are measurements of this cloud CPU and software OpenGL driver, not target-device
or live-game frame-rate promises.

| Model | Near vertices / triangles | Far vertices / triangles |
|---|---:|---:|
| Exact original generic Ghost | 2,142 / 2,204 | 994 / 1,040 |
| Final individual GhostLoop | 25,209 / 39,346 | 10,075 / 15,970 |

The final asset stays below 40,000 Near and 16,000 Far triangles. Both LODs keep
the same source face; Far simplifies hands, garment sampling, boots and groom.

The direct benchmark measures `apply_character_pose` on preallocated output
meshes, excluding the pose solver, world and renderer. It uses 240 measured
samples after 40 warmups for idle, walk, run and attention/talk at each LOD.

| Model / LOD | Range of mode medians | Largest mode p95 |
|---|---:|---:|
| Original Near | 0.0426–0.0442 ms | 0.0561 ms |
| Original Far | 0.0214–0.0218 ms | 0.0224 ms |
| Final Near | 1.0833–1.3893 ms | 1.5411 ms |
| Final Far | 0.4174–0.4329 ms | 0.5818 ms |

Five warm construction samples after one throwaway have medians of 185.263 ms
Near and 29.7146 ms Far. Model construction is an installation cost; ordinary
posing retains output identity, storage addresses, capacities and indices.
The renderer regression separately verifies exact held/restored pixels and
30/15/8 Hz update cadence. Direct pose timing does not include frame rendering.

Bind plus posed vertex/index buffers and influence arrays contain 3,767,712
live bytes for Near and 1,511,680 for Far, about 5.04 MiB combined, excluding
container/rig metadata and allocator slack. The original totals 429,088 bytes.
The shared material atlas and its retained mip pixel arrays total 37,746,432
bytes, about 36.0 MiB, once per process across all actors. Its first request took
460.069 ms in this benchmark. That shared atlas is not copied for Ghost.

The identical-schedule real-network day comparison renders 120 frames at
640×480. Original-model process: 32.937 s wall, 32.418 s CPU, 636,956 KiB peak RSS.
Final-model process: 36.009 s wall, 35.470 s CPU, 652,840 KiB peak RSS. Final night
close-up: 340 frames, 113.438 s wall, 111.805 s CPU, 688,196 KiB peak RSS. Final
18 m Far capture: 120 frames, 34.220 s wall, 33.685 s CPU, 655,476 KiB peak RSS.
These include startup, rendering, simulated updates and capture I/O. The 10 Hz
and 4 Hz capture settings control simulation time; they are not live FPS.

Actual OpenGL is llvmpipe, with one worker requested. The environment lacks
OpenGL development-package discovery, yet the real SDL offscreen GL 3.3 path
works. Driver fallbacks disable shadows/reflections and choose FXAA instead of
requested MSAA. CPU path/raster and actual GL were checked independently.

The world evidence contains the immutable accepted eight-character base plus
this individual Ghost pass, with 16 startup presentation slots. It is not a
performance certification of the coordinator's eventual eighteen-character
integration.

Reproduction and raw data: `tests/ghost_pose_benchmark.cpp`,
`scripts/build_ghost_benchmark.py`, `ghost-evidence/performance/`, and each
`final-network-*/manifest.json`. The benchmark link map and binary/compiler/
source hashes identify the exact measurement artifact.
