# Metro Watch individual pass handoff

The private individual pass and two-hour conservative active-work floor are
complete. This is a reviewed prototype with explicit limits. It is **not an
all-green strict-stress release certification** and is not published.

## Frozen result

- Base: `9d424277e99ba37a8c66e77430512d6dc188ab40`
- Exact production fingerprint:
  `d87b46e676d2152715d3cc83987fd5d8d9f7b15d6f3311aeaa67cd35acdc44ef`
- `NpcExtraGuard`, Metro Watch, Security, seed `0x3e2ad98f`, 1.82 m;
  original role, palette, spawn/waypoints, speeds, mission gates and gameplay
- Near **39,192** / Far **15,980** triangles, under unchanged 40k/16k caps
- Complete 8,660-triangle anatomical Face in both LODs; exact ordered source
  corners/UVs and connected source-derived hands; separately pinned CC0 inputs
- Original fitted finite cap, carrier, connected uniform/trousers and boots
- Frozen owned-file hashes and scoped shared deltas:
  `out/character-study/metro_watch/final-audit/frozen-source.json` and
  `shared-source.patch`; do not replace coordinator-owned shared files wholesale

The source was frozen before final artifact bookkeeping. No production source
changed during this recovery; the retained before/after source fingerprint
continues to match all final evidence. Private copyrighted reference images
are excluded from the deliverable.

## Verification and evidence

The original private paths below identify provenance. Their curated copies are in
`docs/validation/characters/metro_watch/`; INTEGRATED_FILE_MAP.json maps every
original path to its included file. Cumulative integration validation is complete at the disclosed scope: [63-suite results and fresh image/state checks](../../validation/characters/integrated-metro-20261007/README.md).

- `accepted-stage`: 126 exact-source reports/captures; both LODs, eight-side
  body/head material/clay, low/top views, ordinary poses, visible stress poses
  and attention. `accepted-world`: actual unchanged bank before/after and
  twelve live-motion frames. Four after reports carry the frozen fingerprint;
  the baseline deliberately carries its older fingerprint.
- `lifecycle/accepted`: **12 passed production lifecycle cycles across all six
  selected missions**, including the ordinary NightVault gate. Real E/crouched
  breach and selected-vault complication spawn; duplicate spawn idempotence;
  pursuit, Failed and ordinary Escape cleanup; E reset and authored loft
  cooldown; cached meshes/materials and reset locomotion. The failure branch
  calls the real `force_fail()` API intentionally. Diagnostic camera/RNG are
  controlled; gameplay controllers are unchanged.
- Reset residual: at most **5.96046447754e-08 m**, within the existing **2e-6 m**
  renderer contract, with an above-tolerance negative control. The earlier
  failed bit-exact assumption remains in `out/metro-lifecycle-final-tests.log`.
- Ordinary geometry/contact gate: **1,440 samples passed**, three heights
  (1.35/1.82/2.10 m), both LODs, 24 phases and ten modes; exact source/palette,
  storage/indices, finite normals/geometry, topology, seams, scalp/eyes and
  garment/hand/boot contacts; injected crossing controls rejected.
- **ASan/UBSan ordinary: passed the same complete 1,440-sample matrix.**
  `detect_leaks=0`; no address/undefined-behavior error. The separately run
  sanitized strict default fails at its unchanged geometry assertion.
  `final-audit/lsan_availability.cpp` is an independent empty-process probe:
  LSan fails under this environment's ptrace restrictions, so leaks are not
  certified. Logs and binary hashes: `final-audit/verification-index.json`.
- Private-study aggregate: **45/46 CTest gates passed**. The sole failure is
  the retained strict Metro crouch-walk gate; `out/metro-regressions.log`.
- `final-audit/attention-preservation`: all five pre-existing default poses
  have byte-identical pixels and exact pose/camera/bounds against the retained
  pre-change characterlab binary, plus omitted/zero controls. Invalid attention
  arguments are rejected and crouch-walk advances. This closes a gap in the
  earlier CTest report, which compared only omitted/zero controls.
- `final-audit/height-extremes`: 16 individually inspected Near/Far run/crouch/
  attention captures at 1.35 and 2.10 m. `final-audit/lod-distance`: six Near/Far
  bind/walk/crouch stills at 12 m; diagnostic forced LOD, not an automatic
  gameplay transition claim. All retain the exact production fingerprint.
- Source generators' `--check` passed, all 19 pinned inputs match SHA-256,
  size and Git-blob identity, all final source hashes match, and `git diff
  --check` passes. Main checkout/integrated roster was not modified or certified.

## Disclosed failures and limits

1. **Strict full-crouch-walk remains a failure.** First witness: Near, 1.82 m,
   mode 4, phase 3/24, lower-calf triangle 14834 / boot triangle 16873. An
   external diagnostic reusing unchanged predicates characterizes all 288
   stress samples: **90 fail, all boot/lower-calf class 5/6**. None reports a
   different contact class. This is not just one isolated phase. The broader
   foot-weight workaround was rejected because it visibly kinked the calf.
   No weakened predicate, controller rewrite or anatomy distortion is used.
2. **World ground mismatch:** authored BankFloor is Y=0.08 m; unchanged NPC
   ground placement anchors soles at Y=0. Boots are partly hidden in bank
   captures. World fixtures and existing navigation-through-solids remain.
   Source/camera/state evidence is in `final-audit/world-preservation.json`.
3. **Coarse Far knee/cap facets remain visible**, particularly magnified static
   crouch. Native-size 12 m review is recorded honestly; no parity claim.
   Hair is an opaque short underlay, with no strand groom; no facial expression,
   blink, finger controller or cloth simulation is added.
4. Actual GL is offscreen **llvmpipe**, with shadows/reflections disabled by the
   existing backend. Software and CPU-ray images pass, but no native GPU, DXR,
   1440p/120 fps or production-game FPS claim is made. Three 24-frame 320x480
   trials give median frame-loop ms before→after: GL Near 14.95→54.39, Far
   14.07→37.06; software Near 21.49→59.26, Far 20.23→57.23. Shared-host activity
   was not excluded and construction is outside frame-loop timing.

## Reproduction

From this study checkout, use the established local environment:

```sh
export PATH=/tmp/fury-python-tools/bin:$PATH
export PYTHONPATH=/tmp/fury-python-tools
export FURY_CPU_THREADS=1 LP_NUM_THREADS=1
python3 scripts/build_metro_watch_anatomy.py --check
python3 scripts/build_metro_watch_hands.py --check
taskset -c 7 cmake --build build-metro -j1
taskset -c 7 build-metro/tests/fury_character_metro_watch_tests --ordinary
# Expected strict geometry failure, deliberately retained:
taskset -c 7 build-metro/tests/fury_character_metro_watch_tests
python3 tests/metro_watch_lifecycle_tests.py --vaultline build-metro/apps/vaultline/vaultline --source-root . --output-dir out/recheck-lifecycle
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 taskset -c 7 build-metro-asan/tests/fury_character_metro_watch_tests --ordinary
```

The sanitizer cache uses `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer`; both builds use
`SDL2_DIR=/tmp/fury-system-sdl/lib/cmake/SDL2`. Fresh captures use the saved
capture scripts/manifests. Final visual review and stress companion commands
are preserved under `final-audit`; passive reruns do not add art-time credit.
