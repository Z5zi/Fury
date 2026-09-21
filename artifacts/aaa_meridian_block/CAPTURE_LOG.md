# AAA Meridian Block Capture Log

Branding: Harbor Metro / HMPD / Meridian Mutual only.

Renderer: soft (`--aaa-block-capture`) — Cycle-5.

- 01_lobby pos=(35,1.85,12.5) yaw=-1.5708 pitch=-0.05 day OK artifacts/aaa_meridian_block/01_lobby.ppm — Meridian Mutual entrance + lobby glimpse
- 02_street pos=(10,3.4,24) yaw=-1.5708 pitch=-0.2 day OK artifacts/aaa_meridian_block/02_street.ppm — Intersection road→curb→sidewalk looking north to Meridian
- 03_cruiser pos=(23.2,1.55,16.8) yaw=-2.55 pitch=-0.08 day OK artifacts/aaa_meridian_block/03_cruiser.ppm — Hero HMPD cruiser — clearcoat/glass/wet asphalt reflections
- 04_peds pos=(6.4,1.55,15.6) yaw=-0.22 pitch=-0.02 day OK artifacts/aaa_meridian_block/04_peds.ppm — Five Cycle-5 Harbor Metro AAA characters (capsule limbs, faces, hands)
- 05_night_or_alt pos=(19.5,2.35,21.2) yaw=-1.85 pitch=-0.12 night OK artifacts/aaa_meridian_block/05_night_or_alt.ppm — Expensive night: lamp→pavement→car→facade→glass→ped→haze

## Cycle-5 changes
1. **Five humans genuinely AAA** — Rae/Dane/Suki/Noah/Ivy regenerated with capsule limbs, readable faces (ears/nose/lips/eyes), articulated hands, shoe construction, hair masses, SSS/skin roughness, fabric/leather/rubber MTL. Weight-shift / walk / converse / lean silhouettes. No extra NPCs.
2. **Reflections VISIBLE in pixels** — stronger probe-atlas env_w on paint/glass/wet asphalt; clearcoat sheen; wet SSR-lite boosted; contrasty horizon probe. Must read on 03_cruiser + 02_street.
3. **P0 renderer artifacts** — sparkle-safe microdetail (damped on coat/metal), wider clearcoat lobe, emissive Ke clamp, softer bloom. Cruiser still must be clean.
4. **Expensive night** — same 8-light architecture, higher intensity/radius; lamp pools on pavement; lamp→pavement→car→facade→glass→ped→haze continuous.
5. **Street physicality** — larger asphalt aggregate, wet patches (wetness~0.9), curb grit, tire wear, visible repair chips.
6. **Kept denser Meridian dressing + city ring** — no footprint growth, no blue void regression.

## Top mapping
See docs/AAA_MERIDIAN_BLOCK.md
