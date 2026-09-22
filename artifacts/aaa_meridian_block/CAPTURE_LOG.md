# AAA Meridian Block Capture Log — Cycle-15

Brand: Harbor Metro / HMPD / Meridian Mutual only. Timezone: Europe/London (BST/UTC+1).
Captured: 2026-09-22 ~02:00–03:30 BST.

## Soft (SECONDARY) — wiring retained
Soft heroes live under `soft_secondary/` only (never pack-root `01_*.jpg`).
Soft is NOT scored for AAA photoreal. Soft capture exit-0 is optional secondary.
This pack includes an empty `soft_secondary/` directory as wiring proof (no root soft 01_*).

## Blender Cycles beauty (PRIMARY)
Path: `artifacts/aaa_meridian_block/blender_scene_beauty/`
Scripts:
- `/workspace/Fury/scripts/render_meridian_block_beauty_v15.py`
- `/workspace/Fury/scripts/build_harbor_peds_v15.py`
(+ mirrors under `/workspace/vaultline-blender/scripts/`)

### Cycle-15 deltas vs C14 (2.8 NO-SHIP)
1. **Asphalt not chrome** — Poly Haven `asphalt_04` dry-first: high roughness dry island + sparse shallow puddle mask; clearcoat only inside puddles; no geometric chrome puddle decals. Day asphalt crop reads matte aggregate grey (not mirror streak sheet).
2. **Amber/yellow debug purge** — `HMPDv7b_Amber` Ke blaze neutralized; headlight corner emitters cooled; material-name + RGB sweep for yellow/orange/magenta; Invisible ped mats alpha'd out.
3. **Ped wardrobe** — Ivy orange shirt/jacket retinted to olive/charcoal; SkinWarm desaturated; `force_ped_mats()` post-import. Antonia v15 fitted body-paint garments (no rectangular clothing shells).
4. **Façade cards** — `_ash_` / ashrev / `_rev` / `_pane` / `_room` / reveal hard-deleted at import+hide_junk (carried + verified in v15 beauty log).
5. **Lobby / wheels / night** — C14 lobby camera + spoke/rotor wheels + denser night practicals retained; beauty heroes re-cropped after amber purge re-render of 03/04.

### Self-inspect notes (executor)
- Asphalt crops (`02_street_asphalt_crop`, `05_night_asphalt_crop`): matte/wet-road, **not** chrome-like → no asphalt re-tweak/re-render required.
- Hero 03: large yellow amber turn-signal block **purged** + re-rendered; post-check YO≈0%.
- Hero 04: orange shirt + Invisible peach purged; residual warm skin/hair tones may still read stylized (not solid debug blaze). Peds remain below AAA human bar.
- Heroes 01/02/05: no solid yellow/orange debug primitives observed.

### Honest expected score
Likely **3.5–5.5 / 10** — still below 8.0 CLEAR gate. Gains vs C14: asphalt no longer chrome-dominant; cruiser yellow debug gone; wheels still manufactured. Remaining gaps: ped faces/hair still stylized mannequin (hair volumes, wardrobe paint bands); cruiser still blockout-adjacent; lobby still simplified; asphalt aggregate readability limited in daylight. Do **not** claim ≥8 without ChatGPT judgment.
