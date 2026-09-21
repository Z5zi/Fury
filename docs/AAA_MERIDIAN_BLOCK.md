# AAA Meridian Block — ChatGPT Soft-Capture Benchmark

**Branding:** Harbor Metro / HMPD / Meridian Mutual only — no Rockstar/GTA IP.

**Goal:** One controlled Harbor Metro street block that ChatGPT can re-judge against the
modern AAA visual bar.

| Cycle | Score | Verdict |
|-------|-------|---------|
| 1 | 4.0/10 | Broken / uncontrolled prototype |
| 2 | 5.0/10 | Controlled, materially improved prototype — **NO-SHIP** |
| 3 | 5.8/10 | Authored prototype — **NO-SHIP** |
| 4 | *(pending)* | Target **>8.0/10** — believable characters + expensive materials + rich night + dense Meridian + kill blue void |

## Capture

```bash
# From build tree (assets staged beside binary):
cd build/apps/vaultline
SDL_VIDEODRIVER=dummy ./vaultline --soft --aaa-block-capture
# Stills + CAPTURE_LOG.md → artifacts/aaa_meridian_block/ (cwd or repo-relative)
```

| File | Subject |
|------|---------|
| `01_lobby.ppm` | Meridian Mutual entrance + denser authored lobby |
| `02_street.ppm` | Intersection road→curb→sidewalk + skyline ring |
| `03_cruiser.ppm` | Hero HMPD cruiser v12b (clearcoat / glass / metal + probe reflect) |
| `04_peds.ppm` | Five Cycle-4 Harbor Metro characters (upgraded) |
| `05_night_or_alt.ppm` | Night hierarchy + 8 local lights + bounce + lamp spill |
| `CAPTURE_LOG.md` | Camera poses + cycle notes |

## Cycle-4 → ChatGPT Top fixes (for 8.0+)

| # | ChatGPT fix | Cycle-4 change |
|---|-------------|----------------|
| 1 | **Characters (HIGHEST)** | Same five `hm_ped_{rae,dane,suki,noah,ivy}` regenerated: proper proportions, facial topo (eyes/lips/nose/cheeks/ears), articulated hands, shoe construction, volumetric hair clumps, garment construction (lapels/hood/peplum/buttons/cuffs), skin/fabric/leather MTL diffs, distinct silhouettes, natural walk/idle/converse/lean poses + Suki↔Noah conversation grouping. Soft stills must show the upgrade. |
| 2 | **Material response / reflections** | Soft probe-atlas env reflect (sky+horizon facade band+ground — not sky/fog stub alone), wet-road SSR-lite, stronger clearcoat, glass transmission/rim, metal F0, world-space roughness variation + microdetail, richer specular. Capture owns lighting (no day_night Harbor-blue clear stomp). |
| 3 | **Kill prototype lighting look** | Keep cascades/PCF; richer hemisphere bounce, stronger contact shadows, amb-keep in shadow, 6 day / **8 night** point lights (lobby spill, street lamps, window spill, cruiser, ATM, plaza), atmospheric fog, night bloom. Night is a strength target. |
| 4 | **Finish Meridian Mutual** | Same footprint, denser dressing: teller banks + glass, queue stanchions/ropes, security desk+monitor, column caps/bases, ceiling coffers, baseboard/cove trim, layered plant foliage, entrance wear strip, brochure stand. |
| 5 | **Kill blue void** | Denser midrise ring + far skyline layer, rooftop bulkheads, window emissives, vertical fog walls, camera-wedge fillers, asphalt-matched clear, neutral urban fog. |
| + | Street physicality | Tire wear strips, drain grilles, curb grit, wet asphalt response. |

## Cycle-3 (retained)

Soft clearcoat/glass/metal/microdetail/bounce · cascaded 1024 PCF · authored Meridian exterior/lobby · 5 distinct peds (quality-upgraded in C4) · urban bg start.

## Authored meshes used

- `hm_bank_annex_v10.obj` (+ `_soft.obj` + `.mtl`) — Meridian Mutual exterior
- `hm_storefront_v10.obj` — west neighbor
- `hm_midrise_v10.obj` — mid block neighbor
- `hmpd_cruiser_v12b.obj` — hero HMPD cruiser
- `peds/hm_ped_{rae,dane,suki,noah,ivy}.obj` — Cycle-4 characters

## Remaining gaps (post Cycle-4)

- Soft probe-atlas is procedural (not captured cubemap / full SSR).
- Pedestrians are posed static meshes (no runtime skeletal clip playback).
- Background is a controlled ring + fog walls, not a full district.
- No TAA / temporal accumulation on soft path.
- Soft large-plane grazing can leave near-camera ground holes (mitigated by asphalt-matched clear + cam ground slabs).
- Full-city replication of this block’s standards not done (benchmark-first by design).
