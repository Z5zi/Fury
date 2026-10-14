# Lia Merrow — anatomy provenance and fit

Lia uses a separate original graphical recipe, derived from pinned
MakeHuman Community / MPFB2 CC0 asset data. This does not import a person's
likeness or establish age, ethnicity, gender identity, family or biography.
The character keeps the existing `NpcTeller` seed, BankStaff role and palette.

## License boundary and exact inputs

Upstream: https://github.com/makehumancommunity/mpfb2

Immutable source commit: `d0a32e57a7f915cb2f2b95410e2117648c7bbb7e`. The upstream
[asset license](https://github.com/makehumancommunity/mpfb2/blob/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/LICENSE.ASSETS.md)
and [license boundary](https://github.com/makehumancommunity/mpfb2/blob/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/LICENSE.md)
separate CC0 graphical data from addon program logic. Only numeric graphical
morph inputs are newly vendored. No addon code was copied, installed, imported
or run. The importer is Fury's own standard-library OBJ/target integration.

The [manifest](../../../third_party/lia_anatomy/MANIFEST.json) records every
input's pinned URL, upstream path, byte size, SHA-256 and original Git blob.
It references the existing base OBJ, vertex-group metadata, licenses, and
reusable graphical targets in place. It includes 6 new numeric
assets and 15 shared inputs; no unused target is accepted.
The source tree was independently checked against the exact Git blob IDs.

The [recipe](../../../third_party/lia_anatomy/lia_recipe.json) has 17 explicit
weights: a separate feminine-presenting base, broader upper facial planes,
soft inverted-triangle head shape, compact chin, subtle nose-tip and mouth
changes and small eye-corner changes. The lower neck broadens into the shirt.
Upstream parameter names identify controls only; they are not demographic
claims about Lia. The original importer verifies macro normalization and
all finite/bounded numeric weights before generating geometry.

## Geometry retained and normalized

A single scale maps the full morphed source body's feet to -0.5 and bald crown
to +0.493. Source axes remain +X anatomical left, +Y up and +Z front. Runtime
multiplies by the exact requested height, canonically 1.68 m. Hair alone uses
the reserved upper 0.007; footwear soles remain exactly -0.5.

The complete-face cutoff is Y >= 0.330. The result has **4,378
source positions**, **8,704 exact original triangles** and
**4,572 source UVs**, with zero dropped degenerate triangles,
nonmanifold edges or inconsistently directed shared edges. There is one
connected component and one complete **50-edge** lower
attachment cycle. Its bounds are (-0.06075633, 0.33019302, -0.02387996)
to (0.06075633, 0.33989887, 0.03941032). Runtime attaches a separate
Skin continuation to every exact edge, with identical positions/weights and
shared smooth normals; only its lower termination is hidden inside the shirt.
The original source Face triangles and UVs are not altered to conceal the cut.

The original Face UV charts remain distinct:

| Chart | Retained source triangles |
| --- | ---: |
| external_head | 6,232 |
| mouth_interior | 796 |
| neck_shoulders | 504 |
| right_eye_socket | 586 |
| left_eye_socket | 586 |

Each triangle retains source face, source triangle, vertex and per-corner UV
IDs. Runtime atlas tests verify the actual chart bounds, mappings and isolated
mip filtering. No decimation removes lid, lip, ear or socket topology in Far.

## Lia-specific measured fitting

The source head pivot is **(0.00000000, 0.40662281, 0.02603178)** and neck pivot
**(0.00000000, 0.34054454, -0.00087757)**. The crown helper is
**(0.00000000, 0.49339438, 0.02187566)**, whereas the actual highest skin
landmark is **(0.00000000, 0.49300000, 0.02619102)**. An early hair prototype
incorrectly treated the helper as a surface center; the actual skin landmark
now controls the top cap. A rig helper is not a geometric surface point.

The anatomical-left (+X) eye helper center is
**(0.01850752, 0.42867268, 0.06952936)**, with half-extents
**(0.00928384, 0.00885803, 0.00948867)**. Actual eye-opening mean is
**(0.01918105, 0.42825005, 0.07469559)**, spanning **(0.01169969, 0.42572646, 0.07223821)** through
**(0.02597891, 0.43147437, 0.07644815)**. Each rim has 28 ordered original source vertices.
The opposite side is reflected in X. Runtime eye placement is tested against
every rim point and interior spoke in the actual mesh, not merely the helper.
The external mouth seam mean is **(0.00000000, 0.39006124, 0.08277704)**, distinct from the
internal mouth joint; this supplies the modest lip pigment zone.

Closed neck-chart cross-sections run from Y 0.340 through 0.365. Higher
sections contain partial neck charts and/or projecting jaw geometry and must
not be mistaken for collar width. The shirt queries Lia's actual triangles,
and the head blend begins above the collar stand so attention motion does not
pull the lower neck through chest-supported cloth.

## Independent verification

`python3 scripts/build_lia_anatomy.py --check` verifies exact reproduction,
pinned bytes and all source identities. The independent audit in the private
evidence directory checks every emitted triangle corner against the original
OBJ, every emitted UV as binary32, and rejects **39** malformed-input cases.
The manifest, recipe, generated header and audit each carry a SHA-256.
The generated header is compiled as C++17 in the actual engine and Lia tests.

[VALIDATION.json](../../../third_party/lia_anatomy/VALIDATION.json) records
shape correspondence and the evaluated cutoffs. Every one of the 4,378 common
source positions differs from Mira, Jon and Tessa; 4,358 common positions all
differ from Owen. RMS normalized displacement is approximately 0.001913 from
Mira, 0.004272 from Jon, 0.013695 from Tessa and 0.004389 from Owen. These are
source-correspondence differences, not clinical measurements or a claim of
finished likeness quality. Runtime hair, material, garment, motion and world validation is recorded in
PASS_01.md; visible quality limitations remain explicitly listed there.
