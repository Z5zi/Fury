# AAA Meridian Block — ChatGPT Soft-Capture Benchmark

**Branding:** Harbor Metro / HMPD / Meridian Mutual only — no Rockstar/GTA IP.

**Goal:** One controlled Harbor Metro street block that ChatGPT can re-judge against the
modern AAA visual bar.

| Cycle | Score | Verdict |
|-------|-------|---------|
| 1 | 4.0/10 | Broken / uncontrolled prototype |
| 2 | 5.0/10 | Controlled, materially improved prototype — **NO-SHIP** |
| 3 | 5.8/10 | Authored prototype — **NO-SHIP** |
| 4 | 5.5/10 | More content exposed flat materials / prototype ped quality / lighting / **cruiser sparkle** — **NO-SHIP** |
| 5 | 5.9/10 | Recovered C4 regression; AAA humans / reflections / night still fail CLEAR — **NO-SHIP** |
| 6 | 6.2/10 | Blender AAA peds + reflections present — **NO-SHIP** (humans/reflections still fail CLEAR) |
| 7 | *(upload-blocked; Cycle-6 P0s still valid)* | Soft peds 5.3 / Blender 6.4 / reflections FAIL as defining / artifacts mostly PASS |
| 8 | *(pending ChatGPT)* | Target **>8.0/10** — soft face LODs + Blender photoreal-adjacent, planar-wet DEFINING reflections, contact sheet |

## Capture

```bash
cd build/apps/vaultline
SDL_VIDEODRIVER=dummy ./vaultline --soft --aaa-block-capture
# Stills + CAPTURE_LOG.md → artifacts/aaa_meridian_block/ (1280×720 Cycle-8)
```

| File | Subject |
|------|---------|
| `01_lobby.ppm` | Meridian Mutual entrance + denser authored lobby |
| `02_street.ppm` | Intersection + wet asphalt reflections + skyline ring |
| `03_cruiser.ppm` | Hero HMPD cruiser (DEFINING clearcoat / glass / wet env reflect) |
| `04_peds.ppm` | Five Cycle-8 Blender photoreal-adjacent + soft face LODs (close crop) |
| `05_night_or_alt.ppm` | Night DEFINE: elongated lamp→wet→cruiser hood mirror→glass→façade→ped |
| `CAPTURE_LOG.md` | Camera poses + Cycle-8 notes |
| `sheet_cycle8.jpg` | **Single contact sheet** for low-upload ChatGPT rejudge |
| `blender_peds/` | **Blender Cycles beauty stills** (face + full) — facial fidelity proof when soft crop is limited. Label clearly as Blender, not soft-path. |
| `*_crop_*.png` | Soft-path crops for P0 pixel proof |

## Cycle-8 → ChatGPT ordered priorities

| # | Priority | Cycle-8 change |
|---|----------|----------------|
| 1 | **Five humans + soft face LODs** | Same Rae/Dane/Suki/Noah/Ivy — Blender Cycle-8 cornea specular+catchlight, thicker eyelids, lip volume, ear helix/concha, denser hair. Soft face LOD billboards (sclera/iris/pupil/lip/ear) so soft `04_peds_crop_faces` stops reading as toys. |
| 2 | **Reflections DEFINING (planar-wet)** | Elongated lamp pools + planar façade helper + stronger SSR; cruiser hood mirrors buildings. Caps wet 0.99 / clearcoat 0.98 / glass 0.96. Must be FIRST noticed on `02_street` / `03_cruiser` / `05_night`. |
| 3 | **Renderer correctness** | Keep sparkle-safe; no alias/flicker/z-fight/emissive-clip regressions. Capture **1280×720**. |
| 4 | **Expensive night via interaction** | lamp→elongated wet pool→cruiser hood mirror→glass→façade→ped rim→haze. |
| 5 | **Asphalt wet/used at capture distance** | Hero wet mirrors + elongated lamp streaks; wetness~1.0 under hero cams. |
| 6 | **Contact sheet** | `sheet_cycle8.jpg` for low-upload ChatGPT rejudge. |

## Authored meshes used

- `hm_bank_annex_v10.obj` (+ `_soft.obj` + `.mtl`) — Meridian Mutual exterior
- `hm_storefront_v10.obj` — west neighbor
- `hm_midrise_v10.obj` — mid block neighbor
- `hmpd_cruiser_v12b.obj` — hero HMPD cruiser
- `peds/hm_ped_{rae,dane,suki,noah,ivy}.obj` — Cycle-8 Blender photoreal-adjacent characters

Blender authoring: `/workspace/vaultline-blender/scripts/build_harbor_peds_v8.py`

## Remaining gaps (honest)

- Soft path is still software raster (no TAA); Cycle-8 soft face LODs + Blender beauty stills close the gap, but film AAA facial microdetail remains out of reach.
- Soft env is structured cubemap + planar-wet SSR helper (not captured realtime cubemap / full ray-SSR).
- Pedestrians are posed static meshes (authored weight-shift / converse / walk / lean) — no runtime skeletal clip playback yet.
- Background is a controlled ring + fog walls + reflection cards, not a full district.
- Full-city replication of this block's standards not done (benchmark-first by design).
