# Sgt. Hale graphical source provenance

`NpcGuard` / Security / seed `6ef263f5` / nominal height 1.85 m. The only imported graphical data is the existing MakeHuman/MPFB base mesh, numeric targets and vertex metadata, pinned to commit `d0a32e57a7f915cb2f2b95410e2117648c7bbb7e`. The existing LICENSE.md explicitly separates GPLv3 addon program logic from CC0 graphical assets. LICENSE.ASSETS.md contains the CC0 dedication. No addon program code was copied, installed, imported or run.

The dedicated [manifest](../../../third_party/hale_anatomy/MANIFEST.json) references every input in place and records a pinned URL, byte length, Git blob SHA-1 and SHA-256. The original [recipe](../../../third_party/hale_anatomy/hale_recipe.json) selects fourteen shape targets, preserving the seeded complexion/hair palette independently. Target filename labels name graphical controls; they establish no biography, age, ethnicity or gender identity.

Fury's own source importer retains all 8,720 selected head/neck triangles, 4,390 positions and 4,584 UVs. It retains source position, corner UV, face and fan-triangle IDs, a connected attachment loop of 58 edges, and the five canonical Face charts. Original UV coordinates are not replaced with cylindrical or planar projections. The selected upper-body cutoff is 0.33 height units and every selected triangle is retained; no anatomical source triangle is silently deleted to meet the budget.

The separate hand importer reuses Fury's own first-party connected-source method with Hale's recipe and a distinct static rest curl: a 0.012-unit distal lift and 0.024-unit curl. Its positive minimum longitudinal derivative is 0.775. One original connected hand is mirrored with reversed winding for the other side, with its source UV corners retained. Near retains 3,276 hand triangles per side; Far retains 520 through a documented endpoint reduction with locked 20-edge wrist and four finger-gap planes. The importer tests manifold links, winding, connectivity and finger sections. The hand has no finger animation.

The first-party hand and sewn garment construction utilities were inspected read-only in the private Pax study and copied into Hale-owned source files. Hale's independent dimensions, source recipe, uniform/cap construction and test expectations are recorded here; that reuse is not an acceptance claim for the other study.

The cap, brim, closed vest, shirt pattern, pockets, radio, belt and shoes are original mathematical surface construction. Facial movement remains the existing body/head attention rig; there is no facial blendshape, lip-sync or finger controller.

## Private visual references

The actual pixels of Rockstar's official Jason Duval 03 and 04 screenshots were inspected for connected facial forms, hand proportions, fabric layering and hair growth; Guillaume Ds's Ubisoft security-guard portfolio images were inspected for layer thickness and equipment attachment. Reference images remain private and are excluded from source/recovery deliverables. Proprietary likenesses, helmets, logos, insignia, mesh and textures are not reproduced. There is no GTA VI parity claim.

- [Rockstar official screenshot index](https://www.rockstargames.com/VI/media/screenshots)
- [Guillaume Ds, Security Guard / Beyond Good and Evil 2](https://guillaumeds.artstation.com/projects/g8Z4XG)
- [Pinned upstream license split](https://github.com/makehumancommunity/mpfb2/blob/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/LICENSE.md)

## Reproduction

Run `python3 scripts/build_hale_anatomy.py --check` and `python3 scripts/build_hale_hands.py --check` from the source tree. Omitting `--check` regenerates the same header and numerical report bytes. No network access, Blender, MPFB program, nonstandard Python package or proprietary asset is required.
