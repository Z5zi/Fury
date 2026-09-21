# AAA Meridian Block Capture Log

Branding: Harbor Metro / HMPD / Meridian Mutual only.

Renderer: soft (`--aaa-block-capture`) — Cycle-2.

- 01_lobby pos=(35,1.85,12.5) yaw=-1.5708 pitch=-0.05 day OK artifacts/aaa_meridian_block/01_lobby.ppm — Meridian Mutual entrance + lobby glimpse
- 02_street pos=(10,3.4,24) yaw=-1.5708 pitch=-0.2 day OK artifacts/aaa_meridian_block/02_street.ppm — Intersection road→curb→sidewalk looking north to Meridian
- 03_cruiser pos=(24,1.9,17.5) yaw=-2.6 pitch=-0.1 day OK artifacts/aaa_meridian_block/03_cruiser.ppm — Hero HMPD cruiser v12b grounded on asphalt
- 04_peds pos=(6,1.8,16) yaw=-0.3 pitch=-0.05 day OK artifacts/aaa_meridian_block/04_peds.ppm — Improved pedestrians on Meridian block
- 05_night_or_alt pos=(20,2.6,22) yaw=-1.9 pitch=-0.14 night OK artifacts/aaa_meridian_block/05_night_or_alt.ppm — Alt/night lighting hierarchy on same block

## Cycle-2 changes
1. **Multi-mat pipeline** — OBJ+MTL `usemtl` groups → per-submesh entities (cruiser 91 parts; bank 38; storefront 58; midrise 53).
2. **Authored peds** — 5 Harbor Metro ped OBJ+MTL (skin/shirt/pants/hair/shoes), not box humanoids.
3. **Directional shadows** — soft filtered shadow maps (PCF) on capture path; contact blobs retained.
4. **Cruiser material stack** — paint/glass/rubber/chrome/lamps/interior via MTL (no single-albedo override).
5. **Street treatment** — asphalt cracks/patches/oil stains, sidewalk tile variation, curb dirt.

## Top mapping
See docs/AAA_MERIDIAN_BLOCK.md
