# Lia Merrow — first dedicated pass

Lia now has an individually authored anatomical head, fitted eyes, swept short
hair, sewn pale service shirt, pointed collar, tie, name badge, connected work
trousers and grounded shoes. Stable identity `NpcTeller`, name, BankStaff role,
seed `327b8688`, palette and exact nominal height **1.68 m** are preserved.
The private modeling pass preserved gameplay. Combined integration subsequently
applies a separately reviewed 1.5 m staff-side translation to the teller spawn,
home and three patrol points, removing a verified counter intersection while
preserving route shape/timing, speed, schedule and existing alarm/reset behavior.
See `INTEGRATION_REVIEW.md` for the correction and fresh combined evidence. There
is no satchel. This is a reviewed first checkpoint, not a claim of GTA VI parity.

## Scope and exact source

Dedicated reference/modeling work began **2026-10-06 19:04:28 UTC**. The full
120-minute allocation was met at **21:04:28 UTC**; validation and reporting
continued until **21:12:24 UTC** (127 minutes 56 seconds total).
`WORK_LOG.md` records the actual work. No interruption,
approval wait, another actor's work or parallel processing time was added.

The private study began from archive tree
`6c467c0bfa678f4a478113a82f3e4bc05d174ddb` plus 35 individually hashed frozen
inputs, yielding exact starting tree
`6b0faf03aa88af460aa8cc19eff935f14691d251`. Those inputs include the corrected
Tessa shirt and an **intermediate Owen**. `BASELINE.json` records all hashes.
The frozen Lia code patch yields tree
`c9ea67d057897444b0eb8afa400bc261789b5dfe` against that private start. A separate
camera-only diagnostic patch was used for captures; see `CAPTURE_INPUT.json`.

These private world captures and measurements are the isolated Lia study.
They are **not the final combined main build**. The surgical source changes integrate onto accepted Owen tree
`9e21e9a1aae79fb7abd7a9cf3e177349581ef14d` and recaptures combined evidence.
The private baseline and its preserved before executables remain unchanged.
No publishing occurred; the last verified public head is
`348bdd900482b45918d753d17dc08e284a095453`.

## Modeling and fitting

The 17-target original anatomical recipe has a broad upper face, tapering
compact chin and individually adjusted nose, mouth and eyelid corners. It uses
only pinned MakeHuman/MPFB2 graphical CC0 data, original importer code and
original Fury construction. Every one of the **8,704 source triangles**,
4,378 source positions and 4,572 UVs remains traceable. Original FaceUV charts
and mip behavior are retained. `ANATOMY_PROVENANCE.md`, the per-input manifest,
recipe and independent validation describe exact source identities and rights.
Proprietary reference photos are private research only and are not shipped.

A complete **50-edge attachment loop** is sewn to a hidden skin continuation,
using identical positions, normals and weights. The collar queries Lia's own
source neck triangles. Eyes are seated against her actual socket edges. Lower
neck weights remain supported below the collar while the upper head turns.

The shirt has its own garment source and shape. Shoulder openings and sleeves
share sewn boundary nodes, the inner yoke follows Chest and outer shoulders
transfer into arm weights. Collar leaves have actual thickness along normals;
placket, buttons, tie and badge follow actual cloth triangles and their weights.
The final arm fit retargets both upper and lower segments with matching cuffs,
keeping the source forearm/upper-arm ratio close to its anatomical reference.
`ARM_FIT.json` records the bind-pose coordinates and before/after lengths.

The trousers are one connected waist/rise/two-leg surface, with three intended
return loops and exact UV seam weights. A white triangular speck was diagnosed
as the tucked shirt crossing the concave trouser rise. The hidden shirt tail
was tapered inside the actual garment; an enclosure regression rejects the
old geometry in both LODs. Symmetric thigh support across the shared rise and
a fitted posterior envelope address deep-crouch deformation/contact.

Hair uses a continuous opaque surface fitted to the actual source skin crown,
short boundary cards and directional crown ribbons. Using the crown joint as
a surface center was rejected. The final cap passes **390 original scalp
vertex/centroid coverage probes per LOD**. Near and Far top/oblique views were
inspected; no exposed scalp sliver was seen. HairShortStrands atlas index 12
and all shared maps/rendering/animation APIs are unchanged.

Final geometry: **Near 24,637 vertices / 39,880 triangles; Far 9,863 / 15,944**.
The source anatomical face is retained at both levels. Height, soles and the
original role palette are checked separately from visual framing.

## Verification and visual evidence

The final source passed the canonical Near/Far geometry/motion run and a full
ASan/UBSan run at heights **1.35, 1.68 and 2.10 m**, with **198 sampled
idle/walk/run/talk/crouch/crouch-walk poses plus 30 attention turns per LOD per
height**. The sanitizer run finished with exit 0 in 244.26 seconds; source
hashes before and after match. Tests cover finite/unit geometry, winding,
weights, exact source/UV retention, socket seating, scalp coverage, connected
seams, shirt containment, cloth contact, soles, deterministic construction,
174 bit-exact generic fallback cases and invalid-input rejection. Pose
sampling/skinning produce no per-frame allocation or renderer-handle churn.

Independent importer verification reproduced the header exactly and rejected
39 malformed-input cases. Four actual offscreen OpenGL suites passed:
materials, world water, character renderer and character surface. Captures
report **OpenGL 3.3**; logs identify **Mesa llvmpipe**, a software implementation.
This proves compatibility, not GPU hardware performance. Thirteen optimized
character/renderer/NPC/runtime suites are also recorded with individual logs
in the accompanying evidence package.

Personally inspected final captures include face and body at front/back,
±45°, ±90° and ±135°, both LODs, top/low views, neutral material, eight posed
states and actual authored Vaultline lobby scenes. The initial -30° body
camera was below the stage floor; those failed views remain preserved. Added
-10° body views keep the camera above the floor. Diagnostic stage images and
production-game captures are labeled separately in their reports.

Real live clips retain all scenery and AI. The counter/monitor naturally hides
Lia's lower body. The distance 2.6 m, orbit -45°, elevation 25° body/talk clip
shows useful upper-body movement early, then monitor occlusion and a guard
crossing around frame 29. No occluded frames were removed. The distance 0.86 m,
orbit 0°, elevation 0° face clip supplies a closer live attention view. Videos
are fixed **12 fps simulation sampling**, not a measured rendering frame rate.

## Measured cost and remaining gaps

Three alternating fresh-process repeats, four frames each, two CPU workers,
median last-frame times on the same cloud machine:

| View | Private before | Lia after |
|---|---:|---:|
| Isolated 640×640 portrait raster | 50.6 ms | 226.8 ms |
| Actual 640×360 lobby raster | 214.6 ms | 229.3 ms |
| Actual 640×360 street raster | 210.5 ms | 214.2 ms |
| Actual 320×240 lobby path, 1 spp / 2 bounces | 421.8 ms | 421.0 ms |

The detailed portrait costs substantially more than the old generic actor.
Populated-scene changes are smaller and noisy; there is no scene-speed
improvement claim. Raw reports include source fingerprints and scene metrics.
These are CPU observations on the fixed private baseline, not final combined
main results or hardware guarantees. Four-sample path images are deliberately
retained as noisy compatibility evidence, not final-quality beauty renders.

The crown still reads as a smooth cap despite directional detail. Skin remains
waxy/smooth; broad cloth areas need more convincing material and tailored fold
variation. Thin collar/placket edges alias at body scale. Deep crouch compresses
the rise into broad, somewhat mechanical folds; Far silhouettes are coarser.
Hands/facial performance retain the existing rig's limitations. The joined
anatomy and garments improve continuity and identity, but these visible gaps
remain work for later passes rather than being concealed or declared solved.

## Deliverables

`out/integration/lia-code-frozen.tar.gz` contains the Lia-only source patches,
small dispatch/CMake patch, test-registration adaptation and exact manifest.
It was clean-applied in an independent alternate index to reproduce the code
result tree. `out/delivery` contains the final report/provenance patch, bounded
source recovery and selected evidence package with SHA-256 manifests. Original
full-resolution frames, private references, baseline executables and the
midpass full portable archive remain in the private study, preserved intact.
