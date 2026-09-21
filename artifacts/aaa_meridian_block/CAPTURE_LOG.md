# AAA Meridian Block Capture Log

Branding: Harbor Metro / HMPD / Meridian Mutual only.

Renderer: soft (`--aaa-block-capture`) — Cycle-7 @ 1280×720.

- 01_lobby pos=(35,1.85,12.5) yaw=-1.5708 pitch=-0.05 day OK artifacts/aaa_meridian_block/01_lobby.ppm — Meridian Mutual entrance + lobby glimpse
- 02_street pos=(10,3.2,23.5) yaw=-1.5708 pitch=-0.22 day OK artifacts/aaa_meridian_block/02_street.ppm — Intersection DEFINING wet asphalt reflections + road→curb→sidewalk to Meridian
- 03_cruiser pos=(23,1.45,16.5) yaw=-2.52 pitch=-0.1 day OK artifacts/aaa_meridian_block/03_cruiser.ppm — Hero HMPD cruiser — DEFINING hood/door/windshield env reflections
- 04_peds pos=(6.15,1.55,14.35) yaw=0.35 pitch=-0.08 day OK artifacts/aaa_meridian_block/04_peds.ppm — Five Cycle-7 Blender photoreal Harbor Metro characters — close crop for faces/hands
- 05_night_or_alt pos=(19.5,2.25,20.8) yaw=-1.85 pitch=-0.14 night OK artifacts/aaa_meridian_block/05_night_or_alt.ppm — Expensive night: lamp→wet asphalt→cruiser→glass→façade bounce→ped rim→haze

## Cycle-7 changes
1. **Five humans photoreal finish** — Rae/Dane/Suki/Noah/Ivy ONLY. Cornea+eyelid wrap, facial planes, lips/philtrum, hair cap+clump+card hierarchy, skin SSS+warmth, clothing seams/folds, knuckle/nail hands, shoe construction, anatomical wrist/ankle. Soft 04 closer + Blender beauty face/full.
2. **Reflections DEFINING** — cubemap façade/window/silhouette bands + elongated lamp streaks; env mix caps wet 0.97 / clearcoat 0.96 / glass 0.94; diffuse kill under coat/wet; wet SSR hero; F0 push. Must DEFINE 03_cruiser + 02_street.
3. **Renderer correctness** — keep Cycle-6 sparkle-safe; kill microdetail alias, clearcoat instability, z-fight, transparency, reflection flicker, shadow instability, emissive clip.
4. **Night expensive via interaction** — same 8 lights; each lamp→wet asphalt→cruiser→glass→façade bounce→ped rim→haze chain visible.
5. **Asphalt wet/used at capture distance** — larger aggregate/patches/tire/oil/drains/curb grit; wet mirrors wetness~0.995 under hero cams.
6. **Kept denser Meridian dressing + city ring** — no footprint growth, no blue void regression.

## Top mapping
See docs/AAA_MERIDIAN_BLOCK.md
