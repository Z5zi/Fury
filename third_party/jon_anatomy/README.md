# Jon Keel anatomical source data

This directory contains ten compressed numerical morph-target assets (426,471
bytes in total), from the MakeHuman Community's MPFB2 at immutable commit
`d0a32e57a7f915cb2f2b95410e2117648c7bbb7e`. They are graphical data under
**CC0 1.0 Universal**, not addon program logic.

The existing `third_party/mpfb_assets/base.obj`, mesh-group metadata, upstream
`LICENSE.md` and full `LICENSE.ASSETS.md` are referenced in `MANIFEST.json` with
their immutable URLs and SHA-256 values. Those shared files are not duplicated
or modified. Every new target also records its byte count and official Git blob
SHA-1, independently checked against the pinned upstream Git tree/content API.

`jon_recipe.json` is Fury's original shape recipe. Upstream asset names identify
parameters, not Jon's ethnicity, age in years, biography, or skin color. Palette,
hair, clothing, market placement, and the existing 1.70 m character height remain
the character profile's responsibility.

`scripts/build_jon_anatomy.py` uses Python's standard library and Fury's original
OBJ/math helpers from `scripts/build_mira_anatomy.py`. No MPFB Python, shell,
addon configuration, or other program logic was copied, imported, installed, or
executed. Neither Blender nor MPFB is needed.

From the repository root:

```sh
python3 scripts/build_jon_anatomy.py
python3 scripts/build_jon_anatomy.py --check
```

`--check` verifies all source hashes, topology, and exact generated header/report
bytes without writing. `GENERATED_STATS.json` includes source counts, morph
weights, measured eye/rig landmarks, geometry bounds, skin cross-sections,
topology checks, and the generated header SHA-256.

See [Jon's provenance and fitting notes](../../docs/characters/jon/ANATOMY_PROVENANCE.md).
