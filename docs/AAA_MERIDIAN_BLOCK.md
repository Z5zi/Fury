# AAA Meridian Block — ChatGPT Soft + Blender Beauty Benchmark

**Branding:** Harbor Metro / HMPD / Meridian Mutual only — no Rockstar/GTA IP.

## Cycle-11 strategy shift
Cycle-10 soft-only scored **1.8/10 NO-SHIP**. Soft cannot hit AAA photoreal soon.
Cycle-11 therefore:

| Role | Path | Purpose |
|------|------|---------|
| **PRIMARY** | `artifacts/aaa_meridian_block/blender_scene_beauty/` | ChatGPT AAA bar (Cycles) |
| **SECONDARY** | `artifacts/aaa_meridian_block/0{1-5}_*.png` | Soft runtime wiring proof |

### Soft (secondary)
- Orange `AaaBarrier` row **removed**; HarborCone/Barrel/Crate/DepotCone/ExtractCone/Barrier/TrafficCone hidden in aaa-block AABB
- Wet helper cards recolored dark (no tan/orange cards)
- Filmic 0% white clip retained; midtones kept
- Soft stills are **not** the AAA score path

### Blender beauty (primary)
Script: `/workspace/vaultline-blender/scripts/render_meridian_block_beauty_v11.py`
- Imports annex / storefront / midrise / HMPD cruiser v12b / 5 peds
- Wet asphalt + clearcoat paint force + glass/stone/metal cards
- Day + night cameras aligned to soft heroes

### Capture
```bash
SDL_VIDEODRIVER=dummy ./vaultline --soft --aaa-block-capture
# beauty
blender -b -P /workspace/vaultline-blender/scripts/render_meridian_block_beauty_v11.py
```

### Honest note
If Blender beauty is still weak vs AAA bar, say so — do not claim 8.0 from soft polish alone.
