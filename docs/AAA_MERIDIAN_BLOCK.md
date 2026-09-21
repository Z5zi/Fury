# AAA Meridian Block — ChatGPT Soft-Capture Benchmark

**Branding:** Harbor Metro / HMPD / Meridian Mutual only — no Rockstar/GTA IP.

**Goal:** One controlled Harbor Metro street block that ChatGPT can re-judge against the
modern AAA visual bar.

| Cycle | Score | Verdict |
|-------|-------|---------|
| 1 | 4.0/10 | Broken / uncontrolled prototype |
| 2 | 5.0/10 | Controlled, materially improved prototype — **NO-SHIP** |
| 3 | *(pending)* | Target **>8.0/10** — AAA material appearance + characters + lighting + authored Meridian |

## Capture

```bash
# From build tree (assets staged beside binary):
cd build/apps/vaultline
SDL_VIDEODRIVER=dummy ./vaultline --soft --aaa-block-capture
# Stills + CAPTURE_LOG.md → artifacts/aaa_meridian_block/ (cwd or repo-relative)
```

| File | Subject |
|------|---------|
| `01_lobby.ppm` | Meridian Mutual entrance + authored lobby |
| `02_street.ppm` | Intersection road→curb→sidewalk + urban fill |
| `03_cruiser.ppm` | Hero HMPD cruiser v12b (clearcoat / glass / metal) |
| `04_peds.ppm` | Five Cycle-3 Harbor Metro characters |
| `05_night_or_alt.ppm` | Night hierarchy + lamp spill + bounce |
| `CAPTURE_LOG.md` | Camera poses + cycle notes |

## Cycle-3 → ChatGPT Top fixes (for 8.0+)

| # | ChatGPT fix | Cycle-3 change |
|---|-------------|----------------|
| 1 | **AAA material appearance** | Soft path: clearcoat lobe, glass transmission+fresnel+env reflect, metal F0, emissive_color, world-space microdetail, hemisphere bounce fill, point-light specular. MTL heuristics set clearcoat/skin/fabric/chrome. Upgrades every visible Meridian-block surface at once. |
| 2 | **5 convincing characters** | Regenerated `hm_ped_{rae,dane,suki,noah,ivy}` with face (nose/brow/ears/chin), hands+fingers, shoe construction, hair styles, clothing (jacket/tee/blouse/hoodie/coat), distinct silhouettes + walk/idle phases. |
| 3 | **Lighting + shadow quality** | Soft 1024 cascaded PCF (near+far), 5×5 contact-hardening filter, ambient keep-in-shadow, 4 point lights (lobby/street/ATM), night spill+bounce, street-lamp emissives, bloom. |
| 4 | **Authored Meridian Mutual** | Door+frame+brass handle, window bays with glass/blinds/interior rooms, cornice/pilasters, security cam+keypad, roof HVAC/vents/pipes, lobby marble floor, reception desk+wood top+glass, recessed ceiling lights, wall panels, rug, chairs, plants, art, columns. |
| 5+ | Street / reflections / density / post | Partial: urban bg midrises+window emissives (kill blue void), parked civ cars, street lamps; soft env-reflect stub strengthened; wet asphalt aniso retained. Full TAA/SSR still absent. |

## Cycle-2 (retained)

| # | Fix | Status |
|---|-----|--------|
| 1 | Runtime multi-material pipeline | PASS — cruiser/bank/storefront/midrise/peds per-`usemtl` |
| 2 | 5 authored pedestrians | PASS impl → Cycle-3 quality upgrade |
| 3 | Directional shadows | PASS → Cycle-3 cascades/PCF |
| 4 | Cruiser full material stack | PASS → Cycle-3 clearcoat/glass/metal response |
| 5 | Street material treatment | CONDITIONAL — cracks/patches/oil/tiles retained |

## Cycle-1 Top 10 (still in place)

Camera near 0.22 · kill debug placeholders · PBR-ish slots · street hierarchy · NPCs · HMPD cruiser · lighting hierarchy · prop density · grounding · mild post.

## Authored meshes used

- `hm_bank_annex_v10.obj` (+ `_soft.obj` + `.mtl`) — Meridian Mutual exterior
- `hm_storefront_v10.obj` — west neighbor
- `hm_midrise_v10.obj` — mid block neighbor
- `hmpd_cruiser_v12b.obj` — hero HMPD cruiser
- `peds/hm_ped_{rae,dane,suki,noah,ivy}.obj` — Cycle-3 characters

## Remaining gaps (post Cycle-3)

- Soft env reflection is a sky/fog probe stub (no SSR / cubemap atlas).
- Pedestrians are static posed meshes (no runtime skeletal idle/walk clips).
- Background fill is a controlled ring of midrises, not a full district.
- No TAA / temporal accumulation on soft path.
- Full-city replication of this block’s standards not done (benchmark-first by design).
