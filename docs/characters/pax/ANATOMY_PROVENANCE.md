# Pax Wren source and construction

Pax is `NpcCivD`, role Dock, height 1.72 m, stable seed `ec850fd2`.
The original broad waist, narrower shoulders/face, teal shirt, dark olive
trousers, orange work vest, dark footwear and gray tied rear bun remain the
identity constraints. Exact linear colors are recorded in `PALETTE.json`.
There is no new age, ancestry, gender biography, employer, real logo or prop.

## Licensed graphical source

The head and hands use Fury's existing pinned CC0 MakeHuman/MPFB graphical
data, upstream commit `d0a32e57a7f915cb2f2b95410e2117648c7bbb7e`.
`third_party/pax_anatomy/MANIFEST.json` pins each input by immutable URL,
SHA-256, byte count and Git blob ID. Both importers verify these inputs.
No upstream addon program is installed or executed. The source recipe is
Pax's own combination, with independently measured eye, neck and wrist fit.
The named macro controls describe graphical data, not a biography.

The head retains all 4,395 source vertices and 8,732 triangles in both LODs,
including original source identifiers, per-corner UVs and attachment loops.
The source neckline continues into the shirt with blended chest/neck/head
weights. Eye placement is checked against the actual extracted apertures.

The hand extractor follows Fury's original connected-source method first
validated for Nell, refitted to Pax's morphed mesh. Each hand has 3,276 Near
triangles and 520 Far triangles. Near retains the selected original source
corners and UVs. An invertible static curl/compression forms the relaxed rest
pose. Far endpoint-quadric reduction locks the complete 20-edge wrist,
checks the manifold link condition and face inversions, and preserves
representative finger-contour sections. There is no finger animation.
The exact cut loop is continued into an independently shaped forearm.
The mirrored hand reverses winding; it does not duplicate inside-out faces.

## Original fitted layers

The shirt has a continuous torso/yoke, sewn sleeve roots, rolled cuffs and a
bound neck. Its fuller waist is authored independently of previous actors.
The vest is a closed thin shell fitted to actual shirt support triangles,
with its own V opening, curved hem, generous armholes and lining.
The four reflective-looking strips are finite closed shells in both LODs;
they follow the supporting triangle partition and influence correspondence.
They retain the same tint and existing Shirt atlas region in Near and Far.
There is no new reflective shader or material-map allocation.
The center underlap and raised closure rails supply real layering.

Pants have a fuller seat and shaped knees/ankles. The original left cargo
role detail is a fitted pouch and turned flap. The footwear has a toe box,
instep, sole edge and fitted laces. The opaque scalp base, gathered cards and
rear bun preserve the original grayscale hair identity. These are static
meshes deformed by Fury's existing character rig; cloth simulation is absent.

## Reproduction

Run from the repository root:

    python3 scripts/build_pax_anatomy.py --check
    python3 scripts/build_pax_hands.py --check

The checked generated headers are deterministic. The final runtime budget is
38,752 Near triangles / 25,375 vertices and 15,940 Far triangles / 10,497
vertices, inside the unchanged 40,000 / 16,000 limits. The shared immutable
material atlas still occupies 37,746,432 bytes. No new atlas is allocated.
