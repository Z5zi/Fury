# Original character surface atlas

The character material is an original, deterministic, runtime-generated 1536 ×
1536 PBR atlas. It adds surface response to the modeled face, hair, clothing and
accessories while preserving linear vertex colors as the character's skin tone,
hair color and wardrobe palette. It contains no downloaded textures, captured
likeness, painted illumination, highlights, ambient occlusion, or emission.
The analytic fields and generated pixels follow the repository's MIT license.
Face placement uses separately documented CC0 anatomy and original UV data;
see [anatomy provenance](characters/mira/ANATOMY_PROVENANCE.md) and the
[measured texel-density audit](characters/FACE_TEXEL_AUDIT.md).

## Integration contract

Include `fury/character_surface.hpp`. For each vertex, convert the part's local
coordinates using `character_surface_uv(CharacterSurface::Jacket, u, v)` or its
appropriate surface. Draw the mesh with `make_character_surface_material()`.

The factory supplies white albedo, roughness factor **1**, metallic factor **1**,
normal scale **1**, opaque coverage and UV0 mapping. Atlas G/B channels supply
final roughness and metallic values. A metallic factor of zero would turn
hardware into a dielectric; roughness 0.55 would make every material too glossy.
Existing per-vertex colors stay linear. The atlas is a near-neutral reflectance
multiplier rather than an untinted character color palette.

`character_surface_textures()` returns one shared immutable `MaterialTextures`
instance. Generation occurs once on first use. Every actor and near/far mesh
shares it; there is no per-actor texture duplication.

### Stable regions

Coordinates are pixels from the atlas's top-left corner. Face has a 64px gutter;
all other tiles retain their original 32px gutter and content resolution.

| Surface | Origin X, Y | Tile | Usable content | Approximate roughness |
|---|---:|---:|---:|---:|
| Face | 0, 0 | 1024 × 1024 | 896 × 896 | 0.46; slightly smoother nose/lips |
| Jacket | 1024, 0 | 512 × 512 | 448 × 448 | 0.79 |
| Hair | 1024, 512 | 256 × 256 | 192 × 192 | 0.39 |
| Skin | 1280, 512 | 256 × 256 | 192 × 192 | 0.46 |
| Shirt | 1024, 768 | 256 × 256 | 192 × 192 | 0.86 |
| Pants | 1280, 768 | 256 × 256 | 192 × 192 | 0.82 |
| Leather | 0, 1024 | 256 × 256 | 192 × 192 | 0.55 |
| Eyes | 256, 1024 | 256 × 256 | 192 × 192 | 0.17 |
| Rubber | 512, 1024 | 256 × 256 | 192 × 192 | 0.88 |
| Metal | 768, 1024 | 256 × 256 | 192 × 192 | 0.29 |
| HairStrands | 1024, 1024 | 256 × 256 | 192 × 192 | 0.39; coherent alpha cutout |
| HairCrop | 1280, 1024 | 256 × 256 | 192 × 192 | 0.64; opaque short fibers |
| HairShortStrands | 0, 1280 | 256 × 256 | 192 × 192 | 0.65; matte short-card cutout |

All surfaces are nonmetallic except hardware, which uses metallic 0.94.
`character_surface_region()` exposes these rectangles; invalid enums throw
`std::out_of_range`. `character_surface_uv()` clamps finite inputs to [0,1];
nonfinite inputs throw `std::invalid_argument`. Non-Face coordinates map
uniformly to the first/last interior texel centers.

### Hair-card strand mask

`HairStrands` is appended at enum index 10; all earlier enum values and atlas
rectangles remain stable. It occupies previously unused atlas space, with no
new material sampler or pixel-buffer allocation. Use `Hair` for the opaque
underlying hair mass and `HairStrands` for supplemental near-detail cards.

Card U crosses 18 continuous, gently curved longitudinal strands. V=0 is the
root under the opaque mass; V=1 is the tapered tip. Roots start around V=.025–.05
and fade into full width over the next approximately .05. Tips have varied
lengths around V=.82–.95 and taper in both width and coverage. The 192px content
perimeter and all clamped gutters have zero alpha. The construction does not
cut random holes or bake highlights into the hair. Linear near-neutral strand
reflectance preserves the authored vertex hair color; shallow tangent normals
and rough dielectric ORM supply the lighting response.

Set the card-bearing material's `alpha_cutoff=.4f` and leave `alpha_blend=false`.
The existing software, CPU-ray and GL alpha-mask paths discard holes before
writing depth, without transparency sorting. Every tile except the two hair-card masks has alpha 255,
so the same cutout-enabled character material preserves its opaque regions.
Only HairStrands and HairShortStrands base-color alpha varies; normal/ORM alpha remains 255. A near-only
card arrangement can retain the opaque hair mass for the far mesh.

### Opaque short-crop cap

`HairCrop` is appended at enum index 11 in the previously unused tile at
(1280, 1024). All preceding enum values, tile rectangles and local map/mip
payloads remain unchanged. Its 192px interior contains original dense, short,
tapered fibers with varied bends, lengths and staggered roots. The field is a
neutral reflectance multiplier: the actor's vertex colors still set hair color.
It contains no captured hair image, painted highlight, directional shadow or AO.

Use it on the opaque cap, with U wrapping and V following hair growth. Normal
derivation assumes one approximately 0.08m-wide cap sector and 0.16m of growth
travel, matching the short crop's repeated sector UVs. Fiber relief is 14μm with
2μm grain, preserving subtle slopes at that scale. Linear ORM keeps R=1, G near
0.61–0.66 and B=0; all map alpha remains one, including gutters and every mip.
The higher roughness softens the broad plastic highlight of the original Hair
tile. This is a matte isotropic approximation using the existing renderers,
not an anisotropic hair shader or a claim of realistic strand light transport.
Sparse `HairShortStrands` boundary cards may supplement its silhouette; the
older `HairStrands` tile retains its glossier long-card response.

The tile uses the same wrapped U/clamped V gutters and level-six mip cap as
the other opaque surfaces. Atlas dimensions, map count, retained pixel bytes,
sampler count and shared ownership remain unchanged.

### Matte short-card strands

`HairShortStrands` is appended at enum index 12 in the previously unused tile at
(0, 1280). It matches narrow, short silhouette cards around a `HairCrop` cap.
The long-card `HairStrands` and opaque `HairCrop` tiles remain unchanged.

U crosses the same 18 connected filaments and V follows their roots to tapered
tips. Every alpha byte, including the transparent clamped gutters and all six
additional mips, matches the existing connected long-card mask. The mask has no
random holes or detached noise. Set `alpha_cutoff=.4f` and keep blending off.

Normal derivatives use a **0.006 × 0.045m** card span, fitting the approximately
4–6mm-wide, 20–45mm-long short cards. Primary relief is **7μm**, with **0.5μm**
fine variation. A near-neutral, low-contrast reflectance multiplier preserves
authored hair color, and dielectric roughness stays approximately **0.633–0.667**.
This matte response brings the card highlights closer to the cap. It adds no
baked light, ambient occlusion, emission or anisotropic shading model.

Both axes clamp, preserving transparent perimeter texels rather than wrapping
an opaque filament across the card edge. The existing material ABI, three maps,
1536² dimensions, level-six mip cap and 37,746,432-byte allocation remain unchanged.

### Source-preserving anatomical Face mapping

Face accepts the anatomical head's **unchanged original per-corner UV stream**,
then applies one positive uniform affine transform per chart. There is no V
flip, rotation, reflection, nonlinear warp or new seam. The external-head chart
is enlarged by 1.44× inside the doubled Face tile. The mouth, neck and eye-socket
charts keep their relative linear scale. These are source charts rather than
cylindrical theta/height coordinates.

The five canonical bounding rectangles are disjoint, so U,V identify the chart
unambiguously for Mira, Jon, Tessa and Owen. Every source triangle stays affine; its
tangent direction and handedness are preserved. Unknown finite Face coordinates
retain the previous clamped identity mapping, including (0,0)/(1,1) for tile-bound
inspection. Do not use arbitrary cap averaging across Face charts. A later
source topology or wider neck selection requires explicit layout validation.

For the pinned recipe, example canonical lip UVs are approximately
(0.9011, 0.4832), chin (0.9536, 0.4832), nose (0.8732, 0.4832), one eye vicinity
(0.8345, 0.5154) and cheek (0.8676, 0.5324). These illustrate source locations;
they do not place pigment by atlas axis or change the stored source arrays.

Generation rasterizes the source triangles into the 896 × 896 Face interior
using exactly the same chart transforms as mesh UVs. It interpolates normalized
anatomical XYZ through the triangle corners. Pigment fields use those positions:
lips around Y=0.391, nose Y=0.408, cheeks Y=0.412 and eyes Y=0.42767. A front-facing
Z gate keeps the colors off the back of the head. Internal mouth/eye rig joints
are not treated as visible skin landmarks.

The head owns a 576 × 1024 cell. Four separate 448 × 256 cells occupy the right
column. All cells align to the 64px mip grid, and every used UV is at least 32px
inside its cell. Geometry-space pigment extends only within the owning cell,
so filtering remains isolated through level six. The temporary lookup is
released after generation.

Fine pore-height fields remain in **canonical source UV coordinates**. Their
physical frequency stays fixed while the atlas gains samples. Height derivatives
compensate for the uniform chart scale and use one-sided stencils at cell
boundaries; they never differentiate across unrelated charts. Broad geometry,
eyelids, lips, iris, pupil, eyebrows and garment folds remain geometry. The eye
material does not paint a pupil or catchlight.

Duplicate vertices at a loft's U=0/U=1 seam so no triangle interpolates across
the atlas or the entire circumference. Parts with different materials need
separate vertices. For a non-Face cap, averaging the UVs of a same-surface ring
is safe: the helper is affine and the rectangle convex. This preserves material
selection but does not supply a radial cap unwrap. CPU and OpenGL use the same
stored image and UV convention, and derive tangent frames from the authored UVs.

## Map semantics, filtering and allocation

- Base-color RGB is linear reflectance encoded as **sRGB8**, sampled and filtered
  in linear light; alpha is one except the HairStrands and HairShortStrands cutout masks
- Tangent normals encode XYZ in linear RGB8 and derive from shallow physical
  height fields. Their +U/+V derivatives match mesh UVs; mips are renormalized
- ORM is linear: **R=1 (unoccluded), G=roughness, B=metallic**, alpha one. The
  shaders use G/B; reserved R makes no unsupported texture-AO claim
- Every map has six additional mips: **768, 384, 192, 96, 48 and 24** pixels.
  `MaterialTextures` mip element zero is level one

Fine cloth yarns, leather grain, skin microvariation, shallow curved hair bundles
and gentle metal brushing have bounded contrast and slopes. Actual geometry and
lighting produce highlights and shadows; maps do not bake illumination.

Wrapped U gutters continue the opposite content edge for filtered wrapping;
HairStrands and HairShortStrands clamp both axes to transparent margins, while
V gutters duplicate the nearest edge for cuffs/hems. Face uses clamped outer
padding plus isolated chart cells. The 64/32px gutters keep bilinear/trilinear
footprints inside the selected material through level six, including endpoints
and corners. Cell padding additionally protects actual anatomical UVs from
neighboring Face charts. Very large footprints clamp to the final 24 × 24 level.
Smaller full-atlas mips would combine unrelated charts and materials.

**Backend requirement:** honor valid supplied bounded mip chains. CPU samplers
clamp to the last supplied level. OpenGL uploads supplied levels and sets
`GL_TEXTURE_MAX_LEVEL`; automatic mip generation is permitted only when no valid
additional mip was supplied. Regenerating down to 1 × 1 defeats atlas isolation.

Retained pixel buffers allocate exactly **37,746,432 bytes (35.9978 MiB)** for
three RGBA8 maps and all six additional levels, once per process. Small C++
object/vector overhead, temporary generation buffers and backend GPU copies
are additional. There is no per-frame generation, random state or dependency on
image downloads/files on disk.

All nine other surfaces are byte-identical to the preceding 1024² atlas at every
corresponding local texel of every map/mip. A full **12,582,144-byte** comparison
found **zero differences**. Only their atlas origins change, always by multiples
of 64. After adding HairShortStrands, the atlas leaves 327,680 base texels unused; they initialize to
opaque white color, +Z normals and rough dielectric ORM.

## Focused verification

`tests/character_surface_tests.cpp` validates the nonoverlapping layout, exact
allocation, valid UVs, normalized restrained tangent normals, alpha contracts,
physical roughness ranges, dielectric/conductor separation, shared ownership
and correctly decoded linear skin reflectance.

Source-topology checks independently discover five connected charts in each
of Mira, Jon, Tessa and Owen's heads and verify all **35,292** source triangles: exact corner mapping,
barycentric coherence, signed area, tangent directions, 32px cell margins and
**2.88322×** external-head linear texel density. The measured central facial
patch grows from 87 × 104 / 88 × 106 texels to approximately **251 × 301 /
252 × 306** for Mira / Jon; see the audit for reproducible selection planes.

Cylindrical seam checks exercise all retained and fractional LODs. Face pigment
tests sample actual triangle UVs selected by XYZ. Verified lip/chin redness is
1.09397/1.00497, nose/chin roughness 0.42044/0.46373 and maximum sampled source-seam
linear tint difference 0.01536. These assertions reject cylindrical face mapping
or a conspicuous chart pigment seam.

Hostile-neighbor fixtures replace unrelated atlas pixels, rebuild mips and
verify unchanged production-sampler results at region edges/corners/interiors.
Another **266,120** actual anatomical triangle corner/edge/interior probes check
all three maps at 14 LODs against hostile pixels outside the owning Face cell.
Unsafe complete-mip controls must detect contamination. Footprint-driven checks
also retain rubber/metal separation at extreme minification.

A standalone CPU sampler build needs no SDL or graphics context:

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic \
  -Iengine/include -Iengine/third_party/stb \
  tests/character_surface_tests.cpp engine/src/character_surface.cpp \
  engine/src/material_sampling.cpp engine/src/texture.cpp \
  engine/src/math.cpp engine/src/log.cpp \
  -o /tmp/fury_character_surface_tests
/tmp/fury_character_surface_tests
```

`--dump /tmp/fury-character-atlas` exports production base-color, normal and ORM
PPM images. The engine-linked test accepts `--gl`; an unavailable actual OpenGL
3.3 context returns skip code 77, never a substituted CPU renderer. Example:

```sh
SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=1 \
  MESA_SHADER_CACHE_DIR=/tmp/fury-character-gl-cache \
  ./build/tests/fury_character_surface_tests --gl
```

Subpixel geometry forces severe minification at a known pixel. Hostile material
neighbors must leave actual rendered color/ORM lighting unchanged within one
byte; unsafe full mip chains must visibly contaminate the result. The test
queries all three uploaded maps to confirm maximum level six. Separate quads
use actual canonical bounds of every anatomical chart and exercise both color
and ORM isolation against hostile neighboring charts, with unsafe positive
controls. Actual lighting tests verify eye/skin/cloth roughness ordering and the
metallic channel's effect on hardware.

These checks complement posed-character framebuffer review. Silhouette,
geometry, UV placement and actual screen-space resolution still govern the
perceived result; increasing atlas density does not by itself add geometry.

Verified on 2026-10-06 with the private Release build: CPU tests and actual
offscreen OpenGL 3.3 tests passed on llvmpipe (LLVM 19.1.7). Protected/unsafe
Face-chart green values were main 184/121, mouth 183/61, neck 185/62, left
socket 184/108 and right socket 184/121. Corresponding protected/unsafe ORM
lighting responses were 47/30, 47/14, 46/14, 46/27 and 47/30. Eye/skin/jacket
highlights remained 54/47/35; hardware 142 versus 152 with metallic disabled.
Detailed local logs are `out/character-study/face-texel-audit/cpu-tests.txt` and
`out/character-study/face-texel-audit/gl-tests.txt`.

## HairStrands verification

The mask has 18 long connected strands and 39.53% interior coverage at cutoff .4;
17 components have measurable curvature. Longitudinal alpha variation is only
0.0681 of across-strand variation. Tests reject disconnected islands and enclosed
pinholes, require transparent margins/tips/gutters, preserve meaningful cutoff
coverage through every retained mip, and prove alpha isolation with hostile
neighbor pixels plus an unsafe-full-mip positive control.

All ten preceding material tiles, including Face, are unchanged byte-for-byte:
29,358,336 bytes across three maps and seven levels compare equal, SHA-256
`a6f6555472fa89ec0f728822715808da31fa2f38a03787d24ed4c45cd3eae656`.
The shared atlas still retains exactly 37,746,432 pixel bytes. Verification data
and the production-mask preview are under `out/character-study/hair-strands/`.

Actual-renderer fixtures draw red hair over blue rear geometry and test solid
fibers, transparent margins/tips/interior gaps, both submission orders, and the
original opaque Hair tile under the same .4 cutoff. Disabling the cutoff must
fill those same pixels. A complete card produces 1576 versus 3136 pixels in the
software rasterizer, 1553 versus 3141 in the CPU ray tracer and 1545 versus
3136 in actual offscreen OpenGL on llvmpipe. All CPU and GL suites passed.
The GL fixture also passed material-isolation positive controls: protected
green 184 versus unsafe-full-chain 76 for HairStrands. The tile-wide mean alpha
remains .21963 at level zero and .22132 at level six; every retained level keeps
meaningful coverage at cutoff .4 without an added sampler or a special mip
coverage remapping pass.

## HairCrop verification

The preceding eleven material tiles were independently generated from a private
pre-edit source snapshot, then compared to the new generator in local tile order
including every gutter, all three maps and all seven levels. All **30,406,848
bytes** compare equal, SHA-256
`7b5dcbdf1b66aa7597b661ed6e3c61e8983239ea20c11042181cdd3fd1aa9410`.
Retained atlas pixel storage remains **37,746,432 bytes**.

The new tile's sampled mean roughness is **0.641185** and mean linear tint
multiplier **0.936087**. Bright fiber interiors cover **17.21%** of the content,
reach every sampled column and have at most **13px** of continuous longitudinal
extent; the entire cap remains opaque. Across-fiber color variation is **4.95×**
longitudinal variation. Mean absolute tangent-normal XY deviation is **0.01269**.
Tests check these against bounded ranges to reject flat maps, long bands,
unstructured noise, missing fibers and exaggerated normal relief.

All three maps pass hostile-neighbor isolation at tile corners, edges and
interiors through fractional and extreme LODs. Deliberately unbounded mip chains
fail the positive controls. The existing seam tests now include HairCrop, and
all three actual renderers preserve its opaque cap under the shared .4 alpha
cutoff. The updated suite retains the three anatomical heads' source-UV checks.

On 2026-10-06, the CPU sampler, actual software rasterizer, CPU ray tracer and
offscreen OpenGL 3.3 suite on llvmpipe all passed. With normal relief disabled
and black diffuse albedo to isolate specular response, the same center-pixel
highlight changed from original Hair to HairCrop as follows: software **98→43**,
CPU ray **123→27**, GL **49→41**. These establish the new tile's roughness effect
within each renderer; they do not claim identical BRDF implementations. The GL
severe-minification crop probe stayed at green **183**, versus **75** with an
intentionally unsafe full mip chain.


## HairShortStrands verification

Before editing the generator, all twelve preceding tiles were generated from a
private source/header snapshot. The new generator has **zero changed bytes**
across their **31,455,360** local RGBA bytes: all three maps, all seven retained
levels and every gutter. Both byte dumps have SHA-256
`ae8e28231d884c22e7dcac61131671ea3038a2fc7168fa7c33ed431a79855a41`.
The regression suite also retains one byte digest per preceding tile, including
HairStrands and HairCrop. Atlas pixel storage remains **37,746,432 bytes**.

The short-card tile has mean sampled roughness **0.649604**, linear tint
**0.935702**, and mean absolute normal X deviation **0.079320** at full interior
resolution. Its alpha exactly matches HairStrands at every retained mip:
**39.53%** interior coverage, **18** connected fibers, **17** measurably curved
components, no enclosed pinholes, transparent margins and gutters. Every mip
retains coverage at the production .4 cutoff. All three maps pass hostile
neighbor tests through fractional and extreme LODs; deliberately unsafe full
mips fail the isolation controls.

On 2026-10-06, actual software raster, CPU ray and offscreen OpenGL 3.3 tests on
llvmpipe passed. With black diffuse albedo and normal relief disabled to isolate
the material roughness, the long-card → short-card center highlight is
**100→41** in software, **126→24** in CPU ray and **49→41** in OpenGL. The
corresponding HairCrop highlights are **43**, **27** and **41**. These compare
within each renderer, without implying identical renderer BRDFs.

The mask produces exactly the same red-card/blue-background coverage as the
long-card tile in all three renderers: software **1576/3136**, CPU ray
**1553/3141**, GL **1545/3136** cutout/opaque-control pixels. Solid fibers,
transparent gaps, roots/tips, both submission orders and the opaque cap are
checked under the shared cutoff. The severe-minification GL short-card probe
retains green **184**; its intentionally unsafe full mip chain falls to **76**.

The shared source-UV fixture includes Owen's and Lia's original generated UV streams.
All **43,996** source triangles across Mira, Jon, Tessa, Owen and Lia pass the five
chart mappings, tangent/area invariants and all retained mip isolation checks.
