# AAA Meridian Block Capture Log — Cycle-13

Brand: Harbor Metro / HMPD / Meridian Mutual only. Timezone: Europe/London (BST).

## Soft (SECONDARY) — exit 0 retained from Cycle-11 wiring
Renderer: soft (`--aaa-block-capture`) @ 1280×720.
Orange AaaBarrier / HarborCone row removed from frustums (Cycle-11). Soft not scored for AAA.

## Blender Cycles beauty (PRIMARY)
Path: `artifacts/aaa_meridian_block/blender_scene_beauty/`
Script: `/workspace/Fury/scripts/render_meridian_block_beauty_v13.py`

### Cycle-13 deltas
1. **Wet asphalt** — aggregate noise + voronoi grit, sparse puddle mask, tire-path darkening, coat only in wet regions; explicit puddle meshes with dulled reflection.
2. **Cruiser** — v13b mesh (junk cage/grime removed, lightbar multi-lens) + in-scene rim/tire/rotor rebuild; navy clearcoat retained.
3. **Peds** — all five Antonia.Polygon CC0 (dane/noah no Quaternius); body-face clothing paint; closer hero camera.
4. **Architecture** — procedural stone/paint/concrete/metal/glass; street-facing façade cards with window frames + weathering; ash banners hidden.
5. **Night** — denser window practicals + street points + ambient fill.
6. **Junk** — ash banners, cage, thin wires, env wash lights reduced.

### Honest gaps (may still block ≥8.0)
- Antonia faces/hair still stylized vs photoreal AAA; clothing is albedo bands not tailored garments.
- Cruiser secondary trim still imperfect at extreme close-up.
- Night city density still limited vs AAA open-world.
