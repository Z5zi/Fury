# Jon Keel anatomical head: provenance and reconstruction

Jon's head and neck use a separate adult male-presenting shape recipe over the
MakeHuman Community's CC0 authored human mesh. This is an authored virtual human,
not a scan or measured anatomical reconstruction of a real person. The squarer
jaw, slightly broader brow/nose, and stronger neck are shape art direction for
the existing market/apron silhouette. They do not add biographical canon.
Jon's existing palette, chestnut rear-bun hair, and 1.70 m profile height are
independent of the source target names.

## Source and license boundary

All upstream inputs are pinned to [MPFB2 commit
d0a32e57a7f915cb2f2b95410e2117648c7bbb7e](https://github.com/makehumancommunity/mpfb2/commit/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e).
Its [license boundary](https://github.com/makehumancommunity/mpfb2/blob/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/LICENSE.md)
explicitly separates CC0 graphical assets, including base meshes, targets, and
JSON mesh information, from GPLv3 addon program logic. The complete
[CC0 asset license](https://github.com/makehumancommunity/mpfb2/blob/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/LICENSE.ASSETS.md)
is already preserved unchanged at `third_party/mpfb_assets/LICENSE.ASSETS.md`.

The separate [Jon manifest](../../../third_party/jon_anatomy/MANIFEST.json)
pins ten new numerical `.target.gz` files by immutable raw URL, SHA-256, byte
count, and official Git blob SHA-1. Each blob SHA-1 was checked against the
pinned official Git tree/content API while fetching. The targets occupy
426,471 bytes. Four shared inputs are referenced and independently hash-checked
in place, without duplicating the 1.75 MB base OBJ:

| Shared input | SHA-256 |
|---|---|
| `base.obj` | `8e761e6624b8f54536409135d1636da63b32486a90d4897f84e121d144f6fb4c` |
| `LICENSE.md` | `5cefb60680cb9efd4550a2e65d719021cfbec9dad3658481d11d48ae863ce04d` |
| `LICENSE.ASSETS.md` | `f6089cba01cb570a24712b41ab8a586ccd3cc5ef53dc266ca50b95c288956d2c` |
| `metadata/basemesh_vertex_groups.json` | `8cb1417bc55ae5ec8aa99f90734e81ed5fbb556511ff6a2c00e0534b70911444` |

Fury's original standard-library-only importer reuses Fury's own OBJ/math
helpers read-only. No upstream addon code or configuration was copied,
adapted, installed, imported, or executed. Neither MPFB nor Blender is required.

## Reviewable distinct shape recipe

The [exact recipe](../../../third_party/jon_anatomy/jon_recipe.json) adds each
target's weighted displacement to the original base vertex, including helpers:

| Target under `src/mpfb/data/targets/` | Weight |
|---|---:|
| `macrodetails/african-male-young.target.gz` | 0.35 |
| `macrodetails/asian-male-young.target.gz` | 0.25 |
| `macrodetails/caucasian-male-young.target.gz` | 0.40 |
| `head/head-square.target.gz` | 0.14 |
| `chin/chin-width-incr.target.gz` | 0.20 |
| `eyebrows/eyebrows-trans-forward.target.gz` | 0.10 |
| `nose/nose-scale-horiz-incr.target.gz` | 0.45 |
| `neck/neck-scale-horiz-incr.target.gz` | 0.18 |
| `neck/neck-scale-depth-incr.target.gz` | 0.12 |
| `head/head-scale-horiz-incr.target.gz` | 0.32 |

The macro weights sum to one. These are upstream parameter labels, not claims
about ethnicity or a numeric age. Existing skin and hair colors are not derived
from them. Jon does not reuse Mira's female/oval recipe or unchanged positions.
All 4,433 shared source vertices differ in the emitted geometry; the normalized
source-corresponding RMS displacement is 0.00567725 and maximum is 0.01634830.

After uniform height normalization, widths measured on the **same source
vertices** within each target's affected support are 5.9% larger for the chin,
2.1% for the brow, 2.1% for the nose, and 3.0% for the neck target support than
Mira's. These describe the sampled target regions, not clinical anthropometric
measurements; the neck support includes the neck/shoulder transition.

## Coordinates and retained topology

1. Verify every source SHA-256, new target byte count, and new target Git blob ID
2. Parse positions, UVs, groups, and original face corners from the shared OBJ
3. Apply the ten independent weighted morph targets to body and helper vertices
4. Find the full morphed `body` height, excluding helpers, from Y −8.18615 to
   9.21381; apply one uniform scale `0.05706909671056715` to all axes
5. Place feet at Y −0.5 and bald crown at Y 0.493, reserving 0.007 for hair
6. Retain original axes: +X anatomical left, +Y up, +Z face/front
7. Extract each rig joint from the mean of its unique morphed group vertices,
   and verify all used joint/eye-helper/body index sets against pinned metadata
8. Keep each `body` face whose every vertex is at or above Y 0.32; discard helpers
9. Split each source quad on its existing first-to-third-vertex diagonal, keeping
   original winding and all per-corner UV indices

The generated geometry retains all 4,438 selected source quads as 8,876
triangles, with 4,468 shared vertices and 4,664 source UV coordinates. No selected
triangle is removed, and no decimation, smoothing, or retopology is applied.
Each emitted binary32 triangle has squared cross-product length greater than
`1e-24`. There is one connected component, one continuous attachment seam with
58 boundary edges, zero non-manifold edges, and zero inconsistently wound shared
edges. Normals remain the responsibility of Fury's mesh integration.

## Runtime API and measured fitting data

[`jon_anatomy.hpp`](../../../engine/include/fury/generated/jon_anatomy.hpp)
exports namespace `fury::jon_anatomy`, with the same API as Mira: positions,
indices, per-corner UVs, original source vertex/UV/face IDs, source normalization,
eye centers/helper half-extents, lid/jaw/mouth/head/neck/crown rig markers, and
geometry bounds. All IDs are zero-based; face IDs count every OBJ `f` line.

Whole retained geometry bounds are (−0.11454909, 0.32000302, −0.03743162) through
(0.11454909, 0.493, 0.09665222). The low part contains upper shoulders. The
explicit fitting band Y ≥0.375 spans X ±0.05203731 and Z −0.02834907 to
0.09665222. These plane bands are fitting aids, not anatomical segmentation.

The final eye centers are (±0.01780720, 0.43190589, 0.07270460), with helper
half-extents (0.00895785, 0.00849531, 0.00968634). The left helper actually spans
(0.00886340, 0.42351666, 0.06240734) through
(0.02677910, 0.44050727, 0.08178002); the right is its X reflection. Joint means
and helper bounding-box centers are close but not assumed identical.

The head joint is (0, 0.40860779, 0.02720056); the neck joint is
(0, 0.35313520, 0.00711224). The mouth joint (0, 0.41307131, 0.03428141) is
internal, and must not be used as a surface decal/lip position. The source
vertices in the existing Mira-space lip pigment region map onto Jon's skin at
Y 0.38866857–0.39698067, Z 0.08116937–0.09261173, with X ±0.01285881.
Use approximately Y 0.393 as a visible-mouth fitting reference and verify
coverage on the rendered skin. Lid joints are also rig landmarks, not a promise
of the exact visible skin/eyeball boundary.

These are exact triangle/plane section bounding half-extents, not elliptical
fits. `center_z` is the midpoint of the actual section's front/back bounds:

| Y plane | X half-width | Z half-depth | center_z |
|---:|---:|---:|---:|
| 0.33 | 0.09315537 | 0.03793189 | 0.00710185 |
| 0.34 | 0.06795103 | 0.03485690 | 0.01342904 |
| 0.35 | 0.04789337 | 0.03424483 | 0.01701256 |
| 0.36 | 0.03770229 | 0.03428067 | 0.02023771 |

[`GENERATED_STATS.json`](../../../third_party/jon_anatomy/GENERATED_STATS.json)
contains further cross-sections through Y 0.485 and full-precision measurements.

## UV atlas compatibility

An independent decoder of both emitted C++ headers checked every UV coordinate
against the pinned OBJ rounded to binary32, and every emitted vertex/UV corner
against its original source face. All 4,406 source faces shared between Mira and
Jon have identical corner IDs, order, diagonal, and source UVs. Every selected
quad has both triangles. Facial lip/nose/eye regions retain their original
charts; differences in selected faces are at the attachment cutoff.

The current Face atlas's Mira-space facial pigment therefore follows the same
anatomical source regions on Jon. Its pigment stretches with Jon's different
geometry. This validates correspondence, not the finished appearance or
pigment strength; those still require Jon's character render inspection. No
shared surface, atlas-generation, or character-profile code is changed here.

## Reproduction and checks

```sh
python3 scripts/build_jon_anatomy.py
python3 scripts/build_jon_anatomy.py --check
python3 scripts/build_mira_anatomy.py --check
```

The Jon check verifies all pinned inputs and compares the entire regenerated
header and statistics report byte for byte without writing. Generation checks
finite data, valid indices, unique recipe/target entries, OBJ/metadata agreement,
source-preserving triangulation, binary32 triangle area, connectedness, winding,
manifold edges, and one unbranched attachment loop. It rejects polygons larger
than quads instead of assuming an arbitrary fan is valid.

Additional checks performed on the final output:

- A standalone C++17 translation unit included both anatomical headers and
  Fury's actual `Vec2`/`Vec3` definitions; it compiled with warnings enabled and
  passed array-size, finite-coordinate, index-range, marker, and crown assertions
- Independent source/corner checks described above passed for both headers
- Negative checks rejected a NaN weight, duplicate recipe target, unpinned
  target, and a byte-corrupted source; temporary corruption was never applied
  to the vendored source assets
- Mira's own byte-for-byte `--check` still passed; its importer, data, and
  generated header were not modified

Final generated-header SHA-256:
`5ecac83a75fe83a6ca78fc088b45723213167b6b27496e1a36a917e69c7cedac`.
This is intact source topology, shared by any runtime LODs that choose it.
Generation alone does not establish CPU cost, finished hair/eye/clothing fit,
or visual quality; those are character-integration checks.
