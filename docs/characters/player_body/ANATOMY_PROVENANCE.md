# PlayerBody anatomy provenance

The fixed `PlayerBody` presentation uses only original Fury integration code and
already-vendored CC0 MPFB graphical data pinned at
`d0a32e57a7f915cb2f2b95410e2117648c7bbb7e`. No MPFB addon code was installed,
imported or executed. Rockstar and professional reference images informed form
and construction only and are not copied into source or delivered assets.

`third_party/player_body_anatomy/MANIFEST.json` pins all 22 shared input files by
immutable upstream URL, byte count, SHA-256 and Git blob hash. Its 18-control
recipe independently authors a broad oval head, defined jaw turn and chin,
shaped cheek planes and moderate nose bridge. Numeric source parameter names
are not new biography, age, ethnicity or gender identity. Palette remains the
original `PlayerBody` appearance seed `724a6d0c`, independent of these controls.

The original Fury standard-library importer applies each displacement before
one uniform full-body normalization to feet −0.5 and scalp crown +0.493. Hair
uses the remaining +0.007. It retains every source triangle selected above the
actor's documented 0.32 neck/shoulder attachment cutoff, every canonical source
position, UV corner, face ID and triangle ID. There is one connected head
surface, with 4,466 positions, 8,872 triangles and a continuous attachment loop.
The source anatomical topology is retained intact in both LODs.

The complete external-head, internal-mouth and eye-socket chart union defines
4,125 Head-rigid source positions, including all 418 mouth positions and all
exterior jaw/chin points. Shared seam aliases receive the same weights. Neck
interiors blend from this actor's neck landmark to its actual angular head/neck
source seam. This avoids the visible jaw/mouth shear caused by generic Y-only
weights. The collar fits this source neck surface and follows interpolated
source weights. Source face UVs remain canonical; own lip, eyelid and nose tint
placement is fitted to actual source features and checked in rendered pixels.

Hands retain 1,649 source positions and all 3,276 Near triangles each. Their
independent wrist frame uses the source wrist and finger-root landmarks. A
monotonic static rest curl preserves connectivity; it is not finger animation.
The chosen wrist cut lies inside an actor-specific stable interval, preserving
one 20-edge attachment cycle. The source hand is rigidly fitted at 68 degrees
and mirrored with corrected winding. The original endpoint reduction retains
560 Far triangles per hand, preserves source endpoints and the full wrist,
and checks manifold links, triangle orientation and finger cross-sections.

Reproduce and check from repository root:

    python3 scripts/build_player_body_anatomy.py --check
    python3 scripts/build_player_body_hands.py --check
    python3 tests/player_body_anatomy_source_tests.py

The independent audit imports neither producer: it reconstructs the raw OBJ,
target blends, normalization, source/corner selection, hand frame and curl.
Eight corruption controls cover positions, winding, UVs, topology, and dropped
mouth/exterior-head rigidity. Runtime tests separately verify exact source
positions and corners, shared normals and weights, 17 rig semantics, full
jaw/mouth Head motion, finite geometry, both LOD budgets and stable posing.
