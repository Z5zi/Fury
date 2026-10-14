# Original articulated harbor characters

`engine/include/fury/character_model.hpp` and `engine/src/character_model.cpp`
contain Fury's original, embedded character assets. They are generated once per
actor from authored anatomical profiles and wardrobe pieces. No downloaded model,
external texture, runtime network request, or opaque third-party asset is needed.
This replaces the visual vocabulary of thirteen tinted boxes with tapered limbs,
a shaped pelvis and ribcage, defined jaw/cheeks, a short rounded nose, shallow inset eyes,
eyebrows, ears, four hair treatments, hands with overlapping thumbs, and proper
flat-soled shoes. It is deliberately stylized low-poly geometry, not a claim of
photorealistic humans or a facial-animation system.

The counts and source claims below describe this generic model factory.
Dedicated profiles in `character_profile.cpp` are reviewed separately;
[Mira](characters/mira/GEOMETRY_REVIEW.md),
[Jon](characters/jon/GEOMETRY_REVIEW.md),
[Tessa](characters/tessa/GEOMETRY_REVIEW.md),
[Owen](characters/owen/GEOMETRY_REVIEW.md),
[Lia](characters/lia/PASS_01.md), [Nell](characters/nell/PASS_01.md) and
[Pax](characters/pax/PASS_01.md) and [Rina](characters/rina/PASS_01.md) document their distinct CC0-derived
anatomical sources, higher close-range budgets and Fury-authored wardrobe,
hair, hands, footwear and rig integration. These dedicated heads are not
exclusively original geometry; their pinned source data and licenses are
recorded in each character's provenance report.

## Wardrobe and stable identity

Eleven roles have authored silhouettes and equipment:

| Role | Readable distinguishing pieces |
| --- | --- |
| Commuter | Long jacket front panels, diagonal strap, side satchel |
| Market | Rolled sleeves, bib apron, waist apron, patch pocket |
| Dock | Rolled sleeves, orange work vest, reflective strips, cargo pocket |
| Security | Uniform cap and brim, protective vest, badge, radio, belt pouch |
| Fence | Tailored lapels, scarf, dark glasses |
| Crew scout | Compact backpack, shoulder straps, sleeve/chest accent |
| Crew tech | Utility harness, repair pouch, projecting tool handles |
| Enforcer | Broad protective vest, rounded shoulder pads, front panels |
| Player | Compact back pouch, diagonal strap, chest accent |
| Network ghost | Full articulated person with cyan identification vest |
| Bank staff | Clean light shirt, pointed collar, narrow tie, name badge, no satchel |

`character_seed(stable_id)` uses portable FNV-1a. Appearance uses independent,
deterministic integer mixing and curated palettes: seven skin shades, six hair
colors, eight clothing colors, five trouser colors, five accent colors, and
independent shoulder/waist/jaw proportions. The seed must come from the persistent
actor ID, not roster order, position, current role, process RNG or `std::hash`.
Skin, hair, eyes and body proportions are independent of role. Security equipment
changes clothing, never the underlying person's complexion. LODs share identity
and rig measurements.

Vertex colors already contain the final local skin/hair/fabric palette. Use white
material albedo (`{1,1,1}`), neutral nonmetal material, and roughly `0.72`
roughness. Reapplying the former shirt tint to the whole actor tints the skin and
undoes this separation. Models contain no emissive or transparent geometry.

## Rig and coordinate contract

The 17-joint rig is Y-up and faces +Z. Joint bind axes are world-aligned, without
implicit local pre-rotations. Parents precede children:

- Pelvis → spine → chest → neck → head
- Chest → left/right upper arm → forearm → hand
- Pelvis → left/right thigh → shin → foot

The height passed by gameplay is preserved. Bind-pose sole vertices are exactly
`-height/2`, crown vertices exactly `+height/2`. Entity origins therefore remain
at the existing `height/2` ground offset. No actor ID, world transform, collider,
movement speed, route, interaction radius or AI state is part of this module.

Normalized bind measurements (multiply by actor height):

- Hip joints: X = ±0.057, Y = 0, Z = 0
- Knees: Y = −0.225; ankles: Y = −0.46; straight bind legs at Z = 0
- Thigh length = 0.225; shin length = 0.235; ankle-to-sole = 0.04
- Shoulder Y = 0.24, elbow Y = 0.105, wrist Y = −0.033
- Shoulder X = ±0.133 × appearance shoulder scale
- Elbow adds 0.024 lateral offset; wrist adds another 0.015
- Upper arm length = sqrt(0.135² + 0.024²)
- Forearm length = sqrt(0.138² + 0.015²)
- Shoe envelope: X half-width 0.038, heel Z ≥ −0.034, toe Z ≤ 0.10

The two-influence weights blend anatomy around elbows, knees, waist and shoulder
seams. Clothing and small accessories use the relevant rigid joint. Garments
are intentionally overlapping closed pieces; they are not a single welded skin
or a cloth-simulation mesh. The explicit attachment tests check a connected graph
of welded-component envelopes and visual review remains necessary for deformed
poses and oblique views.

## Fixed-topology CPU deformation

Create the immutable model and mutable renderer mesh once:

```cpp
const auto model = make_character_model(height, role, character_seed(actor_id));
Mesh rendered = model.bind_mesh;

std::array<Vec3, character_bone_count> local_angles{};
local_angles[bone_index(CharacterBone::Head)].y = 0.15f;
const auto pose = make_character_pose(model.rig, local_angles);
apply_character_pose(model, pose, rendered);
```

`CharacterPose` defaults to identity skin matrices. Animation may supply matrices
directly (animated global × inverse bind global), including two-bone leg IK.
`make_character_pose` is a convenience FK helper: local Euler radians are composed
Rz × Ry × Rx, with an optional object-space root translation. Skin matrices must
represent rigid rotations/translations, not arbitrary scale/shear.

`apply_character_pose` reads the immutable bind vertices and modifies only the
rendered positions/normals. It normalizes blended normals, never rewrites indices,
never clears/resizes vectors, never replaces renderer handles, and never allocates
memory. `geometry_identity` stays fixed. It calls `mark_dirty()` once only when a
position or normal changes beyond a 1e−6 numerical tolerance; repeating a pose
leaves revision/dirty state unchanged. Colors/UVs/opacity remain untouched. It
rejects invalid output counts, writing into the bind mesh, and nonfinite pose
matrices before modifying any vertex. The caller must keep the initialized index
order intact.

## LOD and cost

Near and Far are generated once, using the same rig, feet, role and identity.
Far uses fewer radial segments, three-ring extremities, simplified garment
corners and eye marks, and omits eyebrows, thumbs, mouth lines and small trim.
The silhouette, nose/ears, hair, clothing equipment and full motion rig remain.

The 704-combination regression population (32 IDs × 11 roles × 2 LODs) reaches:

- Near: at most 2,464 triangles, below the 2,500 budget
- Far: at most 1,120 triangles, less than 57% of each matching Near model
- Fewer than 2,500 vertices per model
- At most two rigid-bone transforms per vertex, with a one-transform fast path

Geometry is compact, but CPU skinning and renderer acceleration-structure updates
still have a cost. Runtime should update only relevant actors and use its animation
cadence/LOD policy. This module does not silently disable AI, replace routes, or
add behavior-specific scheduling. Keep a separate persistent renderer mesh per
LOD; do not rebuild either model every frame or share mutable skinned meshes
between distinct actors.

## Validation

`tests/character_model_tests.cpp` covers:

- 704 role/identity/LOD combinations with finite positions, bounded UV/color,
  unit normals, valid indices, nondegenerate faces, consistent winding and positive
  closed-body signed volume
- Exact actor height/sole convention; every joint used; parent-before-child rig
- Stable arm/leg bind lengths; connected anatomy/accessory attachment envelopes
- Deterministic profiles/geometry; useful population variety independent of role;
  distinct role geometry and consistent near/far identities
- Independent ray/triangle facial-depth checks across 32 appearance seeds: eye
  projection below 0.0025 × height and nose projection below 0.014 × height;
  surface-following eye whites/irises, shallow skin lids and rounded nose tip
- Identity/translated/articulated/reset poses; normalized deformed normals;
  unchanged colors/UVs and unrelated limb positions
- 180 deformation frames with intercepted allocation count equal to zero; stable
  vertex/index addresses, capacities, topology, geometry identity and GPU handles
- No dirty/revision increment for the same pose; invalid height/role/LOD/pose/output

Standalone, without modifying an existing build tree:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iengine/include \
  tests/character_model_tests.cpp engine/src/character_model.cpp engine/src/math.cpp \
  -o /tmp/fury-character-model-tests
/tmp/fury-character-model-tests
```

Visual review additionally renders all eleven roles in front and three-quarter views
using Fury's CPU software rasterizer, white albedo and neutral lighting. The
review output is `out/character-model-review/contact-sheet.png` (generated QA
artifact, not a runtime dependency). Scene-integrated behavior and motion tests
are separate from these model-focused checks.

Validation executed: optimized standalone suite passed; AddressSanitizer and
UndefinedBehaviorSanitizer suite passed with leak detection disabled. LeakSanitizer
could not run in the traced executor (its explicit ptrace incompatibility), so
no leak-sanitizer claim is made.
