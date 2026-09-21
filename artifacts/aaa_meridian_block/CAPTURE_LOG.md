# AAA Meridian Block Capture Log

Branding: Harbor Metro / HMPD / Meridian Mutual only.

Renderer: soft (`--aaa-block-capture`) — Cycle-10 @ 1280×720.

- 01_lobby pos=(35,1.85,12.5) yaw=-1.5708 pitch=-0.05 day OK artifacts/aaa_meridian_block/01_lobby.ppm — Meridian Mutual entrance + lobby glimpse
- 02_street pos=(12,1.85,18.6) yaw=-1.48 pitch=-0.48 day OK artifacts/aaa_meridian_block/02_street.ppm — Cycle-10 reflection-hero: planar-wet buildings/lamps as READABLE shapes in reflection
- 03_cruiser pos=(20.8,1.15,15) yaw=-2.55 pitch=-0.28 day OK artifacts/aaa_meridian_block/03_cruiser.ppm — Hero HMPD cruiser — dark clearcoat + planar hood RT env bands DEFINING
- 04_peds pos=(5.55,1.62,13.55) yaw=0.85 pitch=-0.05 day OK artifacts/aaa_meridian_block/04_peds.ppm — Cycle-10 face-hero: sharper atlases + denser hair cards + contact grounding
- 05_night_or_alt pos=(17.8,1.85,18.8) yaw=-1.72 pitch=-0.26 night OK artifacts/aaa_meridian_block/05_night_or_alt.ppm — Night DEFINE chain: lamp→wet streak→hood→glass→façade (0% clip)

## Cycle-10 changes
1. **Humans — Blender face atlases** — Same Rae/Dane/Suki/Noah/Ivy. Soft dense UV billboards sample Blender-baked face albedo atlases (not plastic box LODs). Beauty stills remain facial proof path.
2. **Reflections DEFINING** — planar reflection RT on clearcoat hood (readable building bands) + planar-wet elongated lamp pools. Caps wet 0.99 / clearcoat 0.98 / glass 0.96. Must be FIRST noticed on 02/03/05.
3. **Renderer correctness** — keep sparkle-safe; artifact-free hero frames.
4. **Night expensive via interaction** — lamp→wet pool→cruiser hood building bands→glass→façade→ped rim.
5. **Asphalt wet/used at distance** — hero wet mirrors + elongated lamp streaks.
6. **Mini contact sheet** — sheet_cycle9_mini.jpg (≤4 tiles) for low-upload ChatGPT; full-res PNGs kept.

## Top mapping
See docs/AAA_MERIDIAN_BLOCK.md


## Cycle-10 clip audit (soft heroes @1280×720)
| Shot | mean luma | p99 | %≥250 | %≥245 |
|------|-----------|-----|-------|-------|
| 01_lobby | 78.5 | 153.7 | 0.000% | 0.000% |
| 02_street | 56.0 | 161.4 | 0.000% | 0.000% |
| 03_cruiser | 40.1 | 148.2 | 0.000% | 0.000% |
| 04_peds | 60.7 | 190.7 | 0.000% | 0.000% |
| 05_night_or_alt | 100.7 | 210.4 | 0.000% | 0.000% |

Reflection crops (structure proxy): street reflect std≈52, hood std≈41, night asphalt std≈44.

Quota-safe: `cycle10_ALL_IN_ONE.jpg` + `sheet_cycle10_mini.jpg`.
Secondary: `blender_scene_beauty/meridian_street_peds_cruiser_beauty.png` (Blender Cycles, labeled).
