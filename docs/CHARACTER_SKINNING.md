# CPU character skinning

`apply_character_pose` deforms the existing CPU vertex buffer. Software raster,
CPU ray tracing, OpenGL, and other backends continue consuming the same mesh;
there is no separate shader skin, new topology, or per-frame heap allocation.
Bone conversion uses a fixed stack array of `character_bone_count` elements.

## Compatibility and authoring

`CharacterInfluence` preserves its original first three fields and appends:

```cpp
std::uint8_t third{0};
float third_weight{0.f};
```

An existing `{first, second, first_weight}` initializer still means two-bone
skinning. Three-bone weights are `first_weight`, `1 - first_weight - third_weight`,
and `third_weight`. All indices must be valid, including zero-weight slots. Both
stored weights must be finite and nonnegative, and their float sum must not exceed
one. A negative residual caused only by float subtraction at that boundary is
clamped to zero. Weights are never silently repaired beyond that rounding case.

The third influence permits apron vertices to bridge pelvis and both thighs.
It is an authoring facility, not cloth simulation or collision response.

`CharacterModel::dual_quaternion_skinning` defaults to `false`. The two-bone LBS
branch retains its original arithmetic and write tolerance, including endpoint
weights. With DQ enabled, single-bone vertices retain the matrix path; multiple
positive slots naming the same bone also resolve to one rigid transform.
Three-bone LBS remains available when the model flag is false.

Positions refresh only when their own cached value differs by more than
`1e-6` in at least one component. Normals retain the existing refresh on either a
position or normal change. A normal-only refresh does not copy a sub-epsilon
position: hard-edge/UV split corners with identical bind positions and weights
therefore remain bit-identical in position across cached pose histories. This
fixes the previous normal-dependent seam gap without changing skinning math,
weights, geometry, allocation behavior, or the cache tolerance.

## Rigid dual-quaternion blending

This implementation was written from the mathematics in Kavan, Collins, Zara,
and O'Sullivan's [2007 paper](https://www.cs.jhu.edu/~misha/ReadingSeminar/Papers/Kavan07.pdf)
and [2008 paper, sections 4 and 4.1](https://users.cs.utah.edu/~ladislav/kavan08geometric/kavan08geometric.pdf).
No external program source was copied. The authors also maintain a
[method overview and limitations](https://users.cs.utah.edu/~ladislav/dq/index.html).

For each bone, a proper rigid matrix is converted to a unit rotation quaternion
`r` and dual part `d = (0, translation) * r / 2`. Matrix conversion selects the
trace or largest-diagonal branch, so rotations near 180 degrees are well-defined
around every axis. Before blending, the signs of both quaternion parts are aligned
to the largest active weight's rotation hemisphere.

The weighted real and dual sums are divided by the real sum's length. Removing
the component of the dual part parallel to the normalized real part enforces the
unit dual-quaternion constraint. Position receives quaternion rotation plus the
vector part of `2 * d * conjugate(r)`; normal receives rotation and normalization.
The positive reference weight bounds the real blend away from zero.

All influence and pose validation happens before output writes. DQ requires an
affine matrix with orthonormal rotation columns and determinant +1, allowing
`1e-4` float error in the rotation tests. Scale, shear, reflection, and projective
matrices outside that tolerance throw `std::invalid_argument`; they are not
reinterpreted as rotations. The existing LBS path keeps its finite-matrix contract.

## What this solves and what remains

A blend of two matrices rotating around one pivot can shrink the radius of a
vertex toward the pivot. The DQ midpoint follows a rigid rotation and preserves
that radius. This addresses the collapse mechanism seen around tightly bent
knees without requiring an extremely broad blend band.

DQ does not guarantee non-intersecting skin, resolve a raised thigh passing
through clothing, or impose anatomical joint limits. Independent vertices still
receive different transforms, which can produce bulging or intersecting surfaces.
Shortest-path interpolation has an ambiguity at a relative 180-degree rotation;
the conversion tests cover stable nearby rotations, not continuity through every
possible extreme joint motion. Each individual model and weight band still needs
pose-specific geometry checks and visual review.

## Verification

`tests/character_skinning_tests.cpp` covers:

- Exact legacy vertex bytes over all 11 generic roles and both LODs
- Exact single-bone legacy behavior with DQ enabled
- Analytical two-/three-bone rigid blends, translations, and duplicate/zero slots
- Common-pivot midpoint radius preservation alongside the expected LBS shrinkage
- Opposite quaternion hemispheres and all three near-/exact-180-degree branches
- Bind identity/reset and unit normals
- Bit-identical split-corner positions with opposite hard-edge normal refreshes
  in both LBS/DQ, rigid/two-/three-weight fixtures, and both corner orders;
  sub-epsilon/repeated poses, accumulated motion, reset, and dirty revisions
- Invalid indices, NaN/infinite/negative/overfull weights, and non-rigid matrices,
  rejected before a partial mesh write
- 180 allocation-free frames with stable vertex/index storage, identity, GPU
  handles, UVs, color, opacity, and once-per-change dirty revisions

The pre-existing character model suite remains a separate compatibility check.
These algebra/storage tests do not certify a particular character's appearance
or the absence of mesh intersections.
