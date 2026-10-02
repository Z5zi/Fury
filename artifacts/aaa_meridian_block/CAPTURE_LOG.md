# Cycle-21 capture log

- Branch: `aaa-meridian-block-v21` (Z5zi/Fury) from `4a9bb3f` (cycle 20); local commits only (box has no GitHub credentials)
- Blender: `/home/box/blender/blender` 4.2.8 LTS, Cycles CPU (8 threads), 128 samples, OpenImageDenoise, Filmic Medium High
  Contrast, 1280x720
- P0 cruiser wheels (C20 Gate B FAIL fix), `scripts/build_hmpd_wheels_c21.py`:
  - Opens `hmpd_cruiser_c20.blend`, deletes the C20 steel wheel + brake objects, builds original geometry at all four corners
    (hub z 0.322 m, track ±0.770 m, axle ±1.455 m; right-side assemblies mirrored so the dish faces outboard) and saves
    `assets/meshes/harbor_metro/hmpd_cruiser_c21/hmpd_cruiser_c21.blend` (collection `HMPD_Cruiser_C21`)
  - Wheel: 17x7.5 five-spoke alloy, 45 mm concave dish, crowned spokes with analytic custom normals, round-bevelled windows,
    machined lip, drop-centre barrel, 5 acorn lug nuts + washers in lug pockets (114.3 mm PCD), chrome centre cap + insert,
    valve stem
  - Tyre: P235/50R17 lathed carcass, 5-rib modelled tread blocks (bevel modifier), raised sidewall lettering
    ("HARBOR PURSUIT", size code), flattened contact patch with bulged lower sidewall, wear/grime/wet shading
  - Brakes: 320 mm ventilated rotor (two plates, 40 vanes, 24 cross-drilled holes, slots, radial anisotropic machining, rusty
    edge, hat), red two-piston caliper (piston bulges, bridge bolts), dust shield. ~212k tris per corner
  - Look-dev harness `scripts/preview_hmpd_wheel_c21.py` (isolated wheel turntable, not part of the pack)
  - Base cruiser body unchanged: "Police car" by Mateusz Woliński (Sketchfab `jeandiz`), **CC BY 4.0**,
    https://sketchfab.com/3d-models/9166b13b6ae341f4bfc093edb71d74f4 — attribution + C21 wheel note in
    `assets/meshes/harbor_metro/hmpd_cruiser_c21/LICENSE.md`
- New native wheel close-ups (DOF f/8): `03_cruiser_wheel_crop` (52 mm, near-lateral, front-left corner) and `03_cruiser_wheel_34`
  (50 mm, 3/4 view showing dish depth and rotor/caliper behind the spokes)
- Environment passes in `scripts/render_meridian_block_beauty_v21.py` (after the C20 bevel pass):
  - `c21_stone_ashlar`: bank + lobby ashlar stone box-projected at true scale (~0.5 m courses) so coursing/joints read
  - `c21_lobby_construction`: teller counter (toe-kick, marble panels with brass reveals, bullnosed marble top, glass teller
    fins), side-wall wainscot / chair rail / baseboard / crown, 600 mm polished stone floor tiles with grout (visible slab
    re-tiled by raycast), marble feature wall panel in a brass frame
  - `c21_skyline_articulation`: piers, sill ledges, plinths, setback crowns, roof plant and water tanks on skyline/background
    blocks (raycast guard keeps detail out of the lobby sight-lines)
  - `c21_night_floodlights`: 9 warm in-ground uplights grazing the bank stone between window bays; facade point fills halved
  - Small per-ped pose deltas on the C18 MPFB2 rigs (no new assets)
- Unchanged: C19 asphalt stack (Gate A), C18 MPFB2/MakeHuman CC0 peds (Gate C), cruiser body/livery from C20; `blender_peds/`
  proofs carried over unchanged
- Renders run one shot group at a time to stay inside box memory (logs `artifacts/aaa_meridian_block/logs/beauty_v21_c21_run*.log`,
  local only): day group 02–04 + all cruiser close-ups (2026-10-02 16:25–17:00 BST), night group 05 + 05_night_asphalt_near
  (17:00–17:15 BST), lobby 01 re-rendered alone after the marble vein fix (2026-10-02 ~17:30–17:50 BST, `BEAUTY_TILE=256`)
- Self-check before packing: viewed every beauty still (01, 02, 02_asphalt_near, 03 + five cruiser close-ups, 04, 04_medium, 05,
  05_night_asphalt_near) against the C20 set; no regressions found in asphalt, peds, cruiser body or night lighting.
  Known weak spots: alloy spokes read slightly bright/glossy; peds are still static idles; left/background midrise blocks are
  simple windowed massing; night 05 is closer to blue-hour than deep night
- Branding: Harbor Metro / HMPD / Meridian Mutual only; no Rockstar/GTA IP
- Timezone: Europe/London (BST)
- Packaged: 2026-10-02 BST via `scripts/pack_c21_evidence.sh`
