# Preserve visible characters across bank lighting zones

The individual-character review exposed a pre-existing world-render bug: Lia
vanished for one second during a normal bank patrol, even though she and the
camera remained inside the same room. The entire actor was culled when its
posed bounds left the security-desk lighting sub-zone. The larger bank wall
was also incorrectly hidden. Both the generic and individual model reproduced
exactly frames 86–97 of the192-frame recorded route.

## Conservative scope

Lighting selection still uses the first matching zone. Near-door and deep-core
conditions are unchanged. When sector hiding is active, `visibility_sector`
now combines the original same-tag volumes connected to that lighting zone
through overlap or a shared face. It excludes disconnected rooms, different
tags, mere edge/corner contact and connections invented only by expanded
margins or the empty corner of an accumulating bounding box. The existing
1.1/1.15/1.1 margins are applied once to the resulting conservative room box.
Single-zone jewelry, loft and depot sectors remain bit-identical.

The patch changes the sector-focus assignment, not NPC routes, colliders,
movement, materials or lighting definitions. The visibility pipeline still
performs outside-room and distance culling. The larger conservative bank box
can submit extra interior geometry; it is not a portal/occlusion solver or a
navigation repair.

## Source-pinned verification

Corrected production source fingerprint:
`75878d9b386b76460fb31b7001f0ed9562b4489e7fab4e4df3553249d865b229`.
Only `engine/include/fury/interior.hpp` and `apps/vaultline/main.cpp` change
between the frozen art binary and this corrected binary. All model, material,
renderer and animation files are byte-identical. The earlier stage/backend
art images intentionally keep their original source fingerprint; they are
not falsely relabeled as newly rendered evidence. Fresh face/body captures
are pixel-identical to their prior images.

All **43 suites pass**: 39 non-GL tests in 195.10s, then four actual llvmpipe
OpenGL tests in 11.33s. The new focused test passes1,567 checks in 0.94s. It steps
the real generic and individual Near/Far meshes through all 192 route frames,
reproduces the original12 missing frames, and keeps the actor visible with the
new room bounds. Additional tests cover transitive connectivity, disconnected
and different-tag rooms, gap/empty-corner negative cases, original lighting,
exact single-zone behavior and outside-room/distance rejection.

Fresh actual CPU raster footage retains all 192 frames. Every frame was inspected
in sequential contact sheets, with original-resolution checks around85–98 and
later occlusions. Lia is present throughout the old missing interval and turns
normally at 98. Complete serialized NPC, crew and mission state at the recorded
endpoint is identical before/after. This is not a per-frame state log or a full
manual mission playthrough. The video uses 12fps simulated capture cadence,
not measured real-time performance.

## Evidence and remaining limitations

- `docs/videos/characters/lia/visibility-repair-comparison.mp4` compares the same
  individual Lia before/after the culling repair, all 192 frames/16s
- `docs/images/characters/lia/visibility-repair.png` shows the exact missing
  interval and subsequent restored frames; original footage remains retained
- `docs/validation/characters/lia/visibility-repair/` contains commands, hashes,
  all-frame review sheets, tests and endpoint/source-difference verification

The repaired wall makes an existing guard-navigation defect visible: a guard
emerges through it around frames 106–107. Guards obscure Lia around123–125;
vehicles obscure much of the body later. These are retained and disclosed.
This is a culling correction, not a claim of unobstructed footage or dynamic
obstacle avoidance. Guard geometry/navigation remains a later character audit.

At the same frame 192 camera/state, submitted raster geometry rises from 304,148
to 411,904 triangles and 296 to 1,046 instances because the complete bank interior
is retained. These counts are direct runtime evidence. Those captures ran with
other work active and different CPU-worker settings, so their frame times are
not presented as a controlled speed comparison. The separately serialized
art-only benchmark in INTEGRATION_REVIEW.md predates this culling correction.
CPU cost of retaining the larger room must not be hidden behind that earlier
benchmark or claimed as an optimization.

## Corrected backend turnaround checks

Fresh actual OpenGL on Mesa llvmpipe and CPU ray mode both visibly retain Lia at
frame 90, inside the previously missing interval. Both use the same camera and
320×320 output. CPU ray mode explicitly uses the existing ambient approximation
(FURY_TRACE_MODE=ray). A separate CPU path-mode1spp capture executes with zero
validation errors but is extremely dark/noisy; it cannot establish visible
model quality or equivalence. Animated geometry resets path accumulation, so
90 simulated frames do not accumulate90 samples for this moving scene.

All three serialized backend endpoint states match exactly. Commands, explicit
mode environment, source hashes and reports are retained. The backend sheet
labels resolution and mode; its raster tile is reduced from480px for layout.
GPU-hardware speed is not inferred from llvmpipe, whose zero timing/geometry
fields are unimplemented metrics, not zero-cost rendering.
