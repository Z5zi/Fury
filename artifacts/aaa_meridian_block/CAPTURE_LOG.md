# Cycle-19 capture log

- Branch: `aaa-meridian-block-v19` (Z5zi/Fury) from `336a7fc` (cycle 18); local commits only (box has no GitHub credentials)
- Blender: `/home/box/blender/blender` 4.2.8 LTS, Cycles CPU (8 threads), OpenImageDenoise, Filmic Medium High Contrast
- P0 asphalt:
  - `scripts/gen_road_masks_v19.py` (numpy/PIL, deterministic seed 1919) → `vaultline-blender/refs/textures/generated/c19_road/`
    (16-bit relief height + 4 RGB masks, 5333x1933 = 1.5 cm/texel over Blender X[-15,65] Y[-27,2]; 0.2 m crown grid)
  - `build_road_c19` / `make_road_material_c19` in `scripts/render_meridian_block_beauty_v19.py`: crowned carriageway mesh
    (5 cm crown), granite kerbs, pavements on both sides, recessed drain grates; C16 flat plane, disc puddles and lane cubes removed
  - Poly Haven CC0 textures (2k, true-to-scale tiling): asphalt_02 (x2 anti-tiled), asphalt_04, asphalt_track,
    concrete_floor_worn_02, metal_grate_rusty (downloaded via api.polyhaven.com)
  - Water only in gutters / ruts / dips (~9% of road area), clear-coat water film (IOR 1.33) with ragged edges + ripple normal;
    rest damp (darker, crevice-wet, rough aggregate peaks)
- Scene: lamp poles moved onto pavements (+ downward pools at night), 3 kerbside parked civ cars (hm_civ_sedan/van/hatch_v1, lamps off),
  background block moved out of the carriageway, faint urban sky glow at night
- Cameras: 01/02/04/04_medium/05 unchanged from C18; 03 reframed to a 35 mm 3/4 front hero; NEW native close-ups
  `02_asphalt_near` (32 mm, f/8), `03_cruiser_wheel_crop` (50 mm, native — no longer a crop), `05_night_asphalt_near` (35 mm)
- Peds: C18 MPFB2/MakeHuman CC0 `.blend` peds appended unchanged; `blender_peds/` proofs carried over from C18 unchanged
- Full run ~16 min (17:52–18:08 BST) (log `artifacts/aaa_meridian_block/logs/beauty_v19_c19.log`); night re-render with night puddle grade
  (`beauty_v19_c19_night_rerender.log`)
- Self-check before packing: viewed 01, 02, 02_asphalt_near(+crop), 03, 03 wheel, 04 (identical to C18), 05, 05_night_asphalt_near.
  Known weak spots: night puddles mirror a dark sky and read very dark; cruiser body geometry still low-fidelity; lobby unchanged
- Branding: Harbor Metro / HMPD / Meridian Mutual only; no Rockstar/GTA IP
- Timezone: Europe/London (BST)
- Packaged: 2026-09-30 18:32 BST
