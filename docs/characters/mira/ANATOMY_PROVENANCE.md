# Mira anatomical head: provenance and reconstruction

Mira's anatomical head/neck is derived from the MakeHuman Community's virtual
human base mesh and morph-target **graphical data**, followed by Fury's own
geometry blend and extraction. This is an authored human mesh, not a scan of a
real person and not a claim of measured anatomical accuracy. Mira's skin,
eyes, hair, clothing, materials, and character identity are authored separately.

## Source and license

The source is [MPFB2 at immutable commit
d0a32e57a7f915cb2f2b95410e2117648c7bbb7e](https://github.com/makehumancommunity/mpfb2/commit/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e).
The pinned [upstream license boundary](https://github.com/makehumancommunity/mpfb2/blob/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/LICENSE.md)
explicitly places base meshes, targets/modifiers, rigs, and JSON mesh data under
CC0 1.0 Universal, separately from the GPLv3 addon program logic.
The complete [upstream CC0 asset license](https://github.com/makehumancommunity/mpfb2/blob/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/LICENSE.ASSETS.md)
is preserved in `third_party/mpfb_assets/LICENSE.ASSETS.md`.

Fury vendors only the OBJ, four compressed target-data files, mesh-group JSON,
and unchanged license documents. No upstream program logic was copied, adapted,
installed, or executed. `scripts/build_mira_anatomy.py` is an original importer
using only Python's standard library. It needs neither Blender nor MPFB.

Every downloaded byte is pinned and checked in
[`MANIFEST.json`](../../../third_party/mpfb_assets/MANIFEST.json), including its
immutable raw GitHub URL and SHA-256. In particular:

- Base mesh: `src/mpfb/data/3dobjs/base.obj`
- Base mesh SHA-256: `8e761e6624b8f54536409135d1636da63b32486a90d4897f84e121d144f6fb4c`
- Base mesh header also explicitly records its September 2020 CC0 release

## Reviewable recipe

The exact recipe is stored in
[`mira_recipe.json`](../../../third_party/mpfb_assets/mira_recipe.json).
Each target's displacement is added to the original vertex position, weighted
as follows, including the helper vertices before landmarks are extracted:

| Upstream asset parameter | Weight |
|---|---:|
| `macrodetails/african-female-young.target.gz` | 0.45 |
| `macrodetails/asian-female-young.target.gz` | 0.35 |
| `macrodetails/caucasian-female-young.target.gz` | 0.20 |
| `head/head-oval.target.gz` | 0.15 |

These are upstream asset parameter names. They do not assign an ethnicity to
Mira, derive skin color, or encode a real person's identity. The first three
weights sum to one; the oval target is an additional shape adjustment. Existing
Fury skin color remains independent of this geometry recipe.

## Coordinates and extraction

1. Verify the SHA-256 of every vendored source against the manifest
2. Parse OBJ positions, UVs, groups, and face corners, and apply the four targets
3. Compute the morphed `body` group's full height, excluding all helper geometry
4. Apply **one uniform scale** to all axes, putting the full body's feet at
   `y=-0.5` and crown at `y=0.493`. The remaining `0.007` reserves room for hair
5. Preserve the source axes: X horizontal (anatomical left positive), Y up,
   Z face/front. Source X and Z origin remain unchanged before scaling
6. Take each named joint's center as the mean of its unique, morphed OBJ-group
   vertices. Cross-check the relevant joint/body index sets against the pinned
   JSON mesh metadata. Then discard all helper geometry from the output
7. Retain only `body` faces whose every vertex is at or above `y=0.32`. This
   leaves a neck/upper-shoulder attachment seam for integration with Fury's body
8. Split each retained source quad along its original first-to-third-vertex
   diagonal, preserving corner order, shared vertices, and UV seams
9. Remove triangles with squared cross-product length `<=1e-24` measured on the
   binary32 positions actually emitted to C++. This recipe removes **zero**

The uniform scale is `0.062177528427591035`. No independent head widening,
vertical stretch, smoothing, retopology, or decimation is applied by the importer.
Normals are deliberately left to Fury's mesh integration so shared-vertex
smoothing remains consistent with the renderer.

## Output and integration

The generated header is
[`mira_anatomy.hpp`](../../../engine/include/fury/generated/mira_anatomy.hpp),
under namespace `fury::mira_anatomy`:

- `positions`, `indices`: 4,438 shared vertices and 8,824 triangles
- `uv_coordinates`, `uv_indices`: 4,633 original UV coordinates and a per-corner
  UV index stream; seams are retained even when geometric vertices are shared
- `source_vertex_indices`, `source_uv_indices`, `triangle_source_faces`: zero-based
  indices into the pinned OBJ; face IDs count every OBJ `f` line
- `eye_centers[2]`: anatomical left, then anatomical right
- `eye_helper_half_extents[2]`: half-extents of the discarded morphed eye helper
  meshes, available as an initial eyeball fitting guide
- `mouth_center`, `head_center`, `neck_center`, `crown_joint`, `jaw_center`,
  `upper_lid_centers`, `lower_lid_centers`, `bounds_min`, `bounds_max`

Selected geometry bounds are `(-0.08921854, 0.32021176, -0.02904934)` through
`(0.08921854, 0.493, 0.09385076)`. The cutoff retains a small upper-shoulder
section; clothing and the body/neck attachment must be fitted in the character
profile. The eye centers are `(±0.01766347, 0.42766541, 0.06796587)` and eye helper
half-extents are approximately `(0.00888983, 0.00860382, 0.00917429)`.

The mouth joint `(0, 0.40801342, 0.03348415)` is **inside** the head. It is a rig
landmark, not the visible lip location. Likewise lid joints are rig landmarks;
fit cosmetics and eye coverage to the actual skin surface. Do not place a flat
lip decal or eye surface at a joint marker without checking the rendered mesh.

The intact head exceeds a 6,000-triangle budget. This deliverable intentionally
contains a single undamaged topology; both LODs may use it while CPU cost is
measured. A later lower-detail mesh should preserve lip loops, eyelids, nostrils,
ears, and the silhouette and pass image comparisons. No such reduced mesh is
claimed here.

## Reproduction and verification

From the repository root:

```sh
python3 scripts/build_mira_anatomy.py
python3 scripts/build_mira_anatomy.py --check
```

The second command verifies all pinned source hashes and compares the complete
generated header and report byte for byte without writing. The generator also
checks finite data, valid source/corner indices, unique target entries,
OBJ/JSON landmark agreement, float32 triangle area, connectedness, and edge
manifoldness. It rejects larger-than-quad source polygons rather than assuming
that an arbitrary fan triangulation is valid.

Exact output measurements and the generated-header SHA-256 are in
[`GENERATED_STATS.json`](../../../third_party/mpfb_assets/GENERATED_STATS.json).
The head has one connected component, 50 attachment-boundary edges, and zero
non-manifold edges. A standalone C++17 compile checks the header against Fury's
actual `Vec2` and `Vec3` definitions. Runtime image quality and cost are separate
character-integration checks; generation alone does not establish either.
