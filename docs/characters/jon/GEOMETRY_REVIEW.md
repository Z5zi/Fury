# Jon Keel geometry and deformation review

Review date: 2026-10-06. The complete numerical regression passes on the applied
`NpcCivB` / `Market` profile in ordinary and focused ASan/UBSan builds. Jon's
roster height is 1.70 m. This review calls the real `make_character_profile`
entry point and skins its actual vertices; it does not substitute a proxy mesh.

## Reviewed geometry and CPU budget

| LOD | Vertices | Triangles | Width / height | Depth / height | Signed shell volume / height³ |
| --- | ---: | ---: | ---: | ---: | ---: |
| Near | 23,426 | 38,716 | 0.356216 | 0.176986 | 0.0170610 |
| Far | 9,795 | 15,912 | 0.356110 | 0.173883 | 0.0170776 |

Counts are identical at 1.35, 1.70 and 2.10 m. The far mesh removes **58.9% of
triangles and 58.2% of vertices**. Both retain all 4,468 source anatomical head
positions and all 8,876 source head triangles. The reviewed limits remain
40,000 near / 16,000 far triangles, with 1,284 / 88 triangles of headroom.
No budget increase was used to make the result pass.

Both shoe soles touch `-height/2` at rest, and the hair crown reaches
`+height/2`. At Jon's roster height those planes are -0.85 m and +0.85 m.
The signed-volume diagnostic sums contributions from overlapping authored
shells and open hair cards; it is not a watertight boolean-union or physical
body-volume measurement. These are local
asset budgets, not a frame-rate promise or a whole-scene geometry budget.

## Regression contract

`tests/character_jon_tests.cpp` checks both LODs at all three heights:

- Finite positions and colors, unit normals, bounded opaque material values,
  valid indices, positive triangle area, and winding consistent with bind normals
- UVs inside atlas content regions and one material region per triangle; Jon's
  nine base materials occur in both LODs, with HairStrands added only near.
  Metal hardware is not required for this cloth-button/strap wardrobe
- Near strand cards are real geometry, rigidly attached to the skull, with the
  documented 0.4 alpha cutoff and no alpha blending. Far retains opaque hair
  volume without cards. The cards add 768 near triangles and 648 vertices
- Finite normalized influences, including all optional third indices/weights;
  all 17 rig joints drive real vertices and form an acyclic parent-first rig
- Requested height, grounded soles, bounded body extent and positive plausible
  signed shell volume
- Bit-identical rig, appearance and identity across LODs, opt-in dual-quaternion
  skinning at both LODs, real reduction and unchanged triangle limits
- Exact preservation of every source head position and ordered triangle;
  original source UV corners mapped through `character_surface_uv`, preserving
  shared normals and skin weights at duplicated UV seams
- Morphed source head/neck landmarks used as rig pivots; both eye globes fitted
  to source centers/helper extents and following the skull
- Repeated construction of both LODs is bit-deterministic and retains Jon's
  seeded appearance and shared immutable PBR textures
- **1,188 actual sampled and skinned poses:** 33 frames for each of idle, walk,
  run, talk/attention, crouch and crouch-walk, at three heights and two LODs
- Every sampled vertex remains finite with a unit normal; every triangle retains
  positive area; actual shoes stay above the ground; adjacent-frame motion is bounded
- Skinning preserves vertex/index storage and capacity, index order, mesh
  identity, renderer handles, tint, UVs and opacity. Identical poses do not dirty
  the mesh twice, and bind geometry remains unchanged
- Closed, consistently oriented Pants shell edges after welding only
  rounding-scale coordinate drift at seams
- **594 actual knee intersection checks:** every sampled run, crouch and
  crouch-walk frame across all heights/LODs, plus the bind poses
- **186 bit-exact generic fallback cases:** unrelated roster identities,
  all wrong Jon roles, case/suffix variants, empty identity and embedded NULs
- Invalid heights, roles and LODs fail explicitly

Exact comparisons include `third`, `third_weight` and
`dual_quaternion_skinning`. C++ padding and fresh runtime mesh identity counters
are intentionally excluded from repeated-construction comparisons; they are not
authored geometry or identity. Runtime mesh identity is checked separately
throughout skinning.

The knee test checks proper triangle-interior crossings between nonadjacent
triangles of the same leg in the authored knee/calf/thigh region. Boundary
contact, shared UV-seam vertices and adjacent faces are not counted as proper
crossings. It does not claim general cloth simulation, apron/body collision
freedom, or collision freedom in every possible pose between samples.

## Defects found and corrected

The initial draft had 53,352 near / 24,596 far triangles, exceeding both budgets.
Seven near and one far front-yoke triangles also disagreed with their normals.
The winding failures were real negative dot products, not zero-area roundoff.
The corrected yoke and reduced authored detail pass without weakening either
winding or budget checks.

The first deformation test checked finite geometry, area, unit normals and
storage stability. An additional posed-normal audit exposed a gap in that
coverage. Disagreement with a transformed bind normal alone does not prove a
fold: bind-normal skinning omits spatial weight gradients. The review separately
recomputed normals from posed triangles and tested actual triangle intersections.
Recomputed normals removed most shading disagreement while genuine posterior
calf/thigh intersections remained in deep crouch.

A separate far-mesh crossing was traced to the trouser circumference seam.
Angular noise frequencies 0.6 and 1.8 gave different radii at 0 and 2π. Integer
frequencies close that seam without adding triangles. The closed-shell edge
assertion now guards this defect; the shared sculpt helper also rejects
meaningful endpoint mismatch instead of hiding it as rounding error.

Dual-quaternion skinning alone did not eliminate the deep-flexion contact.
Profile-only experiments compared linear and dual-quaternion skinning,
moderate blending bands, and posterior shaping. The applied correction uses:

- A shin/thigh blend over normalized Y `[-0.325, -0.125]`
- A posterior trouser offset in Z of
  `0.012 * exp(-((y + 0.225) / 0.20)^2) * max(0, -sin(angle))`

The maximum authored posterior offset is 20.4 mm at Jon's 1.70 m height; the
front surface and X coordinates are unchanged. This combination cleared all
99 run/crouch/crouch-walk samples per LOD in the independent 1.70 m comparison,
then passed the expanded 594 checks in the applied asset across all heights.
Standing/walking silhouette and visual quality still require the separate
real-frame review; numerical clearance alone is not visual approval.

## Mira preservation and shared changes

Before later intentional corrections, the former combined profile was recovered
from staged Git blob `5314686eeff33393823de423705df5edad3db267`, compiled under a
separate entry point, and compared against the new dispatcher, `character_mira.cpp`
and shared `character_sculpt.hpp`. Both LODs at 1.35, 1.75 and 2.10 m matched bit
for bit in position, normal, tint, opacity, UV, index, influence, rig, appearance
and identity fields. Texture pointer, individual flag and switch distance matched.
This verifies the structural split itself.

Subsequent shared Face-chart packing, intentional circumference/endpoint fixes,
and the related Mira deformation correction are separate changes. The earlier
comparison does not claim these later UV or shape edits equal the old snapshot.

Mira intentionally received the same dual-quaternion mode and 0.20h knee blend.
Her denser leg sampling exposed one remaining crossing at crouch-walk frame 9
with Jon's 0.012h posterior offset. Raising only Mira's offset to 0.013h cleared
all sampled crossings; at 1.75 m this is at most 1.75 mm of additional shaping.
Her current regression passes ordinary and focused ASan/UBSan checks: six
static/source-head profiles, 1,188 poses, 594 knee-intersection checks, closed
Pants shells, and 186 unaffected fallback cases, retaining 35,946 / 15,970
triangles. She retains opaque modeled hair without Jon's strand cards.

Mira's fallback list excludes the newly individualized Jon/Market pair, and its
comparisons and weight validation cover the added skinning fields. See her
[corrective follow-up review](../mira/GEOMETRY_REVIEW.md#corrective-follow-up-2026-10-06)
for the distinction from her first-pass checkpoint.

## Verified source snapshot and instrumentation

The applied-source builds use these SHA-256 hashes:

| Source | SHA-256 |
| --- | --- |
| Jon profile | `d767806acdb087a748c087a6eb423bdfae0b3038b48b46421f6efb1ff0d673ca` |
| Shared sculpt helper | `763d6a79a1066a2c4068996a0f154c59cbd3b8ef88e4b01f48c983b25d028029` |
| Skinning/model implementation | `5feaa77d737f7e39c8e05270118bacb63feb955b3a312e5137b55f9ded70582e` |
| Shared atlas | `f945285626bf7b6302f875214d02eab1f9d427db1c8cf6ae96ad5f6ed3714859` |
| Generated Jon anatomy | `5ecac83a75fe83a6ca78fc088b45723213167b6b27496e1a36a917e69c7cedac` |
| Mira profile | `fa82c9f87f0809a3f8893dacf7186448b80fbae9f122ccc12f3a3800602fcc75` |

Independent C++17 builds use `-O2 -Wall -Wextra -Wpedantic`. The focused sanitizer
build uses `-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined`; the tests,
profile dispatcher, Jon, Mira/shared sculpt code, generic model/skinning,
animator, atlas and math are instrumented. Both tests pass with no reported
ASan/UBSan errors. Leak detection is disabled for the executor's known ptrace
limitation (`ASAN_OPTIONS=detect_leaks=0`). The unchanged engine archive supplies
texture/mip utilities and other dependencies; this is not a claim that every
renderer or dependency was instrumented. The shared build was not mutated by
these independent checks.

The integrated CMake targets are `fury_character_jon_tests` and
`fury_character_profile_tests`; CTest names are `character_jon` and
`character_profile`:

```sh
cmake --build build --target fury_character_jon_tests fury_character_profile_tests -j2
ctest --test-dir build -R '^character_(jon|profile)$' --output-on-failure
```

Rerun the checks after later geometry, weighting, seam, UV or skinning changes.

The last card-fit refinement reduced its scalp offset to keep the alpha-card
roots beneath the opaque hair mass. The isolated snapshot and its source-hash
manifest were refreshed at13:10UTC, and optimized plus ASan/UBSan runs passed at
13:10–13:11UTC. The current profile hash above matches that tested snapshot.
