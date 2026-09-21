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
| 7 | *(pending ChatGPT)* | Target **>8.0/10** — photoreal ped finish, reflections DEFINING, 1280×720, night bounce chain |

## Capture

```bash
cd build/apps/vaultline
SDL_VIDEODRIVER=dummy ./vaultline --soft --aaa-block-capture
# Stills + CAPTURE_LOG.md → artifacts/aaa_meridian_block/ (1280×720 Cycle-7)
```

| File | Subject |
|------|---------|
| `01_lobby.ppm` | Meridian Mutual entrance + denser authored lobby |
| `02_street.ppm` | Intersection + wet asphalt reflections + skyline ring |
| `03_cruiser.ppm` | Hero HMPD cruiser (DEFINING clearcoat / glass / wet env reflect) |
| `04_peds.ppm` | Five Cycle-7 Blender photoreal Harbor Metro characters (close crop) |
| `05_night_or_alt.ppm` | Expensive night: lamp→wet asphalt→cruiser→glass→façade→ped→haze |
| `CAPTURE_LOG.md` | Camera poses + Cycle-6 notes |
| `blender_peds/` | **Blender Cycles beauty stills** (face + full) — facial fidelity proof when soft crop is limited. Label clearly as Blender, not soft-path. |
| `*_crop_*.png` | Soft-path crops for P0 pixel proof |

## Cycle-7 → ChatGPT ordered priorities

| # | Priority | Cycle-7 change |
|---|----------|----------------|
| 1 | **Five humans photoreal finish** | Same Rae/Dane/Suki/Noah/Ivy only — cornea+eyelid wrap, facial planes, lips, hair cap+clump+card hierarchy, skin SSS+warmth, clothing seams/folds, knuckle/nail hands, shoes, anatomical transitions. Multi-pose. Soft 04 closer + Blender beauty face/full. |
| 2 | **Reflections DEFINING** | Cubemap façade/window/silhouette bands + elongated lamp streaks; env mix caps wet 0.97 / clearcoat 0.96 / glass 0.94; diffuse kill under coat/wet; wet SSR hero; sky/warm/cool reflection cards; forced cruiser clearcoat. Must DEFINE `03_cruiser` + `02_street`. |
| 3 | **Renderer correctness** | Keep Cycle-6 sparkle-safe; kill microdetail alias, clearcoat instability, z-fight, transparency, reflection flicker, shadow instability, emissive clip. Capture **1280×720**. |
| 4 | **Expensive night via interaction** | Same 8 lights; each lamp→wet asphalt→cruiser→glass→façade bounce→ped rim→haze visible. |
| 5 | **Asphalt wet/used at capture distance** | Aggregate/patches/tire/oil/drains/curb grit; wet mirrors wetness~0.995 under hero cams. |
| 6 | Keep Meridian dressing + city ring | No footprint growth; no blue-void regression. |

## Authored meshes used

- `hm_bank_annex_v10.obj` (+ `_soft.obj` + `.mtl`) — Meridian Mutual exterior
- `hm_storefront_v10.obj` — west neighbor
- `hm_midrise_v10.obj` — mid block neighbor
- `hmpd_cruiser_v12b.obj` — hero HMPD cruiser
- `peds/hm_ped_{rae,dane,suki,noah,ivy}.obj` — Cycle-7 Blender photoreal-finish characters

Blender authoring: `/workspace/vaultline-blender/scripts/build_harbor_peds_v7.py`

## Remaining gaps (honest)

- Soft path is still software raster (no TAA); facial/hand micro-read limited vs film AAA even at 1280×720 — use `blender_peds/` beauty stills as supplemental facial proof.
- Soft env is a structured scene cubemap + wet SSR-lite (not captured realtime cubemap / full ray-SSR).
- Pedestrians are posed static meshes (authored weight-shift / converse / walk / lean) — no runtime skeletal clip playback yet.
- Background is a controlled ring + fog walls + reflection cards, not a full district.
- Full-city replication of this block's standards not done (benchmark-first by design).
