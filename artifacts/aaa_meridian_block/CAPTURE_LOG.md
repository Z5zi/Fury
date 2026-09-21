# AAA Meridian Block Capture Log

Branding: Harbor Metro / HMPD / Meridian Mutual only.

Renderer: soft (`--aaa-block-capture`) — Cycle-8c @ 1280×720.

**ChatGPT Cycle-7 compact pack scored 5.8/10 NO-SHIP** (soft peds 4.3, Blender ~4.1, reflections 4.7 P0 FAIL). Contact sheets may have hurt Blender scores — prefer **full-res PNG heroes + crops + blender face/full** for rejudge.

- 01_lobby — Meridian Mutual entrance + lobby glimpse
- 02_street — Cycle-8c reflection-hero: thin white-hot elongated lamp pools on wet asphalt
- 03_cruiser — Hero HMPD cruiser — DEFINING hood mirrored façade columns + door/glass
- 04_peds — Cycle-8c face-hero: peds facing camera + large sclera/iris/catch/lip soft LODs
- 05_night_or_alt — Night DEFINE: elongated lamp pools dominate wet asphalt + cruiser hood mirror

## Cycle-8 changes (8 → 8b → 8c)
1. **Humans** — Same Rae/Dane/Suki/Noah/Ivy. Blender v8 photoreal-adjacent authoring (cornea+catchlight, thicker lids, lip volume, ear helix/concha, denser hair) + beauty face/full @1024. Soft: large never-culled face LODs (sclera/iris/pupil/catchlight/brow/nose/lip/ear) with peds facing camera.
2. **Reflections DEFINING** — planar-wet SSR helper + thin white-hot elongated lamp pool streaks (not fat orange blocks) + brighter façade columns for hood mirror; stronger clearcoat env; wet diff-kill. Caps wet~0.99 / coat~0.98.
3. **Artifact-free** — keep sparkle-safe; night interaction chain.
4. **Contact sheet** — `sheet_cycle8.jpg` secondary only; primary ChatGPT pack = full-res PNGs.

## Top mapping
See docs/AAA_MERIDIAN_BLOCK.md

## Honest gaps
- Soft software raster still limits facial microdetail vs film AAA; Blender beauty stills remain the facial proof path.
- Soft env is structured cubemap + planar-wet SSR-lite (not captured cubemap / full ray-SSR).
- Blender faces remain stylized/procedural (not scanned photogrammetry) — push continues next cycle if ChatGPT still fails CLEAR.
