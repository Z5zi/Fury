# Owen Pike: anatomical source, recipe and fitting

Authored and verified on **6 October 2026, UTC**, during Owen's dedicated
character pass. This asset supplies connected head, ears, jaw and neck skin
for `NpcBankCust` / Commuter at the existing **1.74 m** height. It is a separate
shape recipe, not an unchanged or globally rescaled Jon, Mira or Tessa head.
This document records the anatomical asset; clothing, hair, materials, runtime
animation and gameplay integration have separate review requirements.

## Source and license boundary

All imported geometry, numeric target fields and mesh metadata come from the
official [MakeHuman/MPFB2 repository](https://github.com/makehumancommunity/mpfb2)
at immutable commit **`d0a32e57a7f915cb2f2b95410e2117648c7bbb7e`**. The upstream
[license boundary](https://github.com/makehumancommunity/mpfb2/blob/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/LICENSE.md)
defines mesh, targets and mesh-information JSON as graphical assets released
under [CC0 1.0 Universal](https://github.com/makehumancommunity/mpfb2/blob/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/LICENSE.ASSETS.md).
Addon program logic has a separate GPL license. No MPFB addon code was copied,
installed, imported or executed. This is authored parametric anatomy, not a
scan or reconstruction of a particular person's likeness.

The [manifest](../../../third_party/owen_anatomy/MANIFEST.json) pins **16** inputs
with immutable upstream paths/URLs, SHA-256, exact byte counts and Git blob
SHA-1 identifiers. All blob IDs and byte counts were checked against the
untruncated official tree for that commit. Seven new numeric `.target.gz`
files were retrieved through the public GitHub file connector using base64;
their decoded bytes matched the tree. Nine existing inputs are referenced in
place: base OBJ, metadata, both license texts, three masculine macro targets
and two neck targets. No shared input was edited or duplicated.

[Fury's importer](../../../scripts/build_owen_anatomy.py) uses its own
standard-library parsing/math helpers from the Mira importer and the validation
patterns developed for Tessa. It adds explicit source-triangle identity,
ordered external eye rims, an ordered attachment cycle and contour status for
fitting sections. Its implementation is Fury code, not upstream addon code.

The copyrighted art and garment photographs listed in
[REFERENCE_CRITERIA.md](REFERENCE_CRITERIA.md) are separate private study
references. None of their geometry, maps, patterns, logos or likenesses is
included in this asset.

## Original shape recipe

The [recipe](../../../third_party/owen_anatomy/owen_recipe.json) specifies a
longer/narrower masculine-presenting head, softly tapered jaw, modest chin,
restrained nose width, gently laid-back ears and substantial neck. These are
art-direction choices only. Upstream population, gender and age labels name
graphical controls; they do not establish Owen's ethnicity, literal age,
gender identity or biography. His existing skin `(0.790, 0.585, 0.415)` and
auburn hair `(0.270, 0.110, 0.052)` remain independent identity anchors.

| Target | Weight |
|---|---:|
| `third_party/jon_anatomy/targets/macrodetails/african-male-young.target.gz` | 0.25 |
| `third_party/jon_anatomy/targets/macrodetails/asian-male-young.target.gz` | 0.30 |
| `third_party/jon_anatomy/targets/macrodetails/caucasian-male-young.target.gz` | 0.45 |
| `targets/head/head-scale-horiz-decr.target.gz` | 0.08 |
| `targets/head/head-scale-vert-incr.target.gz` | 0.18 |
| `targets/head/head-rectangular.target.gz` | 0.07 |
| `targets/chin/chin-width-decr.target.gz` | 0.10 |
| `targets/nose/nose-scale-horiz-decr.target.gz` | 0.08 |
| `targets/ears/l-ear-rot-backward.target.gz` | 0.12 |
| `targets/ears/r-ear-rot-backward.target.gz` | 0.12 |
| `third_party/jon_anatomy/targets/neck/neck-scale-horiz-incr.target.gz` | 0.30 |
| `third_party/jon_anatomy/targets/neck/neck-scale-depth-incr.target.gz` | 0.16 |

Paths beginning `third_party/` are relative to the repository; other target
paths are relative to `third_party/owen_anatomy/`. The three macro weights sum
to one. Every vendored field is used exactly once. Paired ears use equal
weights. All fields are applied before one uniform normalization; no per-axis
scaling of an existing character is performed.

Corresponding source vertices establish the difference quantitatively. All
**4,368** vertices shared with each prior character differ. RMS normalized
displacements are **0.004756859** from Mira, **0.004091836** from Jon and
**0.013422749** from Tessa. On the same source head-control support, Owen is
**6.58% narrower** than Jon and **11.83% narrower** than Tessa. His chin-control
support is **8.55% narrower** than Jon's and nose-control support **8.67%
narrower**. Head-control support height is **2.04% greater** than Jon's and
**6.11% greater** than Tessa's. These are source-support comparisons, not
clinical measurements. The full values and compared vertex counts are in
[VALIDATION.json](../../../third_party/owen_anatomy/VALIDATION.json).

A matched bare-skin front/profile/three-quarter diagnostic was rendered and
inspected beside Jon and Tessa. It shows the intended long narrow face, tapered
jaw, smaller nose, preserved posterior skull depth and neck spreading at its
base. The exposed sawtooth lower edge is the intentionally retained original
face boundary; runtime clothing must cover it. This diagnostic does not claim
finished materials, hair, eye fitting or animation.

## Coordinates, cutoff and complete source topology

The morphed full body spans source Y **−8.18495 to 9.14123**, a height of
**17.32618**. Uniform scale **0.057312113806967255** and translation place the
body feet at **−0.5** and bald crown at **+0.493**, leaving **0.007** for hair
within the existing total height. Original axes remain +X anatomical left,
+Y up, +Z front. The runtime multiplies these normalized positions by height.

Every vertex of a retained `body` face must satisfy **Y ≥ 0.33**. Each source
quad is split on its original first-to-third corner diagonal, preserving
winding and per-corner source UV IDs. Helpers are used for landmarks only;
their faces do not become rendered skin. The resulting asset contains:

- **4,368 positions**, **8,680 triangles**, **4,562 original UVs**
- **4,340 complete source quads**, with no selected triangle dropped
- One connected component and one unbranched **54-edge** attachment cycle
- Zero degenerate triangles in emitted binary32 coordinates, zero
  non-manifold edges, and zero inconsistently wound shared edges
- Exact canonical UVs and source vertex, UV, face and triangle identities

The five canonical Face charts remain present and separate: external head
**6,232 triangles**, mouth interior **796**, neck/shoulders **480**, and each
eye socket **586**. Every triangle's corners belong to exactly one supported
source chart. No decimation, smoothing or retopology changes these triangles.

Cutoffs from 0.32 through 0.335 were evaluated in steps of 0.0025. Each kept
one continuous cycle. Selected comparisons explain the chosen overlap:

| Cutoff | Triangles | Largest retained X half-width | Attachment Y range |
|---:|---:|---:|---:|
| 0.320 | 8,832 | 0.10356586 | 0.32036560–0.32890510 |
| 0.325 | 8,768 | 0.09024079 | 0.32526292–0.33225786 |
| **0.330** | **8,680** | **0.07174044** | **0.33038547–0.33849342** |
| 0.335 | 8,620 | 0.07129914 | 0.33523236–0.34254539 |

0.33 removes the bulky internal shoulder flange of the 0.32 cut while keeping
more collar overlap than 0.335, which offers little further width reduction.
It does not substitute for the separately authored broad shoulder silhouette.
The complete attachment cycle must be covered through its highest point,
**Y 0.33849342**. Overall retained bounds are
**(−0.07174044, 0.33038547, −0.02878501)** to
**(0.07174044, 0.493, 0.09661676)**.

## Source-derived eye, mouth, neck and head fitting

All values below are normalized Fury coordinates, before the 1.74 m height
multiplier. The anatomical-left eye joint is
**(0.01655733, 0.42886251, 0.07266603)**; the right eye is its X reflection.
Source helper half-extents are
**(0.00831255, 0.00788099, 0.00898826)**. The helper's bounds center differs
slightly from the rig joint; neither is the actual external lid surface.

Each actual external head/eye-socket boundary contains **28 ordered source
vertices**. The left opening spans X **0.01154610–0.02395876**,
Y **0.42566171–0.43020254**, Z **0.07482670–0.07994811**, with mean
**(0.01790922, 0.42765240, 0.07788471)**. The right is its X reflection.
Use these rims and rendered depth to fit visible eyes. The upper/lower-lid
rig markers sit substantially farther forward and must not be substituted
for skin positions.

The head joint is **(0, 0.40407066, 0.02719746)**; neck joint
**(0, 0.35001100, 0.00680438)**. The jaw helper is
**(0, 0.37068993, 0.08106798)**. The crown rig helper lies at Y **0.49656410**,
above the true skin crown of 0.493; helpers do not define the visible envelope.

The internal mouth joint **(0, 0.40926414, 0.03397176)** is not a lip placement
point. The **38-vertex external mouth-opening seam** spans X **±0.01309524**,
Y **0.38525323–0.38727290**, Z **0.08014239–0.08737518**, with mean
**(0, 0.38656703, 0.08374356)**. Including one surrounding exterior skin ring
gives **76 vertices** and maximum Z **0.08857874**. The source IDs, ordered
seam positions and surface bounds are preserved in the generated statistics.

Closed neck sections below come from exact triangle/plane intersections,
using only the original neck/shoulder UV chart. They are measured surface
bounds, not assumed elliptical profiles; clothing needs positive clearance.

| Y plane | X half-width | Minimum Z | Maximum Z | Contour |
|---:|---:|---:|---:|---|
| 0.340 | 0.05962028 | −0.02010819 | 0.04999995 | One closed loop |
| 0.345 | 0.05100919 | −0.01825605 | 0.05120613 | One closed loop |
| 0.350 | 0.04442373 | −0.01659792 | 0.05194786 | One closed loop |
| 0.355 | 0.03914389 | −0.01495512 | 0.05482028 | One closed loop |

Y 0.335 intersects the lower attachment and is incomplete. At Y 0.36 and
above, the sloping head/neck chart join opens the neck-chart intersections;
their reported bounds are partial surfaces, not a closed collar envelope.
Whole-skin sections include projecting jaw or other surfaces at those heights.
Both section types carry closed/open contour counts to prevent conflating them.

The [generated header](../../../engine/include/fury/generated/owen_anatomy.hpp)
uses `fury::owen_anatomy`, preserves the 26 existing anatomy API names and adds
`triangle_source_indices`, `attachment_boundary_indices`, and paired ordered
`left_eye_opening` / `right_eye_opening` arrays with their source IDs. Source
triangle IDs count all fan triangles in the original OBJ, including discarded
body regions and helper groups. Attachment indices address the generated
`positions` array. Ring order is deterministic adjacency order, not a claimed
anatomical clockwise convention.

## Verification and limits

`python3 scripts/build_owen_anatomy.py --check` verifies pinned input hashes,
byte sizes, Git blobs and exact regeneration of the header and
[statistics](../../../third_party/owen_anatomy/GENERATED_STATS.json).
Generation and check use only Python's standard library. The header compiled
and ran with **C++17**, `-Wall -Wextra -pedantic`, without diagnostics; static
and runtime checks covered array sizes, index ranges and finite positions.
Python bytecode compilation passed.

The independent audit matched every emitted triangle corner to its original
source face/triangle, vertex and UV identity, and every emitted UV to the
original binary32 value. **39 malformed-input cases** were rejected, covering
source commit/license/repository/path/hash/URL/size/blob/duplicates, unknown or
duplicate recipe targets, invalid weights and macro normalization, unused
targets, normalization/cutoff/degeneracy parameters, malformed or empty target
rows, target index/number errors, OBJ UV/vertex/polygon errors, unknown atlas
charts and branched/disconnected rings. Cases and results are recorded in
[VALIDATION.json](../../../third_party/owen_anatomy/VALIDATION.json).

This verifies the authored anatomical data and import boundary. The asset
alone does not establish eye occlusion, collar clearance through animation,
hair coverage, costume quality, live lobby performance or final visual parity
with the proprietary references.
