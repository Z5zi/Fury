# Full-world runtime regression

`tests/world_runtime_tests.py` launches the actual Vaultline executable. Its audit
path calls the same complete six-district world builder used by gameplay. It does
not infer coverage from source text or validate a substitute gallery.

## Fast gate

```sh
python3 tests/world_runtime_tests.py --vaultline build/apps/vaultline/vaultline \
  --source-root . --output-dir out/world-regression
```

The gate verifies:

- World art disabled, enabled, enabled in a fresh process, and default-enabled
- Ordered original names, tags, solid flags, positions, rotations, scales and
  collider centers/half-extents agree across real off/on builds
- All original entities are accounted for, with fixed anchors in all six districts
- No new collision solids; no added invalid indices, nonfinite vertices/instances
  or degenerate triangles relative to the original asset baseline, including
  every referenced near and LOD mesh even on hidden entities
- Positive architecture, ground, life and water totals and positive aggregate coverage
  in Harbor Metro, Ridge Pier, Ashcourt, Depot, Loft and North Quay
- All three existing water surfaces receive maps, with no new triangles and at
  most 2 MiB of shared map storage
- At most 1,200 added entities and 125,000 added rendered-mesh triangles
- Identical repeated audit JSON, including geometry/layout fingerprints supplied
  by the runtime, and the same result with the environment toggle unset
- Malformed toggle values, a missing audit path and an unknown capture view fail
- Actual CPU raster and ray rendering with art disabled/enabled at 160×90,
  two frames and one sample per pixel, with valid, nonblank, visibly different PPMs
- A repeated enabled CPU-ray launch produces identical frozen capture pixels
- A 45-second per-process wall-time bound; actual ray frame count, triangle count
  and validation-error counters from the application's own shutdown log

Tests run with at most two CPU workers, dummy SDL video/audio and null game audio.
On Linux, process affinity also limits the test and its children to two available
CPUs. An isolated working directory has access to repository assets but separate
settings and save files, so the player's current state cannot influence captures.
Successful temporary runs clean up; failures retain logs/audits/captures.

## Reproducible visual evidence

```sh
scripts/validate-world.sh build out/world-validation
# Reuse a build already validated by CTest:
FURY_SKIP_BUILD=1 FURY_SKIP_CTEST=1 scripts/validate-world.sh build out/world-validation
```

The script saves 44 paired off/on PPMs for `metro-wide`, `metro-street`, `ridge`,
`ridge-street`, `ashcourt`, `market-street`, `depot`, `loft`, `quay`, `waterfront`
and `world-overview`, using both
the software rasterizer and CPU ray tracer at the exact same camera and settings.
Defaults are 640×360, three frames and one sample per pixel. Change
`FURY_WORLD_WIDTH`, `FURY_WORLD_HEIGHT`, `FURY_WORLD_FRAMES`, `FURY_WORLD_SPP`, or
`FURY_WORLD_TIMEOUT` explicitly for heavier evidence captures. Build parallelism
defaults to two and accepts one or two workers only.
`FURY_WORLD_RAY_FRAMES` and `FURY_WORLD_RAY_SPP` separately raise CPU ray quality
without increasing rasterization work; for example, use six ray frames and two
samples per pixel while keeping three raster frames. The small regression gate
always retains its two-frame, one-sample settings.

Optional `FURY_WORLD_PATH_SMOKE=1` adds a small actual CPU path-tracing smoke.
Optional `FURY_WORLD_GL_SMOKE=1` adds a GL smoke; provide a working display or run
the script under `xvfb-run -a`. A software fallback does not count as a GL pass.
No GPU or display server is required for the default gate or evidence matrix.

`world-validation-summary.json` records the command and process wall time for each
launch, capture SHA-256 hashes, changed-pixel fractions, and actual renderer
counters when logged. `last_cpu_frame_ms` is the last renderer-measured frame,
not an average or a percentile. Process wall time includes startup and asset
loading and must not be reported as frame time or FPS. Rasterization currently
does not emit the same shutdown counters, so its record contains wall time only.

## Scope and visual review

Counter equality proves that added art introduces no new reported geometry
diagnostics; baseline findings are separately listed, not hidden or mislabeled
as a clean baseline. The `degenerate_triangles` diagnostic uses squared local-space
cross-product magnitude below `1e-14`. This includes microscopic nonzero facets;
it does not mean that every counted triangle has exactly zero area. The separate
`zero_area_triangles` counter identifies exact-zero results. Preserved
collision records plus zero added
solids protect the original collision world, but do not prove that decorative
geometry never visually overlaps a doorway or roadway. Inspect each paired image
for those overlaps, z-fighting, ground discontinuities, obstructed mission entries,
water visibility, proportion and silhouette. Check wider frames as well as street
views. Changed pixels prove the toggle has a rendered effect, not that it looks
better. These tests do not claim an end-to-end mission playthrough, gameplay FPS,
Windows/DXR validation or hardware-ray-tracing performance.
