# Shipped-asset and runtime-integration audit

## Scope and reproducibility

This audits files actually present in the checkout, not an art-plan score or a Blender beauty render. Run:

```sh
python3 scripts/audit-assets.py
cmake --build build --target fury_gltf_tests fury_asset_pipeline_tests
ctest --test-dir build -R 'gltf_import|asset_pipeline' --output-on-failure
```

The dependency-free Python audit writes [the complete file-by-file JSON inventory](validation/asset-audit.json). It contains SHA-256, byte size, source references, provenance evidence/gaps, and format-specific checks for **all 153 files under assets/**, plus **14 visual-evidence images** outside that directory. No assets were downloaded, replaced, or relicensed in this work.

- 44 GLBs: 1,114,301 source triangles across all exports, including combined kits, individual exports and LODs
- 42 OBJs: 890,358 triangulated source faces, with 39 MTL companions
- 10 runtime texture files: five PNGs and their five PPM fallbacks, **all 64 × 64**
- 12 WAVs: PCM16, mono, 22,050 Hz; four 8-second zone beds and eight shorter cues
- Six asset documentation/license/manifest files
- No file-backed sprites, skeletal animation assets, or Blender source files are shipped; the HUD uses the built-in bitmap font and geometry

These triangle totals are inventory totals, **not** unique scene geometry or a measured visible-triangle count. Kits duplicate individually exported objects; OBJ and GLB are alternative representations.

## What was checked

All OBJ positions/normals/UVs and every GLB accessor were decoded and checked for finite values. Triangle indices, local bounds, zero/near-zero area, and source-index edge incidence were inspected. PNG chunks have valid checksums and complete decompressed raster sizes; PPM headers and payloads are checked. WAV format, duration, peak/RMS, endpoint difference, and clipping are reported. Optional-download files are separately checked against the existing lock if present.

All 44 shipped GLBs also pass the **production C++ importer** in the asset regression test. Every primary/LOD/OBJ path in the Harbor registry resolves. The follow-on [production CPU visual review](ASSET_VISUAL_REVIEW.md) also captures all 44 GLBs and all 42 OBJs from opposing angles, plus six gameplay-filtered civilian LOD closeups (178 successful renders).

The audit reports boundary and nonmanifold **index** edges without welding coincident positions. Exported normal/UV seams and intentionally open decals can look non-watertight under that measure. These counts are diagnostics, not automatic defect classifications. Source triangles with squared cross-product area at or below 1e-20 are flagged: Python double precision finds 5,013 in GLBs; the runtime float32 path removes 5,017, with four threshold-edge differences. It retains 1,109,284 triangles. Source files are untouched. OBJ fallbacks still contain 3,863 flagged triangles.

## Verified runtime improvements

1. **Authored colors survive merged traffic/props.** Each primitive's linear base-color factor is baked into its vertices and the merged material's albedo is neutral. Previously, a whole multi-material vehicle inherited the first material color. The one-material merge still has one roughness/metallic/transmission value; hero assets should use material groups for full per-surface state.
2. **Correct transformed shading.** Harbor geometry baking uses inverse-transpose normals for nonuniform scale and reverses winding for mirrored transforms. This applies to merged, primitive, and material-group paths.
3. **Material groups preserve all represented properties.** Grouping compares all material fields exactly, including opacity, alpha mode/cutoff, transmission, IOR and double-sided state. Groups have deterministic source order. Factor-only materials share one empty imported-material identity per asset, preserving independent glTF emission while permitting batching of identical factors.
4. **Degenerate glTF triangles are removed before rendering/BVH construction.** Empty primitives are excluded, nonfinite material values/transformed positions and area overflow fail explicitly, and failed imports preserve the previous asset. OPAQUE material alpha factors are ignored as required by glTF.
5. **Safe, consistent texture loads.** RGB and RGBA decode paths now share 8192-pixel dimension and 256 MiB input limits. P3 comments are supported between header/sample values; P6 non-255 maximum values are correctly rescaled; invalid ranges, truncation and nonfinite sample coordinates fail safely.
6. **Asset resolution and lifetime fixes.** The bank-annex descriptor now points to shipped v10 files. The merged cache checks current scene ownership and geometry identity before reusing scene-owned pointers, and does not cache missing-asset fallbacks.
7. **One real storefront enters gameplay.** `replace_storefront_shell` uses the previously unused `hm_storefront_v10.glb` for the non-enterable `Bldg3` scenery footprint at (-20, 22). It groups authored masonry/window/trim materials, keeps the old entity as collision-only, and fits X/Z within its existing 11 × 8 footprint. The existing 8-meter structural roof height is retained, with exported rooftop detail above it. Exported remote bins, meters, sidewalk patches and other off-footprint parts are excluded using the named structural shell, rather than fitting the misleading 35-meter whole-file bounding box. Failure leaves the original box intact. No mission door, portal, collider or interaction target is added or changed.

The storefront helper has real-asset regression coverage for unchanged collider/transform, no duplicate solids, footprint containment, idempotence, mission-shell exclusion and missing-file fallback. Synthetic GLB tests separately cover merged red/green material colors, opaque/blended grouping, inverse-transpose normals, mirrored winding and cross-scene cache lifetime. Texture tests cover P3/P6 behavior, oversized and invalid data, alpha semantics and transactional failures.

## Runtime use versus authoring evidence

- Meridian modular bank heroes and the `KIT_*` subset of the interior kit have explicit gameplay loaders. Helper geometry is filtered to avoid stacking whole kit heroes on separately placed modules
- Getaway/HMPD hero vehicles use per-material groups; traffic, numerous small bank props and street props use the compact merged path
- `hm_storefront_v10.glb` is now explicitly used by the storefront replacement helper. Standalone midrise and the combined buildings kit remain unplaced; the annex filename fix does not itself instantiate the annex
- OBJ is a geometry-only fallback. The current OBJ loader does not consume the shipped MTL files, so it cannot preserve their authored material state
- The five texture slots are resolved from PNG, then PPM, then procedural fallback. The other material slots are generated procedurally
- Audio filenames have cue mappings in `engine/src/audio.cpp`; signal inspection is not a claim that every cue was auditioned or that a physical output device was tested
- `artifacts/meridian_cinematics/` contains six 320 × 179 images; the seven heist-capture PPMs and `docs/images/coastal-dxr.png` are evidence files, not runtime textures
- The asset README references external Blender output directories, but those sources are absent here. Plan-pack “AAA” scores are subjective historical text, not runtime validation
- Poly Haven pier/tree packages exist only as a pinned optional-download manifest in this checkout. All 19 locked files were absent at audit time. No high-detail coastal assets are assumed present or rendered by these tests

## Remaining fidelity and performance gaps

- **No shipped GLB has an image texture.** All 13,650 source primitives have normals; 13,642 have UVs; none have tangents. Detailed geometry does not provide baked paint wear, dirt, normal maps or material microstructure
- Optional **clearcoat** appears in 43 GLBs, **specular** in 23, **anisotropy** in 11 and **sheen** in five. These extensions are not represented by Fury's current material type/importer. They must be added across import and rendering before claiming Blender material parity
- Independent texture samplers, AO textures, per-map UV sets/transforms, skinned animation and morph targets need further work. Unsupported required extensions fail rather than claiming support; optional unsupported material features currently use the core metallic/roughness fallback
- The 64 × 64 procedural textures are suitable placeholders, not close-up photoreal surface sources. A future authored material pass should ship matching albedo/normal/roughness sets with explicit UV scale, provenance and CPU texture-memory budgets
- Traffic still uses one scalar PBR material per merged vehicle. A multi-material mesh/submesh representation would preserve glass/paint/rubber roughness without multiplying scene entities
- Decimated LODs retain their broad silhouettes in the bounded overview visual review; near-distance transition and fine-detail review remains necessary. For example, sedan LOD1 is still 64,164 source triangles; hatch LOD1 is 41,034. The HMPD/buildings LOD registry entries do not prove those LODs are selected by the current grouped gameplay path
- The original backdrop remains mostly procedural boxes. The single storefront replacement is deliberately bounded; instantiating the entire combined layout would place unrelated buildings/street furniture at authored staging coordinates and can block existing routes
- Audio signal levels vary substantially: e.g. zone beds are around -12 dBFS RMS, while footstep/printer/radio-blip cues are around -31 to -36 dBFS RMS. Zero clipping and zero endpoint amplitude difference do not prove seamless spectral looping, realistic sound design, or a balanced in-game mix

## Provenance and license evidence

Original textures are described as procedural in `assets/textures/README.md`; buildings/equipment/liveries are described as original in the Harbor README; Meridian WAVs are described as authored in `docs/MERIDIAN_AUDIO.md`. The repository is MIT licensed. These are repository provenance claims, not independent reconstruction of each asset's creation history.

The vehicle-base text identifies **MrJaneLAB Mid-size Sedan CC0**, but does not include the original source URL, original download, exact license text/hash, or a precise derivative lineage for sedan/hatch/van/cruiser. Preserve the existing attribution and resolve that provenance gap before widening distribution claims. Do not silently replace the attribution with an assumed license.

The optional Poly Haven manifest does record source URLs, CC0-1.0 claims, per-file sizes and SHA-256. Because those downloads were absent, neither their geometry nor textures were audited in this pass. No external game IP or reference-image pixels were used.

## Per-GLB inventory

The JSON includes every file, including OBJ/MTL and each audio/image asset. This compact table summarizes all GLBs. “Degenerate” uses the Python source-geometry threshold described above; counts include alternate/kit/LOD exports.

| GLB | Source triangles | Degenerate | Materials | Runtime/source-use note |
|---|---:|---:|---:|---|
| `hm_bank_access_panel_v2.glb` | 312 | 0 | 5 | Gameplay loader/dispatch evidence |
| `hm_bank_alarm_panel_v2.glb` | 324 | 0 | 6 | Gameplay loader/dispatch evidence |
| `hm_bank_annex_v10.glb` | 49,516 | 203 | 38 | Registry only; unplaced |
| `hm_bank_badge_scanner_v2.glb` | 132 | 0 | 3 | Gameplay loader/dispatch evidence |
| `hm_bank_camera_dome_v2.glb` | 772 | 0 | 5 | Gameplay loader/dispatch evidence |
| `hm_bank_card_reader_v2.glb` | 144 | 0 | 4 | Gameplay loader/dispatch evidence |
| `hm_bank_deposit_boxes_v2.glb` | 7,500 | 0 | 11 | Gameplay loader/dispatch evidence |
| `hm_bank_interior_kit_v2.glb` | 58,656 | 0 | 51 | Gameplay loader/dispatch evidence |
| `hm_bank_lobby_chair_v2.glb` | 416 | 0 | 2 | Gameplay loader/dispatch evidence |
| `hm_bank_motion_sensor_v2.glb` | 92 | 0 | 3 | Gameplay loader/dispatch evidence |
| `hm_bank_queue_poles_v2.glb` | 1,980 | 0 | 3 | Gameplay loader/dispatch evidence |
| `hm_bank_security_cabinet_v2.glb` | 1,028 | 0 | 12 | Gameplay loader/dispatch evidence |
| `hm_bank_security_desk_v2.glb` | 1,604 | 0 | 18 | Gameplay loader/dispatch evidence |
| `hm_bank_stanchion_v2.glb` | 504 | 0 | 3 | Gameplay loader/dispatch evidence |
| `hm_bank_teller_counter_v2.glb` | 2,208 | 0 | 17 | Gameplay loader/dispatch evidence |
| `hm_bank_trim_kit_v2.glb` | 276 | 0 | 6 | Gameplay loader/dispatch evidence |
| `hm_bank_vault_door_v2.glb` | 37,428 | 0 | 15 | Gameplay loader/dispatch evidence |
| `hm_buildings_kit_v10.glb` | 121,044 | 771 | 84 | Registry only; unplaced |
| `hm_buildings_kit_v10_lod1.glb` | 23,159 | 160 | 84 | Registry only; unplaced |
| `hm_civ_hatch_v3.glb` | 65,974 | 316 | 38 | Gameplay loader/dispatch evidence |
| `hm_civ_hatch_v3_lod1.glb` | 41,034 | 498 | 38 | Registered LOD; selection path varies |
| `hm_civ_sedan_v3.glb` | 106,920 | 188 | 38 | Gameplay loader/dispatch evidence |
| `hm_civ_sedan_v3_lod1.glb` | 64,164 | 471 | 38 | Registered LOD; selection path varies |
| `hm_civ_van_v3.glb` | 89,676 | 168 | 37 | Gameplay loader/dispatch evidence |
| `hm_civ_van_v3_lod1.glb` | 51,494 | 498 | 37 | Registered LOD; selection path varies |
| `hm_midrise_v10.glb` | 33,264 | 240 | 53 | No direct gameplay load |
| `hm_prop_atm_v2.glb` | 18,940 | 0 | 25 | Gameplay loader/dispatch evidence |
| `hm_prop_barrier_set_v2.glb` | 15,416 | 0 | 21 | Gameplay loader/dispatch evidence |
| `hm_prop_bench_v2.glb` | 6,248 | 0 | 11 | Gameplay loader/dispatch evidence |
| `hm_prop_bike_rack_v2.glb` | 1,524 | 0 | 8 | Gameplay loader/dispatch evidence |
| `hm_prop_bollard_v2.glb` | 8,416 | 0 | 12 | Gameplay loader/dispatch evidence |
| `hm_prop_drain_grate_v2.glb` | 200 | 0 | 6 | Gameplay loader/dispatch evidence |
| `hm_prop_hydrant_v2.glb` | 3,276 | 0 | 12 | Gameplay loader/dispatch evidence |
| `hm_prop_manhole_v2.glb` | 588 | 0 | 9 | Gameplay loader/dispatch evidence |
| `hm_prop_newsbox_v2.glb` | 6,420 | 0 | 17 | Gameplay loader/dispatch evidence |
| `hm_prop_parking_meter_v2.glb` | 6,648 | 0 | 20 | Gameplay loader/dispatch evidence |
| `hm_prop_planter_v2.glb` | 5,060 | 0 | 13 | Gameplay loader/dispatch evidence |
| `hm_prop_sign_post_v2.glb` | 1,388 | 0 | 16 | Gameplay loader/dispatch evidence |
| `hm_prop_trash_bin_v2.glb` | 3,932 | 0 | 14 | Gameplay loader/dispatch evidence |
| `hm_prop_utility_cabinet_v2.glb` | 9,364 | 0 | 17 | Gameplay loader/dispatch evidence |
| `hm_storefront_v10.glb` | 33,196 | 328 | 58 | Bldg3 visual replacement |
| `hm_street_props_kit_v2.glb` | 97,624 | 0 | 78 | Gameplay loader/dispatch evidence |
| `hmpd_cruiser_v12b.glb` | 92,290 | 980 | 91 | Gameplay loader/dispatch evidence |
| `hmpd_cruiser_v12b_lod1.glb` | 44,150 | 192 | 91 | Registered LOD; selection path varies |
