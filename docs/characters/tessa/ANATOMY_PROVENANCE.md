# Tessa Quill anatomical head: provenance and reconstruction

Tessa's separate recipe authors a **mature adult feminine-presenting face**,
broader compact head, cheek/jaw transitions, settled eyelids, and a short
substantial neck. These are deliberate visual choices for `NpcCivC`, not a
found biography, declared numerical age, ethnicity, or an inference from gray
hair. This is an authored virtual human based on a CC0 base mesh, not a scan or
a measured reconstruction of a real person. The established 1.65 m Commuter
identity, gray short hair, ochre/brown wardrobe and satchel stay independent of
the source parameter labels and this geometry recipe.

## Pinned source and license boundary

All upstream graphical inputs are from [MPFB2 commit
d0a32e57a7f915cb2f2b95410e2117648c7bbb7e](https://github.com/makehumancommunity/mpfb2/commit/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e).
The [upstream license boundary](https://github.com/makehumancommunity/mpfb2/blob/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/LICENSE.md)
separates graphical assets, including base meshes, targets, and JSON mesh
information, from GPLv3 addon program logic. The graphical data is CC0 1.0
Universal; the unchanged full [CC0 asset license](../../../third_party/mpfb_assets/LICENSE.ASSETS.md)
is already vendored with Mira's source assets.

The [Tessa manifest](../../../third_party/tessa_anatomy/MANIFEST.json) pins
19 new numerical `.target.gz` files, **443,444 bytes** total, by immutable source
URL, SHA-256, byte count and official Git blob SHA-1. Each downloaded blob's size
and Git object hash were checked against the official pinned recursive tree.
The corresponding [official Git tree endpoint](https://api.github.com/repos/makehumancommunity/mpfb2/git/trees/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e?recursive=1)
identifies the complete source tree. No binary asset was accepted from a moving
branch or floating latest-release URL.

Eleven existing files are independently hash-checked in place: the base OBJ,
mesh-group metadata, two license documents, three female-young targets already
used by Mira, and four head/chin/neck targets already used by Jon. They are
referenced without duplication or modification. The shared base-mesh SHA-256
is `8e761e6624b8f54536409135d1636da63b32486a90d4897f84e121d144f6fb4c`;
the metadata SHA-256 is
`8cb1417bc55ae5ec8aa99f90734e81ed5fbb556511ff6a2c00e0534b70911444`.
Every remaining input's full checksum and URL is in the manifest.

Fury's [Tessa importer](../../../scripts/build_tessa_anatomy.py) derives from
Fury's own Jon importer and imports Fury's original OBJ/math helpers read-only.
It uses Python's standard library. No upstream addon Python, shell, program
logic or configuration was copied, adapted, imported, installed or executed.
Neither Blender nor MPFB is needed to reproduce the generated header.

## Source form study and authored recipe

The pass inspected the actual pinned numerical source meshes, rendered the
emitted Mira/Jon/Tessa geometry from front, three-quarter, side and rear under
the same plain clay lighting, and inspected the official graphical control
references for [head maturity](https://raw.githubusercontent.com/makehumancommunity/mpfb2/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/src/mpfb/data/targets/_images/head-age-decr-incr.png),
[round head shape](https://raw.githubusercontent.com/makehumancommunity/mpfb2/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/src/mpfb/data/targets/_images/head-round.png),
[head height](https://raw.githubusercontent.com/makehumancommunity/mpfb2/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/src/mpfb/data/targets/_images/head-scale-vert-decr-incr.png),
[cheek-bone prominence](https://raw.githubusercontent.com/makehumancommunity/mpfb2/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/src/mpfb/data/targets/_images/l-cheek-bones-decr-incr.png),
[cheek volume](https://raw.githubusercontent.com/makehumancommunity/mpfb2/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/src/mpfb/data/targets/_images/l-cheek-volume-decr-incr.png),
[lower-lid volume](https://raw.githubusercontent.com/makehumancommunity/mpfb2/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/src/mpfb/data/targets/_images/l-eye-bag-decr-incr.png),
[upper-lid fold](https://raw.githubusercontent.com/makehumancommunity/mpfb2/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/src/mpfb/data/targets/_images/l-eye-eyefold-down-up.png),
and [neck height](https://raw.githubusercontent.com/makehumancommunity/mpfb2/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/src/mpfb/data/targets/_images/neck-scale-vert-decr-incr.png).
Those eight source illustrations were Git-blob verified and viewed for this
study, without adding duplicate reference PNGs to the vendored data directory.
They identify where each control acts; they are not empirical anatomical
measurements or photographs of a proposed character likeness.

The clay comparison shows Tessa's wider cheeks/lower jaw, rounder skull, more
settled nasolabial/mouth contour and lower-lid structure. The profile retains
an authored nose bridge/tip and fuller ear lobe, with a shorter head-to-shoulder
transition. The face is not Mira's or Jon's mesh with a different skin tint.
The reference review does not certify the game renderer, eye objects, hair,
clothing, final lighting or animation; those require the individual character
integration review.

The [exact recipe](../../../third_party/tessa_anatomy/tessa_recipe.json) applies
all listed numeric displacement fields before one uniform body-height
normalization. Paths beginning `third_party/` reference existing verified data;
other paths are relative to `third_party/tessa_anatomy/`.

| Target | Weight |
|---|---:|
| `third_party/mpfb_assets/targets/macrodetails/african-female-young.target.gz` | 0.14 |
| `third_party/mpfb_assets/targets/macrodetails/asian-female-young.target.gz` | 0.105 |
| `third_party/mpfb_assets/targets/macrodetails/caucasian-female-young.target.gz` | 0.105 |
| `targets/macrodetails/african-female-old.target.gz` | 0.26 |
| `targets/macrodetails/asian-female-old.target.gz` | 0.195 |
| `targets/macrodetails/caucasian-female-old.target.gz` | 0.195 |
| `targets/head/head-round.target.gz` | 0.14 |
| `targets/head/head-scale-vert-decr.target.gz` | 0.18 |
| `targets/head/head-age-incr.target.gz` | 0.12 |
| `targets/cheek/l-cheek-bones-incr.target.gz` | 0.18 |
| `targets/cheek/r-cheek-bones-incr.target.gz` | 0.18 |
| `targets/cheek/l-cheek-volume-decr.target.gz` | 0.13 |
| `targets/cheek/r-cheek-volume-decr.target.gz` | 0.13 |
| `targets/eyes/l-eye-bag-incr.target.gz` | 0.1 |
| `targets/eyes/r-eye-bag-incr.target.gz` | 0.1 |
| `targets/eyes/l-eye-eyefold-down.target.gz` | 0.1 |
| `targets/eyes/r-eye-eyefold-down.target.gz` | 0.1 |
| `targets/neck/neck-scale-vert-decr.target.gz` | 0.16 |
| `targets/nose/nose-scale-depth-incr.target.gz` | 0.12 |
| `targets/nose/nose-point-down.target.gz` | 0.08 |
| `targets/ears/l-ear-lobe-incr.target.gz` | 0.12 |
| `targets/ears/r-ear-lobe-incr.target.gz` | 0.12 |
| `third_party/jon_anatomy/targets/head/head-scale-horiz-incr.target.gz` | 0.38 |
| `third_party/jon_anatomy/targets/chin/chin-width-incr.target.gz` | 0.12 |
| `third_party/jon_anatomy/targets/neck/neck-scale-horiz-incr.target.gz` | 0.55 |
| `third_party/jon_anatomy/targets/neck/neck-scale-depth-incr.target.gz` | 0.22 |

The six macro weights sum to exactly one: 0.35 in the source `female-young`
family and 0.65 in `female-old`, with a separate 0.12 local head-maturity
control. These labels specify source blend parameters, not a literal age or
biography. Local paired cheek/lid/ear fields preserve bilateral shape, while
head-round/width/height, chin, nose and neck controls construct the deliberate
shape differences. The existing palette is not derived from these labels.

On identical source vertices, all **4,438** vertices shared with Mira differ;
the normalized source-corresponding RMS displacement is **0.01499099**, maximum
**0.02392847**. All **4,464** vertices shared with Jon differ; RMS is
**0.01263235**, maximum **0.02612937**. Widths on the same source target-support
vertices are 11.42% larger at the chin and 8.89% larger across the paired cheek
support than Mira's; compared with Jon, 5.22% and 7.95% larger. These are
reproducible shape-support comparisons, not clinical anthropometric measures.
The neck-width support includes the neck/shoulder transition and is 1.08% wider
than Mira's after normalization. The visual result uses different proportions,
not uniform scaling of an existing character.

## Coordinates and preserved topology

1. Verify the pinned commit, license identity, immutable URLs and every input
   hash; also check byte counts and Git blob IDs for numerical targets
2. Parse the shared OBJ positions, original UVs, groups and face corners;
   verify used group indices against the pinned mesh metadata
3. Apply the 26 weighted targets to body and helper positions
4. Use the complete morphed `body`, excluding helpers, with foot floor
   Y −8.194405 and crown Y 7.597615 (source height 15.792020)
5. Apply uniform scale **0.06287985957464592** and translation to place feet at
   −0.5 and bald crown at +0.493, with the same +0.007 hair reserve as Mira/Jon
6. Preserve the original +X anatomical-left, +Y-up, +Z-front axes
7. Retain each `body` face whose every vertex is at or above Y 0.32;
   split quads on the original first-to-third corner diagonal with original
   winding and per-corner UV IDs; discard source helpers from rendered skin

There are **4,486 shared positions**, **8,912 triangles**, **4,683 source UVs**,
and **4,456 complete selected source quads**. Every selected triangle remains;
none is decimated, smoothed, retopologized or discarded as degenerate. All
emitted binary32 triangles pass squared cross-product length greater than
`1e-24`. The result has one connected component, one unbranched 58-edge open
attachment loop, zero non-manifold edges and zero inconsistently wound shared
edges. Skin normals are calculated by Fury's mesh integration.

The generated [header](../../../engine/include/fury/generated/tessa_anatomy.hpp)
uses `fury::tessa_anatomy` and matches Jon's existing 26 exported API names,
including original source vertex/UV/face IDs. All source indices are zero-based;
face IDs count every original OBJ `f` line. Complete retained bounds are
(−0.10315064, 0.32005712, −0.01828704) to
(0.10315064, 0.493, 0.11055443). The low band includes upper shoulders.

## Eye, mouth, head and collar fitting

The actual source eye-socket seams are preferable to treating a rig marker as
an eyelid surface. Each seam has **28 source vertices**. The anatomical-left
opening spans X **0.01211883–0.02704777**, Y **0.42735216–0.43259445**,
Z **0.08534840–0.08958431**, with mean
**(0.01987100, 0.42973000, 0.08774372)**. The right seam is the X reflection.
The generated eye joint centers are (±0.01924352, 0.43051499, 0.08234522), and
source eye-helper half-extents are (0.00971588, 0.00943261, 0.01007335).
Joint and helper centers are distinct, and the source lid rig markers are
forward of the actual eye opening. Do not fit visible eye geometry to the
upper/lower-lid rig Z coordinate. Verify eye depth on the rendered mesh.

The head joint is **(0, 0.40318194, 0.04025105)**; neck joint
**(0, 0.34420354, 0.01850342)**. The crown rig marker lies at
Y 0.49534510 even though the actual bald skin crown is 0.493. As for the other
rig markers, this helper need not lie exactly on the skin.

The internal mouth joint (0, 0.41194118, 0.04711156) is not a lip placement
center. The actual **38-vertex mouth-opening seam** spans X ±0.01531408,
Y **0.39175274–0.39454680**, Z **0.09299680–0.10000161**, with mean
(0, 0.39343636, 0.09620134). Including one outer skin edge ring gives 76
vertices and maximum Z 0.10064236; source vertex IDs for both patches are in
the statistics report. This is a geometric fitting region, not an invented
anatomical joint or an instruction to paint a flat lip strip.

The exact linear triangle/plane intersections below are skin section bounds,
not assumed ellipses. Their Z center is the midpoint of front/back bounds.
The neck and collar geometry should include practical clearance around these
values; lower samples include the shoulder transition.

| Y plane | X half-width | Z half-depth | center Z |
|---:|---:|---:|---:|
| 0.33 | 0.09229561 | 0.03247130 | 0.02082651 |
| 0.34 | 0.06733913 | 0.03081299 | 0.02382624 |
| 0.345 | 0.05438266 | 0.03046549 | 0.02576525 |
| 0.35 | 0.04644866 | 0.03046731 | 0.02754585 |
| 0.36 | 0.03561929 | 0.03197015 | 0.03296762 |
| 0.375 | 0.03340655 | 0.04568176 | 0.05090913 |
| 0.4 | 0.04985452 | 0.04959380 | 0.05383801 |
| 0.43 | 0.05390610 | 0.05566726 | 0.04355781 |
| 0.46 | 0.04546929 | 0.05334927 | 0.04171470 |
| 0.485 | 0.02928357 | 0.03261377 | 0.04108928 |

The [statistics](../../../third_party/tessa_anatomy/GENERATED_STATS.json) retain
full-precision bounds, helper bounds, rig markers, all section measurements,
source boundary IDs, recipe and output checksum.

## Canonical UV and shared Face atlas verification

Every emitted UV equals its pinned original OBJ coordinate rounded to binary32.
An independent header decoder verified each emitted vertex/UV corner against
its source face, including winding and diagonal. All 4,412 source faces shared
with Mira and all 4,434 shared with Jon retain identical corner IDs and order.
Every selected source quad keeps both triangles.

| Source chart | Triangles | Canonical U bounds | Canonical V bounds |
|---|---:|---|---|
| external_head | 6,232 | 0.641249–0.993515 | 0.148696–0.817634 |
| mouth_interior | 796 | 0.757634–0.923740 | 0.011863–0.136440 |
| neck_shoulders | 712 | 0.216349–0.544711 | 0.842310–0.973856 |
| right_eye_socket | 586 | 0.011809–0.076822 | 0.547391–0.632858 |
| left_eye_socket | 586 | 0.086143–0.151156 | 0.547391–0.632858 |

Every Tessa UV is inside exactly one of the existing five supported rectangles,
and every triangle's three corners remain in one chart. The different neck
selection has 712 triangles, but its bounds remain within the Mira/Jon union
used by the shared atlas. No shared atlas rectangle, transform, runtime surface
code or source UV coordinate needed changing. Each source feature therefore
keeps its existing canonical pigment correspondence, subject to Tessa's actual
shape stretching that pigment. This check establishes UV correspondence and
layout compatibility, not completed visual pigment validation.

## Reproduction and checks

```sh
python3 scripts/build_tessa_anatomy.py
python3 scripts/build_tessa_anatomy.py --check
python3 scripts/build_mira_anatomy.py --check
python3 scripts/build_jon_anatomy.py --check
```

`--check` regenerates the complete header and JSON statistics in memory and
compares bytes without writing. The importer rejects escaping/unpinned paths,
duplicate sources/targets, corrupted assets, nonfinite data, invalid indices,
unsupported OBJ polygons, unexpected normalization, invalid target weights,
a non-unit macro blend, unused newly vendored targets, metadata disagreement,
degenerate/disconnected/non-manifold/inconsistently wound geometry, changed
surface-seam counts, and unsupported or cross-chart UV triangles.

Verification on 6 October 2026:

- Tessa, Mira and Jon byte-for-byte generation checks passed
- A standalone C++17 translation unit included all three headers and the real
  Fury vector definitions; it compiled with `-Wall -Wextra -Wpedantic` and
  passed array lengths, finite values, index/UV ranges, normalized crown,
  attachment cutoff and marker assertions
- Independent source-corner/UV checks and exact exported-API-name comparison
  passed; comparison measurements above came from the actual emitted headers
- Fifteen negative checks rejected NaN/duplicate/unpinned/out-of-range recipe
  targets, a non-unit macro sum, a byte-corrupted temporary source, duplicate
  target vertex, nonfinite delta, out-of-range target index, malformed target
  row, nonfinite OBJ vertex, missing UV, unsupported pentagon, unsupported UV
  chart, and stale generated output; no vendored source was corrupted
- Plain clay geometry was inspected from front, three-quarter, side and rear;
  final game-renderer, texture, hair, eye, wardrobe and animation acceptance
  belongs to the integrated Tessa pass

Frozen generated-header SHA-256:
`4774159205e04d918bc875db5c937982322d2cb07a8cb15c3f82dc524c1efdc4`.
The same intact topology is available to each runtime LOD that chooses this
asset. This anatomical bake alone does not establish full-character CPU cost,
finished art quality or a release/publication approval.
