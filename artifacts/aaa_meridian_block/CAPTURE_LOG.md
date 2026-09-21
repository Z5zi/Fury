# AAA Meridian Block Capture Log

Branding: Harbor Metro / HMPD / Meridian Mutual only.

Renderer: soft (`--aaa-block-capture`) — Cycle-4.

- 01_lobby pos=(35,1.85,12.5) yaw=-1.5708 pitch=-0.05 day OK artifacts/aaa_meridian_block/01_lobby.ppm — Meridian Mutual entrance + lobby glimpse
- 02_street pos=(10,3.4,24) yaw=-1.5708 pitch=-0.2 day OK artifacts/aaa_meridian_block/02_street.ppm — Intersection road→curb→sidewalk looking north to Meridian
- 03_cruiser pos=(24,1.9,17.5) yaw=-2.6 pitch=-0.1 day OK artifacts/aaa_meridian_block/03_cruiser.ppm — Hero HMPD cruiser v12b grounded on asphalt
- 04_peds pos=(6,1.8,16) yaw=-0.3 pitch=-0.05 day OK artifacts/aaa_meridian_block/04_peds.ppm — Five Cycle-4 Harbor Metro characters (upgraded) on Meridian block
- 05_night_or_alt pos=(20,2.6,22) yaw=-1.9 pitch=-0.14 night OK artifacts/aaa_meridian_block/05_night_or_alt.ppm — Alt/night lighting hierarchy on same block

## Cycle-4 changes
1. **Characters (highest)** — upgraded same five hm_ped_* : proportions, facial topo, articulated hands, shoes, volumetric hair, garment construction, skin/fabric diffs, distinct silhouettes, natural walk/idle/converse/lean poses + conversation grouping.
2. **Material response** — probe-atlas env reflect (not sky/fog stub alone), wet-road SSR-lite, stronger clearcoat/glass/metal, roughness variation, microdetail, richer specular.
3. **Lighting** — keep cascades; richer bounce/fill, 8 night point lights (lobby/lamps/window spill/cruiser), contact shadows, atmospheric fog; night shot strengthened.
4. **Meridian Mutual density** — tellers, queue stanchions, security desk, coffers, baseboards, layered plants, wear strips (same footprint).
5. **Kill blue void** — denser midrise/skyline ring, rooftop bulkheads, vertical fog walls, camera-wedge fillers; capture owns clear/fog (no day_night Harbor-blue stomp).
6. **Street physicality** — tire wear, drain grilles, curb grit, wet asphalt response.

## Top mapping
See docs/AAA_MERIDIAN_BLOCK.md
