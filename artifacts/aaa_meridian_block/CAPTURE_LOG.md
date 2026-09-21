# AAA Meridian Block Capture Log

Branding: Harbor Metro / HMPD / Meridian Mutual only.

Renderer: soft (`--aaa-block-capture`) — Cycle-3.

- 01_lobby pos=(35,1.85,12.5) yaw=-1.5708 pitch=-0.05 day OK artifacts/aaa_meridian_block/01_lobby.ppm — Meridian Mutual entrance + lobby glimpse
- 02_street pos=(10,3.4,24) yaw=-1.5708 pitch=-0.2 day OK artifacts/aaa_meridian_block/02_street.ppm — Intersection road→curb→sidewalk looking north to Meridian
- 03_cruiser pos=(24,1.9,17.5) yaw=-2.6 pitch=-0.1 day OK artifacts/aaa_meridian_block/03_cruiser.ppm — Hero HMPD cruiser v12b grounded on asphalt
- 04_peds pos=(6,1.8,16) yaw=-0.3 pitch=-0.05 day OK artifacts/aaa_meridian_block/04_peds.ppm — Five Cycle-3 Harbor Metro characters on Meridian block
- 05_night_or_alt pos=(20,2.6,22) yaw=-1.9 pitch=-0.14 night OK artifacts/aaa_meridian_block/05_night_or_alt.ppm — Alt/night lighting hierarchy on same block

## Cycle-3 changes
1. **AAA material appearance** — soft clearcoat/glass transmission+env reflect/metal F0/emissive_color/microdetail/bounce fill on every Meridian-block surface.
2. **Five convincing characters** — regenerated hm_ped_* with face/hands/shoes/hair/clothing construction + distinct silhouettes (jacket/tee/blouse/hoodie/coat).
3. **Lighting + shadows** — 1024 cascaded soft PCF (near+far), 5x5 contact-hardening, 4 point lights, night spill/bounce, street lamp emissives.
4. **Authored Meridian Mutual** — door/frames/windows/blinds/interior rooms/signage/security/HVAC/pipes + lobby marble/reception/ceiling lights/chairs/plants/art.
5. **Urban fill** — background midrises + window emissives to kill blue void; parked civ cars + street lamps.

## Top mapping
See docs/AAA_MERIDIAN_BLOCK.md
