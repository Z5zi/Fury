# AAA Meridian Block Capture Log

Branding: Harbor Metro / HMPD / Meridian Mutual only.

Renderer: soft (`--aaa-block-capture`) — Cycle-6 @ 1280×720.

- 01_lobby pos=(35,1.85,12.5) yaw=-1.5708 pitch=-0.05 day OK artifacts/aaa_meridian_block/01_lobby.ppm — Meridian Mutual entrance + lobby glimpse
- 02_street pos=(10,3.2,23.5) yaw=-1.5708 pitch=-0.22 day OK artifacts/aaa_meridian_block/02_street.ppm — Intersection wet asphalt reflections + road→curb→sidewalk to Meridian
- 03_cruiser pos=(23,1.45,16.5) yaw=-2.52 pitch=-0.1 day OK artifacts/aaa_meridian_block/03_cruiser.ppm — Hero HMPD cruiser — hood/doors/windshield + wet asphalt env reflections
- 04_peds pos=(7.55,1.42,15.15) yaw=0.15 pitch=-0.06 day OK artifacts/aaa_meridian_block/04_peds.ppm — Five Cycle-6 Blender AAA Harbor Metro characters — close crop for faces/hands
- 05_night_or_alt pos=(19.5,2.25,20.8) yaw=-1.85 pitch=-0.14 night OK artifacts/aaa_meridian_block/05_night_or_alt.ppm — Expensive night: lamp→wet asphalt→cruiser→glass→façade bounce→ped rim→haze

## Cycle-6 changes
1. **Five humans actually AAA** — Rae/Dane/Suki/Noah/Ivy ONLY. Blender higher-poly anatomical faces (eyes/ears/nose/lips), articulated fingers/palms, shoe construction, layered clothing, hair, skin SSS+roughness, fabric/leather/rubber. Multi-pose silhouettes. Soft 04_peds closer + Blender beauty stills in blender_peds/.
2. **Reflections impossible to miss** — scene cubemap (façade/window bands + lamp streaks); raised env mix caps (wet asphalt 0.92 / clearcoat 0.90 / glass 0.88); wet SSR scream; higher F0. Must SEE on 03_cruiser + 02_street.
3. **Renderer correctness** — sparkle-safe microdetail, wider clearcoat lobe, emissive Ke clamp, controlled bloom. Zero sparkle/z-fight/alias/emissive clip in hero stills.
4. **Night expensive via interaction** — same 8 lights; lamp→wet asphalt→cruiser→glass→façade bounce→ped rim→haze.
5. **Asphalt wet/used at capture distance** — larger aggregate/patches/tire/oil/drains/curb grit; wet mirrors wetness~0.98 under hero cams.
6. **Kept denser Meridian dressing + city ring** — no footprint growth, no blue void regression.

## Top mapping
See docs/AAA_MERIDIAN_BLOCK.md
