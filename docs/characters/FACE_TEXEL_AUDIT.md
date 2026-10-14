# Anatomical face texel audit

The old 512px Face tile supplied only about **87 × 104 texels for Mira's
central facial patch** and **88 × 106 for Jon's**. Those are UV-axis extents;
the anatomical horizontal direction runs approximately along V. The complete
external-head chart was 157.46 × 299.02 texels, so most of its resolution served
the sides, back, crown and ears. Its lip tint core was about 28 × 11 anatomical
texels, and an exterior eye vicinity about 15 × 9.

The shared atlas now gives those same source triangles **2.88322× linear
resolution / 8.312966× area**: a 1024px Face tile with a 64px outer gutter and a
positive, uniform 1.44× repack of the external-head chart. Canonical generated
UVs and their provenance are unchanged. No chart is rotated, reflected, warped,
split, merged or independently allocated per actor.

## Method and source identity

This audit parsed the actual generated positions, triangle corners, UV corners,
and source indices. Connected components were checked through both shared UV
vertices and shared UV edges; both identify the same five islands. Feature
areas were calculated by clipping actual triangles against the documented
geometry-space planes, interpolating UVs through the clipped polygons, and
summing triangle UV area. Bounding boxes are not mistaken for occupied area.
Raster counts independently reproduce the generator's texel-center sampling.

The original Face interior contains 448 pixels but spans **447 texel-center
intervals**, which is the multiplier used below. The new interior is 896 pixels
and spans 895 intervals. Results are based on the pinned headers:

- Mira: `7e0376b09a9d2fce33421258879daab3edd2363d7409c8753ed83da373c030d0`
- Jon: `5ecac83a75fe83a6ca78fc088b45723213167b6b27496e1a36a917e69c7cedac`

All 4,628 source UV indices common to both selections have exactly equal UV
coordinates. Four complete islands are identical; the selected neck/shoulder
region differs. No upstream program code was imported or executed.

## Original island allocation

Coordinates are canonical source UVs. Extents below are U × V texel-center
intervals, before padding. The head is one connected external chart, not two
separate halves.

| Island | Triangles Mira / Jon | Canonical bounds U; V | Old pixel extent | Occupied texel² Mira / Jon |
|---|---:|---|---:|---:|
| External head | 6,232 / 6,232 | .641249–.993515; .148696–.817634 | 157.46 × 299.02 | 35,604.52 / 35,604.52 |
| Mouth interior | 796 / 796 | .757634–.923740; .011863–.136440 | 74.25 × 55.69 | 3,124.13 / 3,124.13 |
| Neck / shoulder selection | 624 / 676 | Mira .237369–.523692; .850719–.973856; Jon .215132–.545929; .840623–.973856 | 127.99 × 55.04 / 147.87 × 59.56 | 5,137.42 / 5,793.15 |
| Negative-X eye-socket interior | 586 / 586 | .011809–.076822; .547391–.632858 | 29.06 × 38.20 | 853.83 / 853.83 |
| Positive-X eye-socket interior | 586 / 586 | .086143–.151156; .547391–.632858 | 29.06 × 38.20 | 853.83 / 853.83 |

The actual mapped old Face pixels are 45,555 for Mira and 46,210 for Jon,
respectively **22.70% and 23.02%** of the 448² interior. The external-head
chart alone maps 35,598 pixels. The small analytic/raster difference is normal
boundary sampling. Seven raster centers are shared by incident triangles;
the audit found no competing overlapping islands.

The five bounding rectangles are pairwise disjoint, including their union
across Mira and Jon. The closest source-rectangle separations are the two eye
charts (U gap .009321, 4.17 old texels) and head/mouth (V gap .012256, 5.48 old
texels). Canonical U,V therefore identify the chart without an explicit chart
ID for this topology.

The original chart seams correspond to existing anatomy: 38 edges around the
mouth opening, 28 at each eye opening, and 48 at the head/neck connection.
These existing corners remain separate for UVs and share the existing normal
smoothing groups. No new seam is introduced by repacking.

## Feature resolution

The central patch selects external-chart geometry with |X| ≤ .04,
.375 ≤ Y ≤ .475 and Z ≥ .045. The lip tint core uses |X| ≤ .018,
|Y − .389| ≤ .0045 for Mira or |Y − .392| ≤ .0045 for Jon, and Z ≥ .080.
Each exterior periocular patch uses the generated eye center ±.010 in X,
±.005 in Y and Z ≥ eye-center Z + .006. These are reproducible feature
proxies, not anatomical segment labels or camera-dependent visibility masks.

| Feature | Old Mira U × V | New Mira U × V | Old Jon U × V | New Jon U × V |
|---|---:|---:|---:|---:|
| Central facial patch | 87.00 × 104.35 | 250.84 × 300.86 | 87.54 × 106.12 | 252.40 × 305.96 |
| Lip tint core | 11.29 × 28.41 | 32.54 × 81.90 | 12.72 × 26.54 | 36.67 × 76.51 |
| One exterior eye vicinity | 8.53 × 15.50 | 24.59 × 44.68 | 9.82 × 15.05 | 28.30 × 43.39 |

The old central patch occupied 7,472.59 / 7,703.47 texel² (Mira / Jon),
and now occupies approximately 62,119.40 / 64,038.71 texel². The complete external
head becomes **454.00 × 862.13 texels**, occupying 295,979.16 texel².
Rasterized mapped totals become **335,958 / 338,605** in the 896² interior.
Every source triangle retains its signed orientation.

The eyeballs, sclera, iris and pupils are separately modeled with the unchanged
`Eyes` material tile. The eye figures here describe the skin around the eye,
not iris resolution. Likewise, geometric lip/eyelid detail is independent of
these texture measurements.

## Implemented transform and API

`character_surface_uv(Face, source_u, source_v)` still accepts the canonical
generated source UVs. Each inclusive source rectangle uses a 1e−7 boundary
tolerance, much smaller than the nearest inter-chart gap. For a matching
rectangle:

`packed_local = (source_uv − source_min) * scale + origin / 447`

| Chart | Reference origin X, Y | Uniform scale | Owned Face-tile cell X, Y, W, H |
|---|---:|---:|---:|
| External head | 8, 8 | 1.44 | 0, 0, 576, 1024 |
| Mouth interior | 288, 8 | 1 | 576, 0, 448, 256 |
| Neck / shoulders | 288, 112 | 1 | 576, 256, 448, 256 |
| Negative-X socket | 288, 240 | 1 | 576, 512, 448, 256 |
| Positive-X socket | 288, 368 | 1 | 576, 768, 448, 256 |

Final atlas coordinates use the existing texel-center mapping with the new
Face interior. Rasterization calls the identical chart transform. Geometry,
normals, source arrays, source indices and triangle provenance do not change.
Unknown finite Face coordinates retain the previous clamped identity mapping;
this preserves the (0,0)/(1,1) tile-bound inspection convention. Actual source
triangles are validated against the supported layout. A later source topology
or wider neck selection must extend the shared layout deliberately; silently
assuming an unknown chart fits is not supported.

Every chart cell starts and ends on the 64px grid. Every used source UV is at
least 32px from its cell edges. Geometry-space pigment is padded only within
the owning cell, and the normal derivative stencil never crosses a cell edge.
This protects both chart and material isolation through level six without
renderer-specific sampling. Skin-height fields retain canonical source UV
frequency, and derivative magnitudes compensate for each chart's uniform
scale. Tangent direction and handedness are preserved because the transform
has a positive scalar Jacobian.

## Shared memory and preservation

The 1536² atlas keeps three RGBA8 images and six additional levels:
768, 384, 192, 96, 48 and 24. The retained pixel buffers allocate exactly
**37,746,432 bytes (35.9978 MiB)** once per process. CPU and GL consume the
same images and supplied bounded mip chain. This count excludes small C++
object/vector bookkeeping, temporary generation buffers and any backend GPU
copy; it is not a process-RSS claim.

All nine other surfaces retain their original tile size, gutter, procedural
parameters and corresponding local pixels. A before/after comparison covered
all three maps and all seven levels, **12,582,144 bytes**, with **zero differing
bytes**. Both concatenated snapshots have SHA-256
`005b0ff8ddcee049661e679b8c0b525a6d83b3c837aa9e509be66a589ed6529b`.
Their atlas origins change only by multiples of 64, preserving the entire
retained mip grid. The atlas has intentional unused space initialized to opaque
white reflectance, +Z tangent normals and rough nonmetallic ORM.

Diagnostic JSON, before/after binary snapshots and island images are under
`out/character-study/face-texel-audit/`. The diagnostics use one CPU process
and the repository's generated data only. Focused runtime verification is
recorded in [the shared surface contract](../CHARACTER_SURFACES.md).

## Subsequent HairStrands addition

The original procedural HairStrands tile was later appended at (1024,1024),
256² with a 32px gutter, inside previously unused atlas space. It leaves all
Face transforms, measured densities and source provenance above unchanged.
All ten pre-existing tiles/maps/mips compare byte-identically across
29,358,336 bytes. Total retained atlas allocation is unchanged. See the
[shared surface contract](../CHARACTER_SURFACES.md#hairstrands-verification)
for mask semantics and renderer verification.
