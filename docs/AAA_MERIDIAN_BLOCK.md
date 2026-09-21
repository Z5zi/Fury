# AAA Meridian Block — ChatGPT Soft + Blender Beauty Benchmark

**Branding:** Harbor Metro / HMPD / Meridian Mutual only — no Rockstar/GTA IP.

## Cycle-12 strategy
Cycle-11 Blender beauty scored **4.0/10 NO-SHIP**. Ceiling: humans + material response.

| Role | Path | Purpose |
|------|------|---------|
| **PRIMARY** | `artifacts/aaa_meridian_block/blender_scene_beauty/` | ChatGPT AAA bar (Cycles) |
| **SECONDARY** | `artifacts/aaa_meridian_block/0{1-5}_*.png` | Soft runtime wiring proof |

### Cycle-12 beauty priorities
1. **Humans P0** — Rae/Dane/Suki/Noah/Ivy rebuilt on Antonia.Polygon (CC0) + Quaternius Matt/Sam; SSS skin, real eyes/lips, clothing shells, hair cards
2. **Material hierarchy** — crush white-clay façades; stone/paint/glass/metal separation + facade cards
3. **Cruiser** — deep navy clearcoat (not silver plate), glass IOR, env bands
4. **Reflections** — wet asphalt with roughness breakup (not full mirror)
5. **Night** — practical window lamps, controlled exposure, no black void
6. **Artifacts** — hide lamp poles / junk from beauty cams

### Soft (secondary)
- Orange barriers removed from frustums (Cycle-11 wiring retained)
- Soft stills are **not** the AAA score path

### Capture
```bash
SDL_VIDEODRIVER=dummy ./vaultline --soft --aaa-block-capture
blender -b -P /workspace/vaultline-blender/scripts/render_meridian_block_beauty_v12.py
```

### Honest note
Humans remain stylized vs photoreal AAA — Antonia continuous mesh is a large leap past sphere-salad, but not yet 8.0 alone. Materials/night/cruiser must land with them.
