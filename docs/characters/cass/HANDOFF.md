# Cass Vesper handoff

Core source is frozen at `2af8d6083e602a537b163ac56303c34209c595676e73eae2af54bd79fdc6e05f`,
on private base `22be1bee7a5c66c25db0ce38ab4cc526a6493382`.
The conservative dedicated art/active-QA ledger credits **128 minutes**.

The character retains NpcFence/Fence, 1.78 m, seed 0x513cdaa9, the original
skin/auburn/teal/gray-green/mauve/dark-footwear palette and shipping glasses.
Near/Far are **38,778 / 15,898 triangles**. Exact source face/hands, complete
scalp, original glasses, fitted jacket/scarf and connected wrist anatomy are
preserved. Extreme-attention review caught and corrected a real chin/mouth
weighting defect; complete-mouth and exterior-jaw regressions reject the old
weights. Canonical source positions, UVs, the rig/controller and gameplay stay
unchanged.

## Integration

`/workspace/shared/fury-deliverables/cass-final/FROZEN_SOURCE.json` identifies
all 16 owned core files, their SHA-256 values, the complete 169-file rendering
fingerprint input set and the accepted base. `owned/` contains the source.
`shared-deltas.patch` has only five existing-file changes; merge its small
semantic changes into the coordinator-owned cumulative tree:

- engine/CMakeLists.txt: add the Cass source and garment header
- engine/src/character_profile.cpp: dispatch exact NpcFence/Fence
- apps/characterlab/main.cpp: labeled Cass-only glasses-removal diagnostic
- tests/CMakeLists.txt: register Cass geometry/default and renderer tests
- tests/character_profile_tests.cpp: generic Fence fallback uses unregistered_fence

Do not copy whole shared files over newer roster work. The proposed
character_model.cpp cache patch is isolated, unapplied and excluded.
The source freeze was sent before optional packaging. Public/GitHub writes
remain paused; this task made none.

[Original/current body](../../images/characters/cass/body-before-after.png),
[visible attention correction](../../images/characters/cass/attention-before-after.png),
[final Near attention](../../images/characters/cass/attention-near-all-side.png),
[final Far attention](../../images/characters/cass/attention-far-all-side.png),
and [actual shop portrait](../../images/characters/cass/actual-world-shop-portrait.png)
provide a compact review set. These images preserve source pixels; the
before/after sheets add only captions and layout.

## Evidence and validation

Authoritative evidence: `cass-study/cass-evidence/accepted-final/`.
It includes all-side material/clay, hand/boot/garment details, ordinary and
stress poses, front/back/profile attention, three renderer portraits,
LOD-distance comparisons, actual Vaultline stills and live sequences, exact
stress vertices/matrices, source/dump/topology and renderer results. Every final
capture identifies the frozen fingerprint. `records.json` and
`closeout-records.json` keep commands, times and exit codes. `VALIDATION_STATUS.json`
provides the completed consolidated state.

Ten final world-state records and their capture arguments match the baseline
exactly. Shop scenery occludes several views; clear portrait/raised/rear views
and live-route segments are retained without moving Cass or deleting scenery.
The sequence cadence is offline simulation, not an achieved-FPS claim.

Exact source witness: `cass-evidence/resumed-final/JAW_MOUTH_SOURCE_WITNESS.json`.
It records generated/upstream IDs for 418 mouth positions/484 UV corners and
308 independent exterior-chin positions, plus matched original/corrected
images. Rejected variants and original failures remain available separately.

## Explicit limits

The default Cass test still fails the strict 1e-7 m split-corner cache seam and
full-crouch-walk calf/toe and jacket/thigh contact assertions. The explicit
ordinary-geometry scope passes; it must never be reported as a passing full
default test. The shared cache proposal was deferred, and ordinary anatomy was
kept instead of the rejected distorted calf. ASan/UBSan scope and default exit
codes are reported separately; LeakSanitizer is not certified.

This is a stylized source-anatomical asset with static fingers, rigid cloth/hair
and opaque lenses. There is no commercial-game fidelity or hardware-GPU
performance claim. See GEOMETRY_REVIEW.md for exact checks and reproduction.

## Cumulative integration update

7 October 2026: the owned art files are now applied to the cumulative main source.
The original handoff above records the private freeze. Shared registration and
generic-fallback fixtures were merged surgically; no assertion predicates were
weakened. Integration captures and complete-state comparisons are in
`docs/validation/characters/integrated-thirteen-20261007/` after curation.
Private validation is not substituted for those cumulative checks.
