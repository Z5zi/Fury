# Nell final measurement handoff

**Completed 7 October 2026:** the final integrated generic-Nell versus
individualized-Nell measurement ran in a coordinated quiet window. See
[PERFORMANCE.md](PERFORMANCE.md) for the measured source pair and complete results.
The procedure below is preserved for reproduction; the older private pair at
the end remains historical and was not substituted for the integrated run.

## Pair construction

Use the final integrated roster for both builds, including corrected Lia and
the approved Nell fixture/route/test changes. Make a private baseline copy that
removes only these two dispatch lines from `engine/src/character_profile.cpp`:

```cpp
  if (identity == "NpcCivAsh" && role == CharacterRole::Market)
    return make_nell_character_profile(height, role, identity, lod);
```

Keep the declaration, compiled Nell source and every other file unchanged.
That baseline renders the original generic Nell while retaining all other
actors, the same route, world, assets, settings, and build flags. Reconfigure
both copies so their embedded source fingerprints are truthful. Build with
`--parallel 2`, sequentially, and record compiler/configuration details.
Do not use `FURY_NPC_DETAIL=0`: it changes the whole population. Do not compare
against the original pre-route world or the older Owen-only roster.

Record a JSON manifest for each build with `source_fingerprint`, a complete
`source_file_sha256` map from `generated/source-files.txt`, and `binaries` with
SHA256 values for `fury_characterlab` and `vaultline`. Verify that the input sets
match and that the sole source-file difference is exactly the two removed lines.
The helper rejects any other differing source-file list and checks executable
hashes and observed runtime fingerprints. The integrator must additionally
review that exact two-line source diff and the matching compiler flags.

## Serialized protocol

The private handoff includes `benchmark_nell_pair.py`. It first prints a plan;
passing `--run` executes it. Give it the final pair's absolute paths:

```sh
python3 benchmark_nell_pair.py \
  --before-bin "$BASELINE_BIN" --after-bin "$FINAL_BIN" \
  --before-manifest "$BASELINE_MANIFEST" --after-manifest "$FINAL_MANIFEST" \
  --source-root "$FINAL_SOURCE" --output-dir "$NEW_EMPTY_RESULT_DIR" \
  --repeats 3 --run
```

No other build, renderer, sanitizer or benchmark may overlap the measurement.
The script runs one process at a time with two CPU workers. It clears inherited
FURY settings, uses high quality, character/world/surface detail on, dummy SDL,
null audio and isolated fresh save directories. Variants alternate AB/BA/AB.
There are 36 samples: Near stage, Far stage and actual Ashcourt world, each in
CPU raster and CPU ray modes, three repeats and two variants.

Stage: `NpcCivAsh`, Market, 1.75 m, walk, phase 0, animated at 12 simulated fps,
45-degree yaw, 3.2375 m distance, 480×640. World: actual `--npc-motion NpcCivAsh`,
3.2 m distance, 45-degree orbit, 25-degree elevation, 480×360. Raster runs eight
frames; ray runs four frames at one sample/pixel and two bounces. These bounded
runs measure costs, not route clearance or long-term animation coverage.

Retain per-sample commands, source/binary hashes, metrics, logs, startup-inclusive
wall time and Linux peak RSS. Report individual last-frame samples plus their
median, and keep stage and world results separate. The renderer statistic is
not a whole-loop frame distribution and excludes main-loop skinning/update;
ray statistics also exclude earlier BLAS preparation. Do not convert the
12 fps simulation cadence into an achieved frame-rate claim. No hardware-GPU,
AAA or GTA VI equivalence is implied.

## Preserved historical pair

The private unintegrated pair is verified and its exact 36 commands are saved
in `benchmark-private-plan.json`, but none were executed in this final handoff.
After source: `2b72f211854d0e7ec3ff29ee0f5bc07808e7e89c433897dc49838219233d3b2c`.
Generic baseline: `93a942d351468617d7f2eca22d8db34e2ee2b9ca29608b565c7b9e0555490d31`.
Its rendering-input maps differ only in the dispatch file. This older pair
cannot substitute for the final integrated measurement.
