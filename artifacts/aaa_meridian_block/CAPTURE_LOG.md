# Cycle-20 capture log

- Branch: `aaa-meridian-block-v20` (Z5zi/Fury) from `999c9cb` (cycle 19); local commits only (box has no GitHub credentials)
- Blender: `/home/box/blender/blender` 4.2.8 LTS, Cycles CPU (8 threads), OpenImageDenoise, Filmic Medium High Contrast, 1280x720
- P0 HMPD cruiser (critique fix #1):
  - Base mesh: "Police car" by Mateusz Woliński (Sketchfab `jeandiz`), **CC BY 4.0**,
    https://sketchfab.com/3d-models/9166b13b6ae341f4bfc093edb71d74f4 (via the Objaverse mirror). Attribution + list of
    modifications in `assets/meshes/harbor_metro/hmpd_cruiser_c20/LICENSE.md`
  - `scripts/gen_hmpd_cruiser_livery_c20.py`: original POLICE/705/911 markings and grille badge painted out, body re-coloured
    Harbor Metro navy, lamp-lens mask, HMPD decals (door script + "HARBOR METRO POLICE" + gold pinstripe, unit 27 roof/quarter,
    hood crest), lettered tyre sidewall ("HARBOR PURSUIT P235/55R17"), HM 2727 plate — all original artwork
  - `scripts/build_hmpd_cruiser_c20.py` → `assets/meshes/harbor_metro/hmpd_cruiser_c20/hmpd_cruiser_c20.blend`: real-world scale
    (2.91 m wheelbase), clear-coat paint, object-space projection decals, rocker grime, transmissive glass + textured interior,
    lens emission at night, red/blue lightbar, steel wheels + brake discs, push bar, mirrors/handles
  - `place_cruiser_c20` (beauty script): appended at the C19 hero spot; night head/tail/lightbar spill lights
  - Native close-ups (DOF f/5.6): `03_cruiser_front_detail` (45 mm), `03_cruiser_side_detail` (32 mm), `03_cruiser_rear` (35 mm),
    `03_cruiser_wheel_crop` (50 mm). The C19 upscaled hood/door/glass pixel crops are retired
- Night lighting / reflections (fix #5): deep-blue zenith → warm city-glow horizon world, distant lit skyline, room-by-room lit bank
  windows with spill, cobra-head lamp luminaires; `scripts/gen_road_masks_v20.py` (= v19 + 11 birdbath dips at lamp / lit-window /
  lightbar mirror points for the 05 cameras) → `vaultline-blender/refs/textures/generated/c20_road/`; night fill reduced
- Architecture (fix #2): `add_bank_windows_c20` replaces the 17 blind window slabs with stone surround + sill/drip, bronze frame,
  mullion/transom, reflective glass and a room card (blinds, ceiling falloff, lit rooms by day and night); C14 background blocks
  lose their flat emissive strip cards and get a windowed facade; `add_skyline_c20` distant massing on every horizon;
  `c20_bevel_pass` bevels 120+ legacy box primitives
- Lobby (fix #3): Poly Haven CC0 armchairs, lounge chairs, coffee tables, plants, counter stationery, CCTV, pendant lamps, wall clock,
  fire alarm, wet-floor sign; steel queue stanchions with navy belts, entrance mat, digital rates board + framed brand poster
  (`scripts/gen_lobby_boards_c20.py`, original artwork); C16 floating window-frame slabs inside the lobby removed
- Street density (fix #4): Poly Haven CC0 hydrants, bins, utility cabinets, modular seating, planters, rubbish bags, boxes,
  facade AC units, security lights (`dress_street_c20`)
- Poly Haven models: CC0 1.0 (https://polyhaven.com/license), 1k glTF, list + fetch script in
  `vaultline-blender/refs/polyhaven_models/` (LICENSE.md, fetch.py)
- Unchanged: C19 asphalt stack (Gate A), C18 MPFB2/MakeHuman CC0 peds; `blender_peds/` proofs carried over unchanged
- Renders run per shot group to stay inside box memory (logs `artifacts/aaa_meridian_block/logs/beauty_v20_c20_run*.log`, local only):
  final 05 + 05_night_asphalt_near from runD (21:03–21:05 BST), all day stills 01–04 + cruiser close-ups from runE (21:08–21:31 BST)
- Self-check before packing: viewed every beauty still (01, 02, 02_asphalt_near, 03 + four cruiser close-ups, 04, 04_medium, 05,
  05_night_asphalt_near). Known weak spots: lobby shell/counter are still procedural boxes with PBR materials; bank stone
  massing is the C10 kit; skyline is simple massing; cruiser base is a 2019 sedan mesh (not a scan); peds static idle
- Branding: Harbor Metro / HMPD / Meridian Mutual only; no Rockstar/GTA IP
- Timezone: Europe/London (BST)
- Packaged: 2026-10-01 BST via `scripts/pack_c20_evidence.sh`
