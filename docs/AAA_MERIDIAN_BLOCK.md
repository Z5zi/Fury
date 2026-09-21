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
| 8 | **5.4/10** | Soft heroes NO-SHIP — humans FAIL, reflections NOT DEFINING, **severe white clip** / flat materials |
| 9 | *(pending ChatGPT)* | Target **>8.0/10** — exposure-safe filmic, planar DEFINE midtone bands, face atlases, cruiser paint/glass rewrite |

## Capture

```bash
cd build/apps/vaultline
SDL_VIDEODRIVER=dummy ./vaultline --soft --aaa-block-capture
# Stills + CAPTURE_LOG.md → artifacts/aaa_meridian_block/ (1280×720 Cycle-9 exposure-safe)
```

| File | Subject |
|------|---------|
| `01_lobby.ppm` | Meridian Mutual entrance + denser authored lobby |
| `02_street.ppm` | Intersection + wet asphalt reflections + skyline ring |
| `03_cruiser.ppm` | Hero HMPD cruiser (DEFINING clearcoat / glass / wet env reflect) |
| `04_peds.ppm` | Five Cycle-9 Blender face atlases on dense UV billboards |
| `05_night_or_alt.ppm` | Night DEFINE: elongated lamp→wet→cruiser hood mirror→glass→façade→ped |
| `CAPTURE_LOG.md` | Camera poses + Cycle-8 notes |
| `sheet_cycle9_mini.jpg` | **≤4-tile mini sheet** for low-upload ChatGPT; keep full-res PNGs |
| `blender_peds/` | **Blender Cycles beauty stills** (face + full) — facial fidelity proof when soft crop is limited. Label clearly as Blender, not soft-path. |
| `*_crop_*.png` | Soft-path crops for P0 pixel proof |

## Cycle-9 → ChatGPT ordered priorities

| # | Priority | Cycle-8 change |
|---|----------|----------------|
| 1 | **Kill white clipping** | Filmic tonemap + lower capture exposure/bloom/emissives. Hero stills must not blow highlights (C8 ChatGPT P0). |
| 2 | **Reflections DEFINING** | Planar hood RT + planar-wet with midtone band contrast on dark paint/asphalt carriers. Readable building/lamp mirrors — not just glints, not white slabs. |
| 3 | **Human quality floor** | Blender face atlases on dense UV soft billboards + labeled `blender_peds/` beauty stills. Same five only. |
| 4 | **Cruiser paint/glass rewrite** | Dark body clearcoat carrier; glass dark dielectric; emit/chrome capped. |
| 5 | **Night DEFINE (exposure-safe)** | lamp→wet pool→hood bands→glass→façade; night exposure 1.05. |
| 6 | **Mini sheet** | `sheet_cycle9_mini.jpg` (≤4 tiles). |

## Authored meshes used

- `hm_bank_annex_v10.obj` (+ `_soft.obj` + `.mtl`) — Meridian Mutual exterior
- `hm_storefront_v10.obj` — west neighbor
- `hm_midrise_v10.obj` — mid block neighbor
- `hmpd_cruiser_v12b.obj` — hero HMPD cruiser
- `peds/hm_ped_{rae,dane,suki,noah,ivy}.obj` — Cycle-8 Blender photoreal-adjacent characters

Blender authoring: `/workspace/vaultline-blender/scripts/build_harbor_peds_v9.py`

## Remaining gaps (honest)

- Soft path is still software raster (no TAA); face atlases help but billboard integration remains visible vs film AAA faces.
- Soft env is structured cubemap + planar helper (not captured realtime cubemap / full ray-SSR).
- Pedestrians are posed static meshes — no runtime skeletal clip playback yet.
- Background is a controlled ring + fog walls + reflection cards, not a full district.
- Soft contact shadows improved but still short of AAA AO.
- Full-city replication of this block's standards not done (benchmark-first by design).
