# Pax geometry and motion checks

This is a development checkpoint, not visual acceptance or AAA parity.
The final engine source fingerprint is
`afa63a7d11527084bd92ee001ef88160c2fdb8ad45fa43597d7df413866e105f`.
Source was force-configured before the final build. Private base tree is
`9e21e9a1aae79fb7abd7a9cf3e177349581ef14d`.

## Bounded model checks

`tests/character_pax_tests.cpp` checks both LODs for finite geometry, normalized
normals, valid indices/material coordinates, weights, source correspondence,
height, eye seating and actual opaque scalp coverage. The final result has
zero tiny or reversed triangles. Opaque cap checks include actual triangle
crossings plus sampled source vertices, edges and interiors, not just hair
cards. Near checks 1,392 cap triangles and 23,369 samples, with minimum
normalized clearance 0.00172042; Far checks 360 cap triangles and 23,321
samples, minimum 0.00298323. Both have zero missing coverage or crossings.

The coordinate-welded vest is one closed shell (Euler -2). Four band shells
are closed (total Euler 8). Pouch and flap are two closed pieces (Euler 4).
The shirt has four expected open boundaries, at the neck, hem and cuffs
(Euler -2). Each joined hand/forearm is connected with one 20-edge cuff
boundary (Euler 1). Spatial welding uses neighboring buckets and a true
position tolerance; bucket equality alone is insufficient at cut edges.

233 pose/attention samples per LOD check stable mesh storage, UVs, colors and
welded position seams. The smallest posed/reference area ratio is 0.00299519
Near and 0.0469908 Far. A further 48 contact poses per LOD report zero
vest/shirt, band/vest, neck/shirt, hand/cargo and opposite-knee crossings.
An injected vest penetration is detected as a negative control.
These finite samples do not constitute a proof over every possible pose.

## Corrected findings

Early rendered and numerical checks found shoulder clipping, hourglass-like
underarm pinching, spread hands, open/incorrect shell edge winding, near-equal
clip intersections producing false internal walls, eye-rim protrusion,
concave shading corners and reflective strips cutting through cloth in gait.
The final construction corrects these specific findings. The tapes now use
support triangle boundaries and matching weights; simply raising a regular
strip farther from the cloth was not sufficient.

Thin-shell normals require normalized tangent directions and degeneracy
checks before crossing when using Fury's small-vector normalization helper.
Normals must also be cleared before calling the accumulating Sculpt finish
routine after render-corner splitting. These are local model corrections;
shared shader, material and skinning behavior did not change.

## Regression evidence

The final selection of 18 character/NPC/visibility/rendering/application tests
passed in 57.67 seconds. Two actual offscreen OpenGL character tests passed
in 9.40 seconds with Mesa llvmpipe. The NPC runtime test passed in 88.03
seconds. The source head and hand importer `--check` commands passed.
Logs are in the separate evidence directory `final-checks`.
OpenGL here is software Mesa, not physical GPU validation.

No gameplay roster, route, schedule, collider, dialogue or mission source was
changed. CMake/dispatch and obsolete generic fallback assertions are separate
integration edits. Characterlab crouch-walk and bounded attention controls are
in a separately verified tooling patch, with default-state/pixel checks.
