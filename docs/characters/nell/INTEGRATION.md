# Nell private integration scope

Verified base tree: `9e21e9a1aae79fb7abd7a9cf3e177349581ef14d`.
This private tree must be integrated surgically after separately completed Lia
work. Never overwrite a newer shared tree wholesale.

Nell-owned files:

- `engine/src/character_nell.cpp`, `character_nell_shirt.hpp`
- `engine/include/fury/generated/nell_anatomy.hpp`, `nell_hands.hpp`
- `scripts/build_nell_anatomy.py`, `build_nell_hands.py`
- `third_party/nell_anatomy/` recipe, manifest and generated stats
- `tests/character_nell_tests.cpp`
- Nell-only documentation and verified evidence

Separately identified private integration edits:

- `engine/CMakeLists.txt`: register Nell source and shirt header
- `engine/src/character_profile.cpp`: exact `NpcCivAsh` + `Market` dispatch
- `tests/CMakeLists.txt`: register Nell-specific checks
- Existing Mira/Jon/Tessa/Owen fallback tests: replace their now-individualized
  `NpcCivAsh` fallback fixture with `unregistered_market`; all assertions remain

No renderer, shared skinning, materials or animation source is changed here.

Separately approved world validation changes, applied after original art-stage
captures, are `apps/vaultline/main.cpp` and `apps/vaultline/npc_roster.cpp` from
`nell-route-audit/fixture/{fixture.patch,route.patch}`. They move only two
Ashcourt storage props and substitute the audited 16-point Nell route. Home,
spawn, stable ID, height, collider, speed, schedule, dialogue, shop and mission
semantics remain fixed. The steady loop is intentionally longer: 49.87 to
68.17 seconds. This is a world correction, not an art improvement. Dynamic
vehicle avoidance remains unimplemented.

The audit's `route-tests.patch` is based on newer main source that includes Lia.
It is not applied to this older Owen-based private copy. Parent integration
must preserve Lia's fingerprint while accepting Nell's new fingerprint and the
persistent world-runtime gate. The exact final Nell executable is verified by
the audit's focused runner, independently of the old private fingerprint test.
Do not label an aggregate post-route run as passing until that integration is
complete. Original and revised world evidence are retained separately.

## Final handoff boundaries

The 7 October source/hash recovery review found no changes to the nineteen
code/data/test files in the 23:59 code-only handoff. All 157 rendering-input
hashes still match the optimized snapshot. The final handoff adds Nell-only
documentation and selected original-engine evidence; it makes no source,
world or shared-file changes.

Preserve newer Lia registration and tests when inserting the shared deltas.
Any later actor suite that still uses `NpcCivAsh` as its generic Market fixture
needs the same `unregistered_market` substitution, without deleting assertions.
The existing source patch contains only the four older fallback suites; it
cannot know the newer actor suites in main.

Integrated aggregate validation, route-fingerprint reconciliation and final
serialized frame-time/memory measurement belong to the integrating tree.
The private 38-suite result does not certify that later tree. See
BENCHMARK_PROTOCOL.md; the final integrated measurement remains pending.
