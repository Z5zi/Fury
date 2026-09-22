# AAA Meridian Block Capture Log — Cycle-14

Brand: Harbor Metro / HMPD / Meridian Mutual only. Timezone: Europe/London (BST/UTC+1).
Captured: 2026-09-22 ~01:45–02:00 BST.

## Soft (SECONDARY) — wiring retained
Soft heroes live under `soft_secondary/` only (never pack-root `01_*.jpg`).
Soft is NOT scored for AAA photoreal. Soft capture exit-0 is optional secondary.

## Blender Cycles beauty (PRIMARY)
Path: `artifacts/aaa_meridian_block/blender_scene_beauty/`
Scripts:
- `/workspace/Fury/scripts/render_meridian_block_beauty_v14.py`
- `/workspace/Fury/scripts/build_harbor_peds_v14.py`
(+ mirrors under `/workspace/vaultline-blender/scripts/`)

### Cycle-14 deltas vs C13 (2.5 NO-SHIP)
1. **Lobby rebuilt** — authored Meridian Mutual interior with stone/plaster, desk, chairs, monitors, ceiling practicals, dimensional MERIDIAN MUTUAL wordmark. Camera pulled back/wide so lobby reads as a room (not wall close-up). Bank kit hidden for lobby shot.
2. **Floating façade cards deleted** — `_ash_` / `ashrev` / `_rev` / `_pane` / `_room` / `reveal` / `cheek` hard-deleted at import+hide_junk.
3. **Poly Haven asphalt** — `asphalt_04` CC0 via Object-space tiling + darken + puddle mask + irregular wet decals.
4. **Cruiser wheels** — tire torus + tread ribs + rim rings + 5 thick spokes + rotor + caliper + lugs (prior disc/v13 wheels hard-deleted). Cruiser base switched to cleaner `hmpd_cruiser_v7b` to avoid v13 floating greebles.
5. **Peds A-pose/idle** — Antonia.Polygon CC0 with gentle outer-arm drop + per-ped yaw asymmetry; 3 staggered heroes (not 5-wide mannequin line); shrinkwrap clothing shells.
6. **Architecture PH textures** — stone_wall_02 / plastered_wall_02 / concrete_wall_008.
7. **Night** — denser practicals + distant window glow; wet asphalt reflections.

### Prior executor failure (found)
- Mid-run beauty re-render (`beauty_v14b`) interrupted after day shots; night finished late.
- Blender Python lacked PIL → all crops failed until bpy-image crop path added.
- Scripts claimed façade/wheel fixes but: (a) `_ash_` filter missed many ashrev cards until hard-delete; (b) Kenney single-slot wheels got stomped to stone/plaster by `force_architecture_ph`; (c) lobby camera was too tight; (d) aggressive mesh A-pose tore shoulders.

### Honest expected score
Likely **4.5–6.5 / 10** — below 8.0 gate. Remaining gaps: Antonia faces/hair still stylized; clothing shells basic; bank window reveals can still read as panel rhythm; asphalt aggregate readability limited in daylight. Do not claim ≥8 without ChatGPT judgment.
