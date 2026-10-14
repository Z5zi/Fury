# Rook individual pass — 7 October 2026

Rook now has an independently shaped connected face/neck, connected source hands, a continuous sewn shirt, supported utility harness, lined hanging pouch and rounded articulated work boots. The original CrewRook identity, 1.70 m height, CrewTech appearance role, Muscle network assignment, deterministic palette and left/trailing follow offset remain intact. No actor placement, scenery, collider, navigation, mission rule or production animation sampler changed.

The individual art and active QA allocation is **120 minutes**, conservatively rounded down from 120 minutes 9 seconds; see [WORK_LOG.md](WORK_LOG.md). Outages, uncertain resumption intervals, shared diagnostic work, passive waits and packaging are excluded. This allocation is not a GTA VI parity or finished-AAA claim.

**Ordinary contact validation is not fully passing.** The unchanged shared gait flattens a planted shin into its level shoe at the real crew speed of 6.5 m/s. The retained ordinary test reports eight contact samples: phases 14/32 and 30/32, Near/Far, at 1.35 and 1.70 m. This is distinct from full-weight crouch-walk stress failures. The parent deferred shared gait redesign after rejecting an isolated toe-pivot proposal. Rook's anatomy was not distorted and the tests were not weakened to hide either problem. The complete strict sweep also exposes six Near trouser self-contacts where the thigh and calf fold together at phases 10/32 and 26/32, at all three tested heights; these are retained as strict stress failures.

## Art and provenance

The compact broad jaw/chin, fuller neck, restrained cheek planes, projecting nose and modest upper-lid fold come from an original numeric morph recipe. Source-correspondent comparisons distinguish the face from all eight accepted baseline faces. Independent final clay review against Sparrow also retains a broader lower face/neck and flatter brow-to-bridge transition before tint or equipment cues.

The source head has 4,442 vertices and 8,832 triangles. Each connected hand has 1,649 physical source positions, 3,276 Near triangles, a locked twenty-edge wrist, and an independently reduced 520-triangle Far version. The static relaxed finger curl is monotonic; individual finger animation is not implemented. The head and hands use only the previously pinned CC0 MPFB/MakeHuman graphical base and numeric target data. Twenty-four input hashes and all retained source/UV corners are independently reconstructed. No GPL addon code was installed or executed, and no commercial reference pixels or assets enter the character.

The shirt has four proper physical openings, turned neckline and sleeve hems. Original fitted support surfaces construct the harness through front, shoulder and back; it has thickness, a real open metal adjuster and belt attachments. The pouch has a cavity, inner lining, shaped gussets, a sewn slip pocket, two broad folded leather hangers and two retained tool handles. The groom is a coherent auburn cap/side/rear volume with short alpha roots, still a deliberately modest procedural treatment. Material/clay, both LODs and motion views were inspected; rejected straight temple strips, rectangular temple overlays and rounded-outline pouch hangers remain only in intermediate evidence.

Final budgets: **25,468 vertices / 38,893 triangles Near; 10,289 vertices / 15,916 triangles Far**. The original generic model was 2,332 / 1,072 triangles. Remaining budget is 1,107 / 84 triangles under the 40,000 / 16,000 ceilings.

## Reproducible source and integration

- Baseline tree: `b7bb006f1748fbe4f0e6ce46a19a7631a6166e0a` (accepted eight actors)
- Frozen private source fingerprint: `6da7e1e640e510e1c231a4dff1abe06ed5028cf7e6ed64a21e4e3b347c3f66e2`
- Unchanged sampler SHA-256: `6eca745440820d188ebd96c4408c41eee5cd496ce7ec38d7db2e01a3f37b21d4`
- Source freeze: `/workspace/shared/rook-final-freeze/SOURCE_MANIFEST.json`, `SHA256SUMS`, twenty owned files, and `rook-integration.patch`

Merge the small registration/CMake/fallback-test additions into the coordinator's current cumulative source. Do not overwrite other characters using this older isolated base. Existing licensed input assets and importer helpers remain shared dependencies in place. All actor-owned files and complete transitive sanitizer inputs were verified again after the source freeze.

The optional characterlab `--speed` and `--idle-time` controls are a separate diagnostic delta; defaults retain exact original pixels, and invalid/ambiguous inputs are rejected. The production crew capture and live-presentation refresh are separately owned Sparrow diagnostic changes. They do not advance animation during a frozen review or change controller state. Neither diagnostic is a renderer rewrite.

## Validation matrix

| Scope | Result | Evidence in private `rook-study` |
| --- | --- | --- |
| Independent pinned graphical-data reconstruction | Pass: 24 inputs, 26,496 head source corners, both hand topologies, six corrupt-output controls | `out/rook/final-source.json`; head/hand `--check` logs |
| Ordinary geometry/source/material/rig/ground/seam checks | Pass: three heights, both LODs, 228 ordinary animation/attention poses per model, zero per-frame skinning allocations | `out/rook/final-geometry.log` |
| Ordinary triangle contact | **Fail: eight actual 6.5 m/s shin/boot samples**; idle, slower walk/run, talk, stationary crouch and attention samples otherwise clear | `out/rook/final-contact-ordinary.log` |
| Strict triangle contact | **Fail: 60 crouch-walk shin/boot samples, six Near knee-fold trouser self-contacts, plus the same eight actual-speed samples**, with twelve injected contact controls at each height/LOD | `out/rook/final-contact-strict.log` |
| Strict ground geometry | **Fail: Near upper-boot crouch-walk stress**, approximately 0.884 mm below floor at canonical height; planted sole remains the support | `out/rook/final-geometry-strict.log` |
| Software and CPU BVH/path renderer | Pass: exact held-frame and restoration pixels, dynamic mesh/BLAS updates, 30/15/8 Hz cadence | `out/rook/final-renderers.log` |
| Actual OpenGL | Pass using SDL offscreen + llvmpipe; no skip/fallback accepted | `out/rook/final-gl.log` |
| Generic fallback/profile regression | Pass: 180 bit-exact generic cases; an unregistered CrewTech ID replaces the now-individual Rook fallback example | `out/rook/final-profile.log` |
| Production presentation/gameplay | Pass: 5,938 presentation and 780,527 gameplay checks | `out/rook/final-npc-{presentation,gameplay}.log` |
| Production world follow/heist/reset | Pass: 1,216 real update ticks, 13 snapshots, all four follow headings, real E/timer progression, extraction and reset | `docs/validation/characters/rook/world-{before,after}` |
| ASan/UBSan | Pass for ordinary geometry, software/CPU-ray and real GL; contact exits 1 on the same eight known samples with no sanitizer diagnostics | `out/rook/sanitizer-final/execution.json` |

The ordinary contact test remains registered as a failing test; there is no `WILL_FAIL`, disabled case, relaxed tolerance or timeout increase. Strict scopes remain separately invocable. Shared cached-pose floating-point seam differences below a micrometre are documented as numeric precision only, not an excuse for visible garment gaps.

## Matched pixels and real gameplay

`baseline/characterlab-original` and `baseline/vaultline-original` retain their original hashes and embedded source fingerprint `8015066ee082524146d5888024447c1ed5198d719529a98c1484942ce8da856c`. Original executable SHA-256 values are recorded in `baseline/executables.sha256` and source hashes in `baseline/source_manifest.json`.

The complete original and final stage sets contain 64 matching camera/state views each: body/face, material/clay, Near/Far, front/back/both sides/four obliques. `out/rook/STAGE_PAIR.json` asserts every camera, light, height, pose and LOD match. `out/rook/final-motion` adds 96 idle/walk/run/talk/stationary-crouch/full-crouch-walk frames. `out/rook/final-witnesses` keeps explicit real-speed and crouch failures, including wider framing with unchanged actor geometry/state.

A separate original-model executable uses the preserved original dispatcher with the current common diagnostic/application code. Its virtual source fingerprint is `7ffc21db1bd1c1291e0666f48deeea1183fce9b8f8f006ae0c7077487c40fdb5`; link maps exclude the new Rook and stale dispatcher/build-info objects. It is clearly a derived matched baseline, not the untouched original executable. Two default stage probes match the preserved original executable pixel-for-pixel.

All thirteen world pairs assert exact state, simulation tick and review camera equality. Seven snapshots clearly show Rook; retained walls, counters, monitors or foreground objects obscure some of the other views. Those obscured frames establish matched state, not readable character art. The actual approach, breach, escape, success and reset views are clear. Rook travels 64.492 m, reaches the production 6.5 m/s peak and stops at real reset. Actual scene fixtures and other actors remain present. The diagnostic positions only the player review camera and freezes existing state for rendering; it is a deterministic production-controller audit, not a navigated UI playthrough or scripted `--heist-capture` sequence. The refreshed reset view has idle arms. Its split stance is retained production `gait_started` foot spacing, distinct from the earlier stale whole-mesh witness.

## Cost and remaining limits

With one CPU core, final 480×640 body stage snapshots measured 44.6–104.9 ms Near (median 73.8 ms), 38.4–99.8 ms Far (median 55.3 ms). Full-world 640×480 software snapshots measured 317.5–851.4 ms. These are diagnostic frame costs on a shared executor, not interactive FPS guarantees. Actual 4-spp/two-bounce CPU-ray images and real GL pixels were also inspected; a zero GL CPU metric means unreported, not free rendering. See `out/rook/FINAL_COSTS.json` and `out/rook/renderer-review`.

The face has no facial blend-shape performance, hands have no separate finger animation, grooming remains procedural, and no cloth simulation is added. Full crouch-walk and real maximum-speed shared gait defects remain visible and reproducible. The exact-source sanitizer build took 119.9 seconds. Its ordinary geometry run took 193.5 seconds, ordinary contact 155.3 seconds, software/CPU-ray regression 39.4 seconds and real GL 10.3 seconds on one assigned core. It reuses 44 verified engine units and recompiles four, with link-map proof and every project dependency compared against frozen and current bytes. Shared absolute external headers are hashed at reuse time; historical external-header identity is not claimed. No leak-clean claim is made: LSan cannot run under the executor's ptrace restriction. No public write, release or main-tree integration was performed by this worker.

## Cumulative integration note

The owned art is integrated into the current cumulative source. Private paths
above identify original evidence; INTEGRATED_FILE_MAP.json under
`docs/validation/characters/rook/` maps every indexed final file into this
repository. Cumulative CTest additionally registers both strict geometry and
strict contact scopes. The ordinary contact gate remains red. Integration
review is continuing while Sparrow receives a visible bind-hem correction.
