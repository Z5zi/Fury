# Vale cost measurements

This is a visual-quality increase with a substantial CPU/geometry cost. It is
not a free replacement for the original generic Security mesh.

The exact final-source executable constructs both the original generic model
and the individual Vale model with the same 1.86 m height, identity seed, CPU
set and optimization flags. Eight interleaved A/B and B/A samples reduce order
bias. Each skin batch evaluates 128 changing patrol poses. A separately
coordinated quiet window had no other actor-pass or main heavy work running.
CPU affinity is 5–6, with two-worker environment limits.

| Measurement | Generic Near | Vale Near | Generic Far | Vale Far |
|---|---:|---:|---:|---:|
| Triangles | 2,332 | 38,364 | 1,088 | 15,928 |
| Median warm constructor | 0.0936 ms | 62.5214 ms | 0.0424 ms | 24.3386 ms |
| Median skinning per changing pose | 0.0520 ms | 1.1754 ms | 0.0262 ms | 0.5179 ms |

Construction excludes the one-time shared material atlas generation, which is
reported separately in `COSTS.json`. The skin timer excludes animation sampling,
rendering, GPU uploads and ray acceleration builds. The generic model uses the
original skinning mode; Vale uses his verified dual-quaternion path. Both LODs
are still installed by the original presentation code, and its existing update
throttling/culling remains unchanged. No controller is edited to make these
measurements look better.

Memory data in the JSON records `sizeof(Vertex)` and `sizeof(CharacterInfluence)`,
logical bind bytes, actual reserved vector capacity, and the bind-plus-mutable
mesh storage used by the original presentation layout. Logical bytes are not
an RSS measurement. Capacities include reserve overhead; neither estimate
includes allocator metadata, rigs, driver copies, texture storage or ray BVHs.
The shared character atlas is listed separately and is already shared by other
individualized actors, not allocated separately for Vale.

Raw samples, source/binary hashes, measured vector sizes and capacity are
retained in `COSTS.json`. The system has no `/usr/bin/time`; a wrapper failed
before launching the probe, then the exact probe ran directly once. No process
RSS number is invented. OpenGL's renderer statistics expose zero timing fields
in this build, so those zeros are not interpreted as zero rendering cost.

## Original-world CPU-ray samples

Three interleaved original-binary/final-binary pairs used the original world,
320 × 424 pixels, one spp, two bounces, and eight live guard frames at 12 Hz.
Every saved final simulation state is identical across all six runs. Median
whole-process time was 8.1252 s before and 8.5651 s after. Median final-frame
render time was 625.3841 ms before and 697.4048 ms after. The before frame range
was 616.6604–727.0894 ms; after was 669.8501–699.6447 ms. These ranges overlap.

This second measurement was not a team-wide quiet window. It is a small
bounded workload, not a statistically established steady-state frame-rate
regression or a hardware performance claim. Unique scene triangles increased
from 717,611 to 753,643, exactly the 36,032-triangle Near model increase.
Full commands, samples, state checks and fingerprints are in `WORLD_COSTS.json`.

The measured Near+Far bind/posed vector capacity is 6,355,856 bytes for Vale
versus 638,512 for the generic baseline, excluding rigs, allocators, driver
copies, shared textures and ray acceleration. Logical geometry/influence
bytes are 5,298,496 versus 464,448. The existing shared atlas is 37,746,432 bytes;
its first-generation time in the cost process was 483.9 ms, shared across
individualized actors rather than an additional per-Vale atlas.
