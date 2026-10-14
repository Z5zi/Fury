# Tessa rolled-flap correction

**2026-10-06, after the 16:45 UTC first-pass checkpoint.** Owen's later bag
review exposed the same rolled-sheet defect in Tessa's satchel. This note and
its new evidence directory document a separate correction. The historical
`PASS_01.md`, `GEOMETRY_REVIEW.md`, images, reports, and immutable recovery
checkpoint for Tessa source `69441a74` were not replaced or relabeled.

## Change and scope

The old underside moved every flap sample by `-0.0011 × height` on the Z axis.
That is the intended thickness only on the vertical front. Around the rolled
top it becomes mostly a tangent displacement, and its normal projection can
change sign. The outer sheet also had inward triangle winding.

`make_bag` now reverses the outer sheet's across parameter and preserves the
underside's original direction. The underside moves **0.0011 × height** along
the inward surface normal, estimated from finite differences. Each derivative
tangent is normalized before their cross product; otherwise Fury's `1e-8`
normalization tolerance discards a valid but numerically small cross product.

This changes only the two flap sheets. An independent old/new model comparison
at 1.35, 1.65 and 2.10 m verifies:

| LOD | Changed flap vertices | Other vertices, bit-exact | Triangles, unchanged |
| --- | ---: | ---: | ---: |
| Near | 630 | 24,496 | 39,988 |
| Far | 126 | 9,807 | 15,976 |

The complete index buffer, rig, influences, every color/UV/opacity, seeded
appearance and texture handles remain bit-exact. All source anatomy, bag-body
dimensions, strap/hardware, clothing, head and limbs are unchanged. No shared
helper, material, renderer, controller or animation code changed in this
backport. The existing generic-fallback and unused-HairShortStrands test
allowances were retained.

## Regression and negative controls

The new regression recovers actual indexed Leather components, using their
topology, bounds and UV span to identify the two open, nonperiodic flap sheets.
It does not duplicate the procedural flap formula or assume vertex offsets or
tessellation. It checks unique mirrored-U corner correspondence, intended
offset length, outward geometric winding, opposing shading normals, positive
normal thickness at every triangle corner, and no proper intersections
between the real outer/underside triangles.

All sheet influences are identical and dual-quaternion skinning is enabled,
so each entire sheet receives the same rigid transform. This preserves their
measured separation in runtime poses.

At all three tested heights:

| Measured quantity, divided by height | Near | Far |
| --- | ---: | ---: |
| Minimum triangle-normal gap | about 0.000586 | about 0.000283 |
| Minimum shading-normal gap | about 0.000430 | about 0.000227 |
| Corresponding-corner distance | 0.0011 | 0.0011 |

Both LODs exceed the unchanged physical gate of **0.0001 × height** normal
separation. Coarse flat triangles approximate the smooth rolled surface, so
their projected gap is smaller than the exact 0.0011h corner offset.

There are two independent negative controls:

1. The regression recovers the historical Z direction from the real lower
   sheet and projects a Z-only 0.0011h displacement against actual triangle
   normals. It fails the same signed thickness gate in both LODs. Near's
   smallest absolute gap is only about **0.0000355h**; the signed minimum is
   **−0.000562h**. Far's signed minimum is **−0.000256h**.
2. The exact old production source, SHA-256 `69441a74…`, was compiled and linked
   against the new regression object and the same engine archive. It fails
   with geometric and shading gaps of **−0.0011h**, reporting that flap shading
   normals do not face the proper sides of the separated layers. No production
   file was reverted to run this test.

The intersection predicate checks proper noncoplanar triangle-interior
crossings; it excludes shared edges and boundary contact. Together with the
positive sheet-separation tests, this is a focused flap regression, not a
whole-character collision certificate.

## Completed validation

- Isolated Linux Release build, maximum two compiler jobs: **passed**, 14.08 s
- Isolated `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer` build:
  **passed**, 108.25 s, with no address or undefined-behavior findings
- Each run includes the complete **1,188 actual skinned poses** and **180
  dedicated shoulder/head-turn checks**, both LODs and all three heights
- Existing source-anatomy, topology, materials, deterministic generation,
  generic fallback, pose-allocation, knee, shoulder and wardrobe checks pass
- Old-source negative control: **failed as intended**, exit 1
- Independent source-scope comparison: **passed**, all six profiles
- `clang-format --dry-run --Werror` on the two changed C++ files: **passed**

Leak checking remains unavailable because of the previously confirmed runner
ptrace limitation. The sanitizer run explicitly used
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`; no leak-check pass is claimed.

## New actual-engine pixels

[Bag/torso before, after and amplified difference](../../validation/characters/tessa/flap-correction-2026-10-06/bag-torso-before-after.png)
uses the production software renderer, at two camera angles, 768 × 768,
bind pose, height 1.65 m. The paired executables share the exact same renderer,
materials, characterlab object, camera and lighting. Only the Tessa model
object is replaced with the old or corrected source.

The effect is localized and visually small at this scale. Front and top views
change **5,588** and **6,610** of 589,824 pixels respectively, all within the
bag/flap bounds. Maximum channel differences are 48 and 74. The comparison
board enlarges crops with nearest-neighbor sampling and labels the absolute
difference amplified eight times. It contains no Blender render, retouching,
or beauty processing. These are diagnostic engine captures, not new
Vaultline world or GPU/path-tracing evidence.

Raw render reports retain their shared build-info fingerprint. Because the
old actor object was separately linked into that same engine, that shared
fingerprint does **not** identify the overridden Tessa source. The capture
manifest separately records the precise actor-source and executable hashes.

## Source identity and reproducibility

| Source | SHA-256 |
| --- | --- |
| Before `engine/src/character_tessa.cpp` | `69441a74a30b6e5addad511a11f522a45848d63511fc1f00c753fdf7e9db61f0` |
| Corrected `engine/src/character_tessa.cpp` | `616f751172a77c5ca6615eb09cab6be3bd347f2527ccde22c3165b47b2f296b8` |
| Updated `tests/character_tessa_tests.cpp` | `30c8df01844cc26b4152388bf5287c664091a378fff2dbea61a75179b6fa49c0` |
| Unchanged generated anatomy | `4774159205e04d918bc875db5c937982322d2cb07a8cb15c3f82dc524c1efdc4` |

All twelve recorded relevant source inputs were checked unchanged after both
successful runs. The [new evidence manifest](../../validation/characters/tessa/flap-correction-2026-10-06/MANIFEST.json)
records their hashes and the hashes of each new artifact. That directory also
contains complete release/sanitizer logs, results, the actual old-source
failure, scope-comparison source/output, raw captures/reports, pixel metrics,
capture metadata and the minimal production patch.

Normal regression reproduction from a configured checkout:

```sh
cmake --build build --target fury_character_tessa_tests -j2
ctest --test-dir build -R '^character_tessa$' --output-on-failure
```

For the old-source control, reverse-apply `production-change.patch` to a
temporary copy of the corrected source, verify its `69441a74…` hash, compile
with the checkout's `engine/include` and `engine/src` include paths, and link
that object before the current engine archive and the current Tessa test
object. The full test must exit 1 at the flap-facing check. The scope comparison
links that same baseline object under the renamed
`fury::make_before_tessa_character_profile` symbol, along with the current
engine; its source is included beside the result.
