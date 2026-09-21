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
| 6 | *(pending ChatGPT)* | Target **>8.0/10** — Blender higher-poly humans, reflections impossible to miss, 1280×720, expensive night interaction |

## Capture

```bash
cd build/apps/vaultline
SDL_VIDEODRIVER=dummy ./vaultline --soft --aaa-block-capture
# Stills + CAPTURE_LOG.md → artifacts/aaa_meridian_block/ (1280×720 Cycle-6)
```

| File | Subject |
|------|---------|
| `01_lobby.ppm` | Meridian Mutual entrance + denser authored lobby |
| `02_street.ppm` | Intersection + wet asphalt reflections + skyline ring |
| `03_cruiser.ppm` | Hero HMPD cruiser (clearcoat / glass / wet env reflect) |
| `04_peds.ppm` | Five Cycle-6 Blender higher-poly Harbor Metro characters (close crop) |
| `05_night_or_alt.ppm` | Expensive night: lamp→wet asphalt→cruiser→glass→façade→ped→haze |
| `CAPTURE_LOG.md` | Camera poses + Cycle-6 notes |
| `blender_peds/` | **Blender Cycles beauty stills** (face + full) — facial fidelity proof when soft crop is limited. Label clearly as Blender, not soft-path. |
| `*_crop_*.png` | Soft-path crops for P0 pixel proof |

## Cycle-6 → ChatGPT ordered priorities

| # | Priority | Cycle-6 change |
|---|----------|----------------|
| 1 | **Five humans actually AAA** | Same Rae/Dane/Suki/Noah/Ivy only — Blender higher-poly (~28–30k verts) continuous cranial vault + recessed eyes, articulated hands, shoe construction, layered clothing, hair, skin SSS+roughness, fabric/leather/rubber. Multi-pose. Soft 04 closer + Blender beauty stills. |
| 2 | **Reflections impossible to miss** | Scene cubemap (façade/window/column bands + lamp streaks); raised env mix caps (wet asphalt 0.92 / clearcoat 0.90 / glass 0.88); wet SSR scream; higher F0; sky/warm façade emissive reflection cards; forced cruiser paint clearcoat. Must SEE on `03_cruiser` + `02_street`. |
| 3 | **Renderer correctness** | Sparkle-safe microdetail, wider clearcoat lobe, emissive Ke clamp, controlled bloom. Hero stills clean. Capture **1280×720**. |
| 4 | **Expensive night via interaction** | Same 8 lights; lamp→wet asphalt→cruiser→glass→façade bounce→ped rim→haze. |
| 5 | **Asphalt wet/used at capture distance** | Larger aggregate/patches/tire/oil/drains/curb grit; wet mirrors wetness~0.98 under hero cams. |
| 6 | Keep Meridian dressing + city ring | No footprint growth; no blue-void regression. |

## Authored meshes used

- `hm_bank_annex_v10.obj` (+ `_soft.obj` + `.mtl`) — Meridian Mutual exterior
- `hm_storefront_v10.obj` — west neighbor
- `hm_midrise_v10.obj` — mid block neighbor
- `hmpd_cruiser_v12b.obj` — hero HMPD cruiser
- `peds/hm_ped_{rae,dane,suki,noah,ivy}.obj` — Cycle-6 Blender higher-poly AAA characters (~28–30k verts)

Blender authoring: `/workspace/vaultline-blender/scripts/build_harbor_peds_v6.py`

## Remaining gaps (honest)

- Soft path is still software raster (no TAA); facial/hand micro-read limited vs film AAA even at 1280×720 — use `blender_peds/` beauty stills as supplemental facial proof.
- Soft env is a structured scene cubemap + wet SSR-lite (not captured realtime cubemap / full ray-SSR).
- Pedestrians are posed static meshes (authored weight-shift / converse / walk / lean) — no runtime skeletal clip playback yet.
- Background is a controlled ring + fog walls + reflection cards, not a full district.
- Full-city replication of this block's standards not done (benchmark-first by design).
