# Rina Holt: anatomy and original authoring provenance

Rina's data derives solely from the already pinned MakeHuman/MPFB graphical
assets at commit `d0a32e57a7f915cb2f2b95410e2117648c7bbb7e`.
The [manifest](../../../third_party/rina_anatomy/MANIFEST.json) independently pins
21 shared files by upstream URL, byte count, SHA-256 and Git blob ID. This
includes the mesh, UVs, mesh-information metadata, 17 numeric morph targets and
both license statements. It adds no third-party executable or addon code.

The upstream [license boundary](https://github.com/makehumancommunity/mpfb2/blob/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/LICENSE.md)
separates GPL addon program logic from CC0 graphical data. The retained
[CC0 statement](../../../third_party/mpfb_assets/LICENSE.ASSETS.md) applies to
the latter. Only graphical data is used. Fury's own OBJ/numeric-target import,
source correspondence, fitting, clothing, hair, skinning integration and tests
are original project code. No MPFB Python addon is installed, run or copied.

Rina has a separate [head recipe](../../../third_party/rina_anatomy/rina_recipe.json),
[hand recipe](../../../third_party/rina_anatomy/rina_hands_recipe.json) and
[generated statistics](../../../third_party/rina_anatomy/GENERATED_STATS.json).
Graphical target filenames do not establish ethnicity, literal age, gender
identity or a personal history. The deterministic source palette is untouched.

## Preserved head and texture correspondence

The recipe applies every weighted numerical target before one uniform body
normalization: source body feet map to −0.5 and source crown to +0.493, leaving
0.007 normalized height for the close crop. Coordinates retain +Y up and +Z
front. The source body selection keeps whole original faces above normalized
Y=0.32; no head triangle is manually edited, dropped or rewound.

Both LODs retain 4,447 unique source positions and 8,844 original triangles,
including the connected ears, mouth interior, socket walls and neck/upper
shoulders. Original per-corner UVs, source vertex/UV/face/triangle IDs and the
complete 48-edge attachment cycle are emitted. The runtime joins an exact Skin
underlay to that cycle and uses the morphed source neck/head pivot landmarks.
Canonical Face atlas mapping remains shared; position-based lip/lid pigment
is fitted to Rina's measured source seams, and actual eye rim coverage is tested.

The separate independent source test reads the raw OBJ/targets without importing
the producer, reconstructs all output coordinates and indices, and rejects
altered positions, triangle winding and UV corners. Regeneration with --check
must match the emitted header and statistics byte for byte.

## Connected hands and rest fitting

The source hand is selected in Rina's own morphed wrist frame: the positive-side
filter is 0.60 times the actual source wrist X and the wrist-plane offset is
−0.18 source units. The chosen patch is one connected disk with one complete
20-edge wrist loop. Near retains all 3,276 source triangles per hand, original
source IDs and per-corner UVs. A source-corner split handles a concave thumbnail
fold without removing geometry.

Fury's original endpoint-quadric Far reduction locks the entire wrist cycle,
checks the manifold link condition and rejects inverted/degenerate local
triangles. Each Far hand has 520 triangles. Independent topology checks verify
one component, Euler characteristic 1 and the identical complete wrist. Finger
cross-section counts match the source at four planes in the generated report.

The recipe documents a mild, monotonic static rest curl. Runtime fitting rotates
the whole hand 58 degrees inward around its local wrist, mirrors the other side
with corrected winding, and attaches the forearm to every exact wrist point.
There are no new finger bones or animated fingers. The source UV corners and
rigid source-hand fitting are tested separately from silhouette review.

## Original clothing and hair

The compact plum shirt reuses Fury's own verified physical-node sewing method,
with Rina's independently fitted dimensions, rounded bound neckline and sleeve
lengths. Torso, yoke and sleeves share actual positions and skin weights across
UV splits; the shirt has exactly four open boundaries: neck, hem and two cuffs.

The orange vest is a separate shell cut from that actual shirt surface, with
Rina's V opening, larger armholes, shorter curved hem, local hanging ease,
lining, turned thickness, pale material tapes and the existing left cargo
pocket. A spatial tolerance joins arithmetic-equivalent clip intersections;
shared cut edges are oriented into a closed genus-two garment shell. Triangles
are not silently discarded to pass contact tests. The basic shirt and vest
method is shared engineering; the recipe, dimensions, fitting and pixel review
are individual Rina work.

The scalp cap is fitted to Rina's actual morphed head. Short masked cards are
fitted to the polygons actually rendered, with nested boundary roots and small
clearance beyond the cap edge. This is bounded opaque/cutout geometry with
isotropic material response, not full strand simulation or anisotropic hair.
The source-surface test checks cap triangle crossings and more than 21,000
actual scalp samples in each LOD, rather than merely checking cap vertices.

The brown footwear has a distinct upper/sole outline, narrow welt, fitted tongue
and laces projected onto each actual LOD upper. The trouser crotch is physically
joined across mirrored halves. Knee rings follow the anatomical pivot, the
posterior knee uses a broader blend for compression, and a shallower crotch
rise keeps the connected seat clear in deep-crouch samples. The cargo panel retains
actual underlying trouser triangle correspondence and influences, including
the coarse Far topology, rather than draping a disconnected analytic grid. Clothing and shoes use the original shared
surface atlas; no proprietary reference pixels ship in any material.
