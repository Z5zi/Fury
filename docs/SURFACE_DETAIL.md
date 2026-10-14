# Physically scaled runtime surface detail

Fury's six opaque legacy surface profiles have original, deterministic, tileable
512 × 512 PBR maps. They add small-scale surface structure without modifying
shipped asset binaries, object shapes, colliders, or gameplay dimensions. They
are material-detail assets, not scanned materials or a substitute for authored
geometry. The existing material and vertex colors remain artist-controlled
multipliers.

## Provenance and scope

- Authoring source: `engine/src/surface_detail.cpp`, written for this project
- License: the repository's [MIT license](../LICENSE), including generated maps
- Input imagery: none; no downloads, photographs, external texture libraries, or
  image-generation model outputs are used
- Reproducibility: fixed integer hash seeds and periodic mathematical fields; no
  global random state, clock, filesystem, or network dependence
- Runtime source identity: `fury:original-surface-detail/v1/<profile>/<size>;rotation=<0..3>;license=MIT`
- Unchanged source assets: existing PNG/PPM/GLB/OBJ files are not rewritten

The generator only supports `Asphalt`, `Concrete`, `Brick`, `Wood`, `Metal`, and
`BarrelMetal`. Unsupported slots return no maps and UV scale zero. Existing
imported image maps should always take precedence; emission, glass, water,
checker/debug materials, and transparent surfaces are outside this replacement
scope. `FURY_SURFACE_DETAIL=0` retains the existing runtime material path.

## Physical layout

| Profile | One repeated tile | Structure in that tile | Height detail |
| --- | --- | --- | --- |
| Asphalt | 2 × 2 m | Irregular aggregate with ~20 mm mean centre spacing, varying exposed mineral size, binder variation and sparse weathered fissures | Up to ~1.8 mm aggregate relief, restrained fissure recess |
| Concrete | 2 × 2 m | Cement clouding, fine aggregate, sparse ~2–6 mm air pores and faint trowel variation | Sub-millimetre finish and up to ~1.9 mm pore recess |
| Brick | 2 × 2 m | Eight 250 mm brick pitches × 24 ~83 mm courses, staggered running bond, ~8 mm mortar core with softened/chipped edges | ~4.5 mm recessed joints, fine fired-clay variation |
| Wood | 2 × 2 m | Continuous longitudinal fibres, growth variation and occasional knots; actual modeled planks/slats define board boundaries | Sub-millimetre grain; no synthetic lateral or end joints |
| Metal | 1 × 1 m | Brushed sheet with directional lay, rolling variation and sparse fine scratches | Tens of micrometres of shallow brushing |
| BarrelMetal | 2 × 2 m | Neutral painted sheet, restrained irregular coating chips, oxide edges and exposed metal | Thin coating/chip relief, no baked barrel hoops or fake shape |

`surface_detail_uv_per_meter()` returns `0.5` for a 2 m tile and `1` for the 1 m
metal tile. At full resolution, these correspond to ~3.91 mm and ~1.95 mm per
texel. Features below that scale are represented by filtered response rather
than claimed to be resolved geometry. World-planar material UVs preserve these
sizes across differently scaled legacy meshes. The renderer owns the projection
and its matching tangent/bitangent basis; this module does not alter mesh UVs.

Brick-to-brick brightness and warm/cool variation is restrained. Albedo has no
baked directional lighting. Aggregate, mortar, pores, fibres and coating damage
share the same fields across color, height-derived normals and roughness, so the
channels describe the same physical feature.

## Channel and normal contract

All maps are tightly packed RGBA8 with opaque alpha:

| Map | Encoding | Channels |
| --- | --- | --- |
| `base_color` | sRGB | RGB albedo multiplied by existing material/vertex color; A = 255 |
| `normal` | Linear | RGB maps tangent XYZ from `[-1,1]` to `[0,1]`; A = 255 |
| `metallic_roughness` | Linear | R = 255, unused; G = roughness multiplier; B = metallic multiplier; A = 255 |

The R channel is **not** baked ambient occlusion. There is no emissive map.
Roughness and metallic values multiply the corresponding material factors. In
particular, a metallic multiplier cannot turn a zero-metallic material into
metal; exposed-chip metalness remains limited by the underlying artist factor.
Paint and oxide remain largely dielectric, and the four non-metal profiles have
zero metallic multiplier. Do not apply sRGB decoding to normals or packed maps.

The temporary height field is measured in metres. Normals use wrapped central
finite differences and the physical texel spacing:

`normalize(-height_slope_U, -height_slope_V, 1)`

Positive tangent U means increasing image column; positive bitangent V means
increasing image row/UV V. This is the generator's explicit convention, so the
renderer must construct its TBN basis consistently. A flat normal encodes close
to `(128,128,255)`. All generated normal vectors are normalized before encoding.
There is no parallax, silhouette displacement, self-occlusion or extra geometry.

## Resolution, filtering and bounded caching

The public API is in `fury/surface_detail.hpp`:

```cpp
auto maps = fury::surface_detail_textures(fury::TextureSlot::Brick, 512);
float repeats_per_metre = fury::surface_detail_uv_per_meter(fury::TextureSlot::Brick);
// Clockwise grain orientation, with normal vectors rotated coherently:
auto horizontal_wood = fury::surface_detail_textures(fury::TextureSlot::Wood, 512, 1);
```

Only 128, 256 and 512 are accepted. Other resolutions throw
`std::invalid_argument` before allocation, including when the slot is unsupported.
The optional `quarter_turns` argument accepts only 0, 1, 2, or 3; other values
also throw before allocation. All quality settings are derived from a common 512 master. Choosing a smaller
setting does not reseed, resize the physical pattern, or move material features.

Every returned set includes complete `*_mips` vectors. Their first element is
level 1 (half-size); the main image is level 0 and is not duplicated in the mip
vector. The production `build_mip_chain` path averages sRGB color in linear
light, renormalizes normal vectors, and linearly averages roughness and metallic
multipliers. The final level is 1 × 1. Renderers should select/interpolate mip
levels from the projected footprint, especially for grazing roads and distant
brickwork; nearest full-resolution sampling will alias. The maps do not encode
an anisotropic BRDF or a normal-variance roughness correction.

A clockwise quarter-turn transforms the scalar field as
`H_new(u,v) = H_old(v,1-u)` and normal XY as `(-oldY,oldX)`, leaving Z
unchanged. Encoded normal red becomes `255 - old_green` and green becomes
`old_red`. Rotation is applied coherently to each existing map and mip level;
it does not refilter or change physical scale. Four turns return exactly the
original encoded values. The wood pattern has continuous grain and no baked
board joints; orientation can follow actual plank geometry through a rotated
variant or appropriately authored mesh UVs.

The thread-safe global cache retains six profiles at **one active
(resolution, quarter-turn count) pair**. Changing either key releases the
cache's previous references; caller-held `shared_ptr`s remain valid. The exact all-profile RGBA pixel payload including
all mip levels is:

| Active resolution | Six profiles, three maps, complete mip chains |
| --- | ---: |
| 512 | 25,165,800 bytes, just under 24 MiB |
| 256 | 6,291,432 bytes, just under 6 MiB |
| 128 | 1,572,840 bytes, just under 1.5 MiB |

These figures exclude container metadata, the temporary master/height buffers,
caller-held other resolutions/orientations, and renderer/GPU copies. The default
six profiles remain just under 24 MiB. One additional caller-retained 512 wood
orientation costs 4,194,300 bytes, just under 4 MiB; six defaults plus that
orientation therefore require about 28 MiB of CPU pixel data. The global cache
bound does not bound assets deliberately retained by renderer instances.
Runtime attachment should cache returned sets by slot and orientation rather
than repeatedly request alternating resolution/rotation keys.
Generation is lazy per profile, performed once per active cache key. The focused
optimized cloud test measured approximately 0.84–1.35 s to generate all six 512 sets
including their mips; this is an observed CPU/startup measurement, not a portable
frame-time claim or a hard test threshold.

## Verification and reproducible previews

The focused test executable covers:

- Supported/unsupported slots, invalid-resolution rejection and physical scale
- Complete RGBA sizes, opaque alpha, correct packed channel semantics
- Finite, normalized positive-Z normals at every mip level
- Exact agreement with production sRGB, normal and linear mip filters
- Wrapped edge-derivative checks and repeat-sampling invariance
- Brick normal directions and correlated mortar color/roughness, concrete pores,
  aggregate roughness, painted metal/oxide/exposed-steel regions
- Byte-identical regeneration after eviction, exact master-derived lower settings
- Exact coherent 0/90/180/270-degree map/mip rotations, independently checked
  against finite differences of a rotated periodic height-gradient fixture
- Shared pointer identity, release of old resolution/rotation cache-only owners
  and concurrent misses
- Explicit map-plus-mip memory bound

Run through CTest or directly:

```sh
ctest --test-dir build -R surface_detail --output-on-failure
build/tests/fury_surface_detail_tests --dump-dir /tmp/fury-surface-detail-preview
```

The optional dump writes the actual 18 default level-zero RGB PPM channel
previews plus the three clockwise wood variant maps used by the runtime. A file browser or image viewer can inspect them without any
network tool. The albedo and normal atlases were visually inspected during
implementation; asphalt grain shapes, wood grain regularity and brick course
proportions were revised after that inspection. The final brick tile has 24
courses. A further inspection verified the seam-free wood surface and its
clockwise color/normal orientation. Keep visual runtime comparisons at a fixed camera/light/quality setting
and include a legacy-opt-out view; these data tests do not certify the appearance
or frame cost of a complete rendered scene.
