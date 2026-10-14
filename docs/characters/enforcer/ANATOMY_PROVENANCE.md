# Syndicate Enforcer anatomical provenance

Actor `NpcEnforcer`, 1.95 m, Enforcer appearance role, exact stable seed
`0xbb361ac4` (3140885188). Original appearance is always
`character_appearance(character_seed("NpcEnforcer"))`; skin, chestnut hair, eye,
stored shirt/trouser, shoe, accent and body-choice values are retained. As in
the original generic Enforcer, rendered uniform colors override the stored
shirt/trouser fields: shirt (0.16,0.22,0.28), trousers (0.095,0.13,0.175).
The original dark sole (0.055,0.065,0.075) and stitching accent are retained. Role colors on
the protective vest/pads and the original accent inserts remain separately
visible. Shape recipes are original graphical choices, never claims of a real
person, ethnicity, gender, age or story.

## Licensed graphical data

All imported anatomy is the pinned MakeHuman Community MPFB graphical data at
commit `d0a32e57a7f915cb2f2b95410e2117648c7bbb7e`.
https://github.com/makehumancommunity/mpfb2/commit/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e

The upstream license distinguishes CC0 base meshes, targets/modifiers, rigs and
JSON mesh data from GPL addon program logic. Only graphical data is used.
No addon code is copied, adapted, installed or executed. Exact unchanged license
documents remain in `third_party/mpfb_assets/`. The individual manifest at
`third_party/enforcer_anatomy/MANIFEST.json` pins 20 inputs by immutable
URL, SHA-256, byte length and Git-blob identity. The importer checks all four.
No proprietary reference images, textures, meshes, brands or patterns are
shipping inputs.

## Individual face

`enforcer_recipe.json` records the distinct broad temple/jaw, restrained squared
chin, broad straight nose, moderate chin projection, quiet brow and substantial
neck. The source mesh and its targets are blended first. One uniform scale
0.056936702212446705 retains original proportions, with full-body source feet at
-0.5 and scalp crown at +0.493. The existing 0.007 height reserve fits hair.
The exported face/neck retains 4403 positions and 8748
ordered source triangles; zero selected triangles are removed. Every canonical
UV corner and source vertex/face index is retained. The neck attachment is one
56-edge continuous loop. Both LODs retain the complete face.

The internal mouth/jaw helpers are not visible lip positions. This source's
exact 38-vertex lip seam has Y bounds 0.39072916–0.39292236. Its actual head/neck
chart seam has 48 vertices and rises from Y=0.36358174 at the chin to
0.37948530 at the center nape. The neck blend follows a smooth lower envelope
clamped below this actual closed contour. A global horizontal cutoff had
over-constrained true posterior neck vertices and caused a visible bulge.
All non-neck source charts and both copies of the complete seam remain rigid
to Head. Tests independently recover their ownership from canonical UV charts,
check all protected vertices in nine extreme attention samples, and deliberately
assign a low jaw vertex and a mouth vertex to Neck as corruption controls.
The exact 38-vertex lip loop also retains the historical bad-weight control.
Every imported position, triangle and UV is unchanged. The artist-fitted Head
pivot is 35% source neck helper / 65% source head helper, placing rotation at
the cranio-cervical attachment; the shared animation/controller is unchanged.
These are this actor's source measurements, not copied facial thresholds.

## Hands and scalp

Hands preserve every selected original Near source corner and wrist loop, with
an original static relaxed rest curl and a rigid palm orientation of ±1.16 rad.
They attach through the exact 20-edge source wrist loop. Near retains
3276 triangles per hand; Far uses 520 endpoint-reduced
triangles, locking the wrist, rejecting triangle reversal/nonmanifold collapses,
and retaining source-position corners and tested finger-section topology.
The left/right counterpart is mirrored with reversed winding. There is no new
finger controller, expression system or animation behavior.

The scalp is clipped from this actor's actual morphed source head rather than
an approximate cap. Original source normals support a 0.0024–0.007 normalized
hair offset. Hairline and crown are locked during original Far endpoint reduction.
The scalp has 502 positions, 850 Near and
400 Far triangles. Original source-face/barycentric correspondence
is saved in `GENERATED_HAIR_STATS.json`. The tied bun and matte tie are authored
separate fitted geometry, not copied assets or a simulated strand groom.

## Reproduce

Run `python3 scripts/build_enforcer_anatomy.py --check`,
`python3 scripts/build_enforcer_hands.py --check`, and
`python3 scripts/build_enforcer_hair.py --check` from this private study. Each
checks pinned inputs and compares complete generated output byte-for-byte.
Geometry generation is separate from active pixel, contact, renderer and
actual-game lifecycle review; reconstruction alone is not visual acceptance.
