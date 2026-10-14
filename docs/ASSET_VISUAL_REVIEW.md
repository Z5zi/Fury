# Production CPU asset visual review

## Result and scope

**178 of 178 bounded production-renderer captures succeeded**: all 44 GLBs and all 42 OBJs at two opposing camera angles, plus six close views of civilian vehicle LOD0/LOD1 pairs after applying the exact gameplay helper filter. All nine labeled contact sheets were visually inspected. The ten PNG/PPM texture files were also inspected in a separate contact sheet; all five PNG/PPM pairs have identical RGB pixels.

This is an actual framebuffer review using `build/apps/renderlab/fury_renderlab --backend cpu-ray --asset-only`, not a Blender export, generated illustration, wireframe approximation, or claim of photorealistic quality. Each capture uses 320 × 180 output, a neutral white sun/gray environment, a fitted perspective camera, two bounces, and 2 samples per pixel across four accumulated frames (8 spp). Cameras are yaw 0° and 180°; the six filtered closeups use yaw 0°.

Every per-frame report confirms CPU ray tracing, exactly two CPU workers, nonzero ray counts and CPU timings, four accumulated frames, nonzero geometry and zero validation errors. Final-frame ray counts range from 117,321 to 162,653. These small, concurrently scheduled captures are visual checks, **not gameplay frame-rate benchmarks**.

The preview executable records source fingerprint `2fd2ce0febd38843370d90c1f08636891968144ee15dd13a8c730b1094a9a23d`. The three index files also retain executable SHA-256, original asset SHA-256, exact command lines, screenshot SHA-256, logs and measured results. Later application error-handling changes do not alter these recorded captures; they remain evidence for the exact recorded executable.

## Reproduce

Requires Python 3, Pillow and the built Renderlab executable:

```sh
python3 scripts/preview-assets.py --output out/cpu-validation/asset-previews --jobs 2
python3 scripts/preview-assets.py --output out/cpu-validation/asset-previews-reverse --yaw 180 --jobs 2
python3 scripts/preview-assets.py --output out/cpu-validation/civilian-close-previews --civilian-close --jobs 2
```

The script permits only one or two concurrent renderer processes, sets `FURY_CPU_THREADS=2`, uses a 60-second timeout per asset and a 4 GiB virtual-memory ceiling per process on POSIX. The final batches took approximately 20.7, 16.3 and 1.7 seconds in this environment. Original source assets remain unchanged.

## Evidence paths

Generated evidence stays under the ignored `out/` tree and can be regenerated with the commands above:

| Evidence directory | Contents |
|---|---|
| `out/cpu-validation/asset-previews/` | All 86 raw files, yaw 0°; `glb-contact-sheet-01.png`, `glb-contact-sheet-02.png`, `obj-contact-sheet-01.png`, `obj-contact-sheet-02.png` |
| `out/cpu-validation/asset-previews-reverse/` | All 86 raw files, yaw 180°; the same four sheet names |
| `out/cpu-validation/civilian-close-previews/` | Six explicitly labeled gameplay-filtered GLB closeups, `glb-contact-sheet-01.png`, original/wrapper hashes and `filtered-inputs/` wrappers |
| `out/cpu-validation/asset-previews/texture-contact-sheet.png` | All ten texture files, nearest-neighbor 2× pixel display |

Each directory contains `index.json`. Individual images, raw unmodified PPM framebuffers, runtime JSON reports and process logs are under `glb/` and/or `obj/`, using the asset stem as filename. `docs/validation/asset-visual-review.json` records a compact durable manifest of all 178 captures and sheet hashes.

## Visual findings

### Improvements confirmed

- All inspected assets produce nonempty recognizable geometry. Front and reverse views distinguish genuinely plain backs of access panels, readers, deposit boxes and the vault door from their detailed front faces; a single back-view thumbnail would be misleading
- glTF material separation is visible: teal chair fabric, tan bench planks, dark bike racks, green planter foliage, cyan screens, police glass/body/trim and colored lights survive import. OBJ previews are intentionally neutral gray because the production OBJ path does not import MTL materials. Their lack of color is a documented fallback limitation, not a failed glTF material test
- Independent emission survives black/dark bases: bank indicator lamps/screens and HMPD rear lights visibly emit in the final captures after the shared imported-material identity fix
- The final preview camera contains tall parking meters, signposts and bollards. An earlier preview fit clipped their tops; the fit was corrected and **all** final raw-file captures were rerun
- The final mirrored/single-sided renderer corrections remove the false cutaway/black wedge seen in an earlier motion-sensor preview. Final front/reverse sensor renders show a coherent housing and lens

### Concrete remaining asset issues

1. **Staging geometry is bundled into “individual” exports.** Civilian cars have long detached sidewalk strips; annex, storefront and midrise files have remote fragments/furniture. These force wide raw-file camera fits and would be misplaced if entire exports were instantiated blindly. The raw sheets intentionally show them. Gameplay helper filtering and the bounded storefront-shell integration handle their currently used paths; a clean source/export manifest is still preferable
2. **Surface detail remains mostly flat factors and separate geometry.** Large paint, panel, concrete and trim regions read as smooth/plastic, and signs/screens mostly read as simple rectangles. All source GLBs lack image maps, so the renderer cannot reconstruct authored grunge, wear, labels or micro-normal detail that was never shipped
3. **Some silhouette assets are still placeholders.** The loose crate is a 12-triangle box, the cone is 16 triangles, the barrel is 32 triangles, and planter foliage visibly consists of rounded primitive masses. These need a dedicated art pass for close-up realism; shader changes alone will not fix those silhouettes
4. **Low-sample convergence remains visible.** Shadow-side metal and small emissive indicators show grain at 8 spp, especially vault/security assets in the reverse view. This is a deliberately bounded overview test. It does not establish converged specular/transmission quality, close-up roughness fidelity, or freedom from every subpixel surface artifact
5. **Interior-kit overview is limited by its own roof/walls.** The two exterior fits show the kit shell and one open side; they do not substitute for the separately captured playable bank walkthrough or inspect every occluded interior component

### LOD assessment

The HMPD LOD0/LOD1 and combined-building LOD0/LOD1 sheets retain the broad body/building silhouettes at this overview scale. Fine police hardware, window strips, rims and bevels are subpixel or noisy here, so this is not a certification for close-distance LOD switching.

Civilian raw views were too small for a useful LOD judgment because of their staging sidewalks. The extra six closeups address that limitation. The preview wrappers remove **only the mesh bindings** of nodes matching the exact, case-sensitive `is_helper_prim` substrings (`ground_walk`, `ground_curb`, `Shadow`, `SaltRing`, `xmem`, `StreetWalk`), preserving all other geometry, materials, binary buffers, transforms and child relationships. Six helper nodes are excluded per vehicle. These are clearly labeled filtered previews; the original GLBs and their hashes are preserved.

| Gameplay-filtered pair | LOD0 runtime triangles | LOD1 runtime triangles | Overview observation |
|---|---:|---:|---|
| Civilian hatch | 65,586 | 40,499 | Roof, hood, windshield and wheel/body outline remain coherent |
| Civilian sedan | 106,660 | 63,657 | Long hood/trunk shape and front fascia remain coherent |
| Civilian van | 89,436 | 50,957 | Box cargo body, cab, windshield and mirror silhouette remain coherent |

No obvious missing major vehicle component or collapsed broad silhouette was found in these closeups. Remaining limitations are the low image resolution/sample count, one closeup angle, absence of texture detail and currently high LOD1 triangle counts. Full animated/gameplay transition review remains a separate task.

## Interpretation

The visual pass supports a complete shipped-model runtime/import check and catches real framing/integration problems. It does **not** turn the original assets into photorealistic content, certify every occluded face, establish Blender material parity, or validate absent optional Poly Haven downloads. See [the structural/provenance audit](ASSET_AUDIT.md) for the file inventory and unresolved license/source gaps.

## Committed review sheets

The exact runtime contact sheets are included in this PR:

- [GLB front 1](images/cpu-upgrade/front-glb-contact-sheet-01.png), [GLB front 2](images/cpu-upgrade/front-glb-contact-sheet-02.png)
- [GLB reverse 1](images/cpu-upgrade/reverse-glb-contact-sheet-01.png), [GLB reverse 2](images/cpu-upgrade/reverse-glb-contact-sheet-02.png)
- [OBJ front 1](images/cpu-upgrade/front-obj-contact-sheet-01.png), [OBJ front 2](images/cpu-upgrade/front-obj-contact-sheet-02.png)
- [OBJ reverse 1](images/cpu-upgrade/reverse-obj-contact-sheet-01.png), [OBJ reverse 2](images/cpu-upgrade/reverse-obj-contact-sheet-02.png)
- [Filtered civilian LOD closeups](images/cpu-upgrade/civilian-glb-contact-sheet-01.png)
- [All runtime texture variants](images/cpu-upgrade/front-texture-contact-sheet.png)
