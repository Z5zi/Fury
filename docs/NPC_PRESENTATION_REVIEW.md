# NPC presentation integration review

## Scope and status

This is the independent integration review of the detailed character presentation
wrapper and its connection to Vaultline. It supplements the model, animation,
locomotion and gameplay suites. It does not substitute numerical tests for actual
in-game image/video inspection or certify a platform that was not run.

The reviewed production path creates deterministic Near/Far articulated meshes
for a stable entity ID, then accepts read-only movement and conversation signals.
The wrapper may update its own renderer meshes and actor material; it does not
own routes, dialogue selection, colliders, transforms, mission state or names.
The presentation object and its cached mesh pointers belong to one live Scene;
that Scene must remain alive while the wrapper is used.

Validation executed on the final focused sources: the optimized standalone suite
and AddressSanitizer/UndefinedBehaviorSanitizer suite both pass all 5,665 checks.
The optimized compile uses `-Wall -Wextra -Wpedantic` and emitted no warnings.
Leak detection was disabled; full-engine/runtime checks are separate.

## Executable wrapper coverage

`tests/npc_presentation_tests.cpp` exercises the actual application wrapper,
character model, animator and Scene implementation together:

- Role mapping for player, network ghost, both crew members, guard, fence,
  enforcer, bank staff, and the authored civilian wardrobe IDs
- `enabled=false` keeps both original mesh pointers, vertex data, every material
  field, transform, collider, name, tag, visibility and detail flag untouched;
  no actors, mesh allocations or pose work are introduced
- One detailed actor receives exactly two renderer meshes. The hot path runs
  360 moving/talking frames without a heap allocation. Twenty respawns reuse
  both mesh objects with no allocation or extra Scene geometry
- Vertex/index addresses, capacities, topology, geometry identities, GPU handles
  and upload ownership survive animation and respawn
- Respawn resets deterministic ID-seeded idle geometry. Comparison permits
  2e-6 model units because the skinning layer intentionally avoids writes for
  changes below its 1e-6 per-component dirty tolerance
- Actor transforms, collision AABBs, solidity, names, mission tags and visibility
  remain untouched; an unrelated mission-tagged vault entity is also unchanged
- White, nonmetal, untextured character material preserves authored vertex skin,
  hair, fabric and equipment colors
- Idle, actual-distance locomotion and talking produce distinct geometry. Talking
  visibly changes upper-body pose and decays back to the matching idle pose
- Equal actor IDs and equal input histories yield identical mesh output;
  different IDs own independent mutable meshes
- Requested speed without travel/move weight cannot make a stationary actor walk
- At 60 Hz simulation, near geometry skins at 30 Hz, the mid-distance overlap
  skins both LODs at 15 Hz, and far geometry skins at a bounded approximately
  8 Hz (7.5 Hz with integer 60 Hz frame quantization)
- Pose/vertex/deferred/triangle counters match actual work
- Hidden actors perform no skinning while their animation clocks continue;
  showing one immediately refreshes its pose before the normal interval elapses
- Disabled LOD and nonfinite LOD thresholds safely select the near mesh
- Nonpositive/nonfinite dt, invalid positions/camera/yaw/travel, and unknown actor
  IDs do not alter state. Valid input after rejected samples matches a clean run
- Extreme finite yaw, enormous travel counters, malformed optional movement
  weights/speeds, and a subsequent return to valid movement remain finite
- Invalid talk listeners/durations are ignored. A long update uses the same
  capped 0.25-second animation step as an explicitly bounded update
- Reinstall rejects changed/invalid actor heights and changed roles
- Audit output includes stable identity seed and escaped quote/backslash IDs,
  and reports an unwritable destination as failure

## Findings corrected during review

The initial wrapper review identified and the integration author corrected:

1. A mixed float/double `std::max` compile failure after cumulative travel became
   double precision
2. Iterative angle wrapping that could hang on infinite or extremely large input;
   finite-checked `std::remainder` now handles it in bounded time
3. Missing wrapper guards for nonpositive dt and nonfinite spatial/travel data,
   plus invalid listener positions
4. NaN height bypassing the existing-actor role/height consistency check
5. NaN LOD thresholds selecting neither renderer mesh, leaving both stale
6. Main's spawn path creating an unused legacy mesh before every detailed
   respawn. Detailed actors now allocate only their cached LOD pair; legacy
   respawns reuse their original renderer mesh
7. Wardrobe mapping using IDs absent from the actual authored roster

The existing dynamic-NPC duplicate-entity issue was separately corrected by the
integration/gameplay work: a repeated stable ID replaces its existing entity
record rather than appending a second same-name entity. Wrapper tests verify its
cached geometry half; the actual main-loop flow remains runtime validation.

## Main-loop review notes

- NPC and crew feed actual traveled distance, measured speed, turn rate and
  movement weight into presentation after locomotion runs
- Dialogue remains selected by the gameplay focus/bark system; starting a real
  conversation additionally starts presentation attention/gestures
- Player crouch retains the standing rig height and lowers the articulated pose,
  avoiding double-shrinking both mesh and entity height
- Player/network facing converts Camera's +X-forward yaw convention to the
  models' +Z-forward convention. This shared correction also applies with
  `FURY_NPC_DETAIL=0`; the comparison flag selects legacy visuals, not an exact
  replay of historical orientation bugs or locomotion behavior
- Off-duty hiding, mission routes, reinforcement triggers and crew loot assistance
  remain gameplay responsibilities, separately tested in `npc_gameplay`
- Character material overrides are deliberate presentation changes. The wrapper
  is intended for ordinary actor materials, not an arbitrary imported material

## Reproduce focused verification

Run from the repository root; no shared build tree is needed:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iengine/include \
  tests/npc_presentation_tests.cpp apps/vaultline/npc_presentation.cpp \
  engine/src/character_animation.cpp engine/src/character_model.cpp \
  engine/src/math.cpp engine/src/scene.cpp \
  -o /tmp/fury-npc-presentation-tests
/tmp/fury-npc-presentation-tests
```

For sanitizer coverage, replace optimization with:

```text
-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
```

Run with `ASAN_OPTIONS=detect_leaks=0` in traced executors. Leak detection is not
claimed. CMake/CTest integration and the aggregate build are run separately by
the integration owner; focused standalone success is not an aggregate-build pass.

## Remaining runtime evidence

Actual Vaultline portraits and animated sequences must be inspected separately
for silhouette, clothing, facial readability, feet, turns, occlusion, LOD popping
and conversation framing. Software/CPU-ray renderer timings and paired gameplay
state audits establish their own results. Unit tests alone do not establish
visual quality, frame rate, network behavior or full gameplay completion.
