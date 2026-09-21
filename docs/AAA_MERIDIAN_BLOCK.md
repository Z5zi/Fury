# AAA Meridian Block — ChatGPT Top 10 Runtime Fixes

**Branding:** Harbor Metro / HMPD / Meridian Mutual only — no Rockstar/GTA IP.

**Goal:** One controlled Harbor Metro street block that ChatGPT can re-judge against the
modern AAA visual bar (prior runtime verdict: 2.1/10 NO-SHIP).

## Capture

```bash
# From build tree (assets staged beside binary):
cd build/apps/vaultline
SDL_VIDEODRIVER=dummy ./vaultline --soft --aaa-block-capture
# Stills + CAPTURE_LOG.md → artifacts/aaa_meridian_block/ (cwd or repo-relative)
```

| File | Subject |
|------|---------|
| `01_lobby.ppm` | Meridian Mutual entrance + lobby glimpse |
| `02_street.ppm` | Intersection road→curb→sidewalk |
| `03_cruiser.ppm` | Hero HMPD cruiser v12b |
| `04_peds.ppm` | Five improved pedestrians |
| `05_night_or_alt.ppm` | Night/alt lighting hierarchy |
| `CAPTURE_LOG.md` | Camera poses + notes |

## Top 10 → what changed

| # | Item | Change |
|---|------|--------|
| 1 | Camera / geometry | `Camera::near_plane` **0.22**; capture rejects camera-in-mesh via AABB nudge; soft rasterizer rejects extreme near-plane blow-up triangles (`clip.w` + oversized screen AABB). |
| 2 | Kill debug placeholders | Meridian/kit AABB hides parked-car proxies, neon, ExtractionPad, WinZ/WinX strips, billboards, district signs, HarborWater, overlapping `Bldg0–8`, procedural bank exterior + StreetGrid/Sidewalk under the block. |
| 3 | PBR-ish materials | Soft path differentiates **asphalt / concrete / brick / glass / painted metal / rubber** (albedo + roughness + metallic). New `TextureSlot::Rubber`. |
| 4 | Street surface | AAA block rebuilds **road → curb → sidewalk**, lane marks, crosswalk, manholes, Meridian plaza in kit-front street space. |
| 5 | NPCs | Five Meridian-block pedestrians (`AaaPedA–E`) via `make_humanoid` (torso/head/limbs/hands/feet — not single cubes). |
| 6 | Hero HMPD cruiser | Loads authored `harbor_metro/hmpd_cruiser_v12b.obj` (soft LOD fallback) with painted-metal material, rubber tire accents, contact blob. |
| 7 | Lighting hierarchy | Day: sun key + neutral ambient + point fills; fog desaturated for capture. Night shot: low sun + emissive point lights. |
| 8 | Prop density | Bollards, benches, barriers, street ATMs (+screens), crates; Meridian Mutual signage. |
| 9 | Grounding | Soft **contact_shadow_strength**; dark contact blobs under cruiser/props; stronger AO on capture. |
| 10 | Mild post | Soft tonemap **exposure clamp** + capture exposure **0.88** so lobby isn’t blown-out white. |

## Authored meshes used

- `hm_bank_annex_v10.obj` (+ `_soft.obj` fallback) — Meridian Mutual exterior
- `hm_storefront_v10.obj` — west neighbor
- `hm_midrise_v10.obj` — mid block neighbor
- `hmpd_cruiser_v12b.obj` — hero HMPD cruiser
- Soft LODs are Harbor Metro derived (subsampled) for optional soft-path use

Kit meshes are placed at **native Blender world positions** (storefront / midrise / bank annex layout).

## Remaining gaps (next ChatGPT judge cycle)

- Soft path does not ingest MTL multi-materials — cruiser/buildings render as single albedo (silhouette present; glass/paint/rubber not yet per-submesh).
- Humanoids remain box-constructed low-poly — need authored ped GLB/OBJ + LODs + idle/walk sets.
- Contact shadows are AO/blob approximations — no directional soft shadow maps on soft path.
- Some clear-color / fog leakage can still read as cool ground tint in wide shots.
- Lobby interior still mostly procedural furniture (layout salvageable; not AAA set dressing).
- No temporal AA / SSR / wet-road reflections on soft path.
- Full-city replication of this block’s standards not done (benchmark-first by design).
