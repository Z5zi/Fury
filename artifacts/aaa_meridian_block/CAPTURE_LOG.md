# AAA Meridian Block Capture Log

Branding: Harbor Metro / HMPD / Meridian Mutual only.

Renderer: soft (`--aaa-block-capture`) — Cycle-9 @ 1280×720 — **exposure-safe**.

**ChatGPT Cycle-8 heroes scored 5.4/10 NO-SHIP** (humans FAIL, reflections NOT DEFINING, artifacts/presentation FAIL — severe white/value clipping). Cycle-9 prioritizes presentation fix over new content. Soft heroes measure **0% near-white clip**.

- 01_lobby — Meridian Mutual entrance + lobby glimpse
- 02_street — Planar-wet reflection-hero: elongated lamp pools + façade bands (midtone contrast)
- 03_cruiser — Hero HMPD cruiser — dark paint clearcoat + planar hood RT building bands; glass rewrite
- 04_peds — Face-hero: Blender face albedo atlases on dense UV billboards (Rae/Dane/Suki/Noah/Ivy)
- 05_night_or_alt — Night DEFINE: planar-wet pools + cruiser hood bands (night exposure 1.05)

## Cycle-9 changes (presentation-first)
1. **Kill white clipping** — filmic soft-shoulder tonemap + display ceiling; day exposure 0.74 / night 1.05; bloom/emissive caps; soft luma ceiling. 0% near-white clip on all 5 heroes.
2. **Reflections DEFINING via structure** — planar hood RT + planar-wet SSR with midtone-preserving band contrast on dark paint/asphalt carriers.
3. **Humans** — Blender v9 face albedo atlases on dense UV soft billboards. Beauty face/full in `blender_peds/` = labeled secondary proof.
4. **Cruiser paint/glass rewrite** — dark body clearcoat carrier; glass dark dielectric; chrome/emit capped.
5. **Mini sheet** — `sheet_cycle9_mini.jpg` (≤4 tiles); keep full-res PNGs.

## Top mapping
See docs/AAA_MERIDIAN_BLOCK.md

## Honest gaps
- Soft raster limits facial microdetail; atlases help but billboard integration remains visible.
- Soft env = structured cubemap + planar helper (not captured cubemap / full ray-SSR).
- Blender faces authored/procedural (not photogrammetry).
- Soft contact shadows improved but short of AAA AO.
