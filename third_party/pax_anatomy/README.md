# Pax Wren anatomical source

Pax's original Fury recipe references existing pinned CC0 graphical data from
MakeHuman/MPFB commit `d0a32e57a7f915cb2f2b95410e2117648c7bbb7e`.
No upstream addon program logic is copied, installed or executed.
The manifest pins every shared input by immutable URL, SHA-256, byte length and
Git blob hash; the importer independently verifies all four identifiers.

- `pax_recipe.json`: separate head/neck shape controls, not a biography or a
  renamed/scaled copy of another actor
- `GENERATED_STATS.json`: actual source vertex/triangle/corner UV correspondence,
  connected head/neck topology, attachment boundary, eye aperture seams and fit
- `GENERATED_HAND_STATS.json`: the same morphed source's connected hand, a
  documented invertible static rest curl, 20-edge wrist, Near/Far topology and
  preserved representative finger sections. Finger animation is not implemented

Regenerate/check from the repository root:

    python3 scripts/build_pax_anatomy.py --check
    python3 scripts/build_pax_hands.py --check

The source labels on graphical controls do not assign an age, ethnicity,
gender identity or skin palette. The actual complexion, gray hair and wardrobe
colors continue to come from Fury's unchanged stable seed `ec850fd2`.
