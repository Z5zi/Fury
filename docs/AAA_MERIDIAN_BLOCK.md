# AAA Meridian Block — ChatGPT Top 10 Runtime Fixes

**Branding:** Harbor Metro / HMPD / Meridian Mutual only — no Rockstar/GTA IP.

**Goal:** One controlled Harbor Metro street block that ChatGPT can re-judge against the
modern AAA visual bar (Cycle-1 soft verdict: **4.0/10 NO-SHIP**).

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
| `03_cruiser.ppm` | Hero HMPD cruiser v12b (multi-mat) |
| `04_peds.ppm` | Five authored multi-mat pedestrians |
| `05_night_or_alt.ppm` | Night/alt lighting hierarchy |
| `CAPTURE_LOG.md` | Camera poses + cycle notes |

## Cycle-2 → ChatGPT ordered Top fixes (for 8.0+)

| # | ChatGPT fix | Cycle-2 change |
|---|-------------|----------------|
| 1 | Runtime multi-material pipeline | Soft path `load_obj_mtl` / `spawn_obj_mtl` splits `usemtl` groups into per-submesh entities with MTL Kd/Ns/Ke → albedo/roughness/metallic/emissive + TextureSlot heuristics (paint/glass/rubber/chrome/brick/concrete/asphalt). Cruiser + bank/storefront/midrise now multi-mat. |
| 2 | 5 authored pedestrians | Original Harbor Metro ped OBJ+MTL (`hm_ped_{rae,dane,suki,noah,ivy}`) with Skin/Shirt/Pants/Hair/Shoes material splits, proportioned silhouettes (not box humanoids). Placed on Meridian block with contact grounding. |
| 3 | Real directional shadows | Soft backend directional shadow maps (filtered 3×3 PCF) on the `--aaa-block-capture` path via `begin_shadow_pass` / light VP orthographic. Characters/vehicles/buildings cast & receive. Contact blobs retained as grounding assist. |
| 4 | Cruiser full material stack | v12b soft OBJ+MTL ingested per-submesh (paint/clearcoat-ish, glass, rubber, lamps/emissive, metal, interior, decals) through the new material pipeline — no single-albedo override. |
| 5 | Street material treatment | Asphalt cracks, patches, oil stains (wetness), sidewalk tile albedo/roughness variation, curb dirt edges on top of road→curb→sidewalk hierarchy. |
| 6+ | Bounce / lobby / glass / density / post | Partial: glass MTL slot + emissive lamps; lobby still largely procedural; no TAA/SSR yet; block density unchanged this cycle (priority was 1–5). |

## Cycle-1 Top 10 (still in place)

| # | Item | Change |
|---|------|--------|
| 1 | Camera / geometry | `Camera::near_plane` **0.22**; capture rejects camera-in-mesh via AABB nudge; soft rasterizer rejects extreme near-plane blow-up triangles. |
| 2 | Kill debug placeholders | Meridian/kit AABB hides parked-car proxies, neon, ExtractionPad, WinZ/WinX strips, billboards, district signs, HarborWater, overlapping Bldg0–8, procedural bank exterior + StreetGrid/Sidewalk under the block. |
| 3 | PBR-ish materials | Soft path differentiates asphalt / concrete / brick / glass / painted metal / rubber (+ Cycle-2 per-submesh MTL). |
| 4 | Street surface | Road → curb → sidewalk, lane marks, crosswalk, manholes, Meridian plaza (+ Cycle-2 wear). |
| 5 | NPCs | Cycle-1: humanoid boxes → Cycle-2: authored multi-mat peds. |
| 6 | Hero HMPD cruiser | Authored `hmpd_cruiser_v12b` (+ Cycle-2 full MTL stack). |
| 7 | Lighting hierarchy | Day/night; Cycle-2 adds directional soft shadows. |
| 8 | Prop density | Bollards, benches, barriers, street ATMs, crates, signage. |
| 9 | Grounding | Contact blobs + Cycle-2 directional shadows. |
| 10 | Mild post | Soft tonemap exposure clamp + capture exposure 0.88. |

## Authored meshes used

- `hm_bank_annex_v10.obj` (+ `_soft.obj` + `.mtl`) — Meridian Mutual exterior
- `hm_storefront_v10.obj` — west neighbor
- `hm_midrise_v10.obj` — mid block neighbor
- `hmpd_cruiser_v12b.obj` — hero HMPD cruiser
- `peds/hm_ped_{rae,dane,suki,noah,ivy}.obj` — Cycle-2 authored pedestrians

## Remaining gaps (next judge cycle)

- Soft shadows are single-cascade orthographic PCF (no CSMs / contact-hardening).
- Pedestrians are strong silhouettes with material splits but no skeletal idle/walk clips yet.
- Lobby interior still mostly procedural furniture (not AAA set dressing).
- No TAA / SSR / wet-road reflection probes on soft path.
- Background skyline / fill beyond the block AABB still thin (blue/empty risk).
- Full-city replication of this block’s standards not done (benchmark-first by design).
