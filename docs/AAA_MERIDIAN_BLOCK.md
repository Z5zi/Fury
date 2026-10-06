# AAA Meridian Block — ChatGPT Soft + Blender Beauty Benchmark

**Branding:** Harbor Metro / HMPD / Meridian Mutual only — no Rockstar/GTA IP.

## Cycle-13 strategy
Cycle-12 Blender beauty scored **3.6/10 NO-SHIP**. Ceiling: asphalt mirror, cruiser blockout hardware, ped toys at hero distance, white-clay architecture.

| Role | Path | Purpose |
|------|------|---------|
| **PRIMARY** | `artifacts/aaa_meridian_block/blender_scene_beauty/` | ChatGPT AAA bar (Cycles) |
| **SECONDARY** | `artifacts/aaa_meridian_block/0{1-5}_*.png` | Soft runtime wiring proof |

### Cycle-13 beauty priorities (ruthless P0)
1. **Wet asphalt gate** — aggregate, wet/dry, puddle boundaries, tire paths; reflection localized (NOT mirror)
2. **Cruiser complete** — rim/tire/rotor wheels + lightbar/pushbar/mirrors; keep navy clearcoat; remove side grime/cage junk
3. **Peds at hero distance** — all Antonia.Polygon CC0 (no Quaternius); closer 04 camera; body-face clothing
4. **Architecture materials** — stone/paint/glass/metal + frames + weathering (crush white clay)
5. **Night** — denser practicals; asphalt not chrome
6. **Junk** — ash banners / wires / cage hidden

### Soft (secondary)
- Orange barriers removed from frustums (Cycle-11 wiring retained)
- Soft stills are **not** the AAA score path

### Capture
```bash
SDL_VIDEODRIVER=dummy ./vaultline --soft --aaa-block-capture
blender -b -P /workspace/Fury/scripts/render_meridian_block_beauty_v13.py
```

### Honest note
Still below photoreal AAA bar likely: Antonia remains stylized; clothing is material-banded not tailored garments; night density limited. Expect mid-band improvement over 3.6 if asphalt/cruiser/arch gates land visually.
