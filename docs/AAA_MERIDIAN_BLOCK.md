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
| 5 | *(pending)* | Target **>8.0/10** — PIXEL QUALITY: AAA humans, visible reflections, clean render, expensive night, asphalt physicality |

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


## Cycle-5 → ChatGPT ordered priorities (for 8.0+)

| # | Priority | Cycle-5 change |
|---|----------|----------------|
| 1 | **Five humans genuinely AAA** | Same Rae/Dane/Suki/Noah/Ivy only — capsule/ellipsoid limbs (no box toys), readable faces (ears/nose/lips/eyes), articulated hands, shoe construction, hair masses, SSS/skin roughness, fabric/leather/rubber MTL, weight-shift poses. Soft crop must not read as low-poly toys. |
| 2 | **Reflections VISIBLE in pixels** | Stronger probe-atlas env_w on paint/glass/wet asphalt; clearcoat sheen; wet SSR-lite boosted; contrasty horizon probe. Must read on `03_cruiser` + `02_street`. |
| 3 | **P0 renderer artifacts** | Sparkle-safe microdetail (damped on coat/metal), wider clearcoat lobe, emissive Ke clamp (DRL), softer bloom. Hero stills clean. |
| 4 | **Expensive night** | Keep 8-light architecture; higher intensity/radius; lamp pools on pavement; continuous lamp→pavement→car→facade→glass→ped→haze. |
| 5 | **Street physicality** | Larger asphalt aggregate, wet patches (wetness~0.9), curb grit, tire wear, visible repair chips — not a colored plane. |
| 6 | Keep Meridian denser dressing + city ring | No footprint growth; no blue-void regression. |

## Capture

```bash
cd build/apps/vaultline
SDL_VIDEODRIVER=dummy ./vaultline --soft --aaa-block-capture
# Stills + CAPTURE_LOG.md → artifacts/aaa_meridian_block/
```

| File | Subject |
|------|---------|
| `01_lobby.ppm` | Meridian Mutual entrance + denser authored lobby |
| `02_street.ppm` | Intersection + wet asphalt / reflections + skyline ring |
| `03_cruiser.ppm` | Hero HMPD cruiser (clean clearcoat / glass / wet reflect — no sparkle) |
| `04_peds.ppm` | Five Cycle-5 Harbor Metro AAA characters |
| `05_night_or_alt.ppm` | Expensive night hierarchy + 8 local lights + lamp pools |
| `CAPTURE_LOG.md` | Camera poses + Cycle-5 notes |

## Authored meshes used

- `hm_bank_annex_v10.obj` (+ `_soft.obj` + `.mtl`) — Meridian Mutual exterior
- `hm_storefront_v10.obj` — west neighbor
- `hm_midrise_v10.obj` — mid block neighbor
- `hmpd_cruiser_v12b.obj` — hero HMPD cruiser
- `peds/hm_ped_{rae,dane,suki,noah,ivy}.obj` — Cycle-5 capsule-limb AAA characters

## Remaining gaps (honest)

- Soft probe-atlas is procedural (not captured cubemap / full SSR).
- Pedestrians are posed static meshes (no runtime skeletal clip playback) — poses authored for weight-shift / converse / walk / lean.
- Background is a controlled ring + fog walls, not a full district.
- No TAA / temporal accumulation on soft path.
- Full-city replication of this block's standards not done (benchmark-first by design).

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

## Cycle-5 remaining gaps (honest)

- Soft probe-atlas is procedural (not captured cubemap / full SSR) — reflections improved but still soft-path limited.
- Pedestrians are posed static meshes (capsule/ellipsoid authored weight-shift) — no runtime skeletal clip playback.
- Soft resolution (960×540) still limits facial/hand micro-read vs film AAA; closer 04_peds camera mitigates.
- Background is a controlled ring + fog walls, not a full district.
- No TAA / temporal accumulation on soft path.
- Full-city replication of this block’s standards not done (benchmark-first by design).
