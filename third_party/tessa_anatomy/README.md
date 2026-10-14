# Tessa Quill anatomical source data

This directory vendors 19 compressed numeric morph targets (443,444 bytes)
from MakeHuman Community MPFB2 at immutable commit
`d0a32e57a7f915cb2f2b95410e2117648c7bbb7e`, under CC0 1.0 Universal.
No upstream program logic is included.

`MANIFEST.json` pins immutable URLs, SHA-256, byte counts and official Git blob
IDs. Eleven existing base/metadata/license/target files are referenced and
verified in place without duplication. The unchanged complete CC0 license is
in `../mpfb_assets/LICENSE.ASSETS.md`.

`tessa_recipe.json` is Fury's original mature adult feminine-presenting shape
recipe. The source names identify parameters only; they do not establish
Tessa's age, ethnicity, biography or palette. Her stable identity is `NpcCivC`,
Commuter, 1.65 m, with gray short hair and the existing ochre/brown clothing.

From the repository root:

```sh
python3 scripts/build_tessa_anatomy.py
python3 scripts/build_tessa_anatomy.py --check
```

The importer is Fury's own standard-library implementation, using the existing
original OBJ/math helpers. No MPFB addon, Python logic, shell or configuration
is copied or executed. Neither Blender nor MPFB is needed.

`GENERATED_STATS.json` records the exact recipe, topology, source normalization,
landmarks, skin cross-sections, actual eye/mouth surface-seam source IDs,
five-chart UV validation and generated-header SHA-256.

See [Tessa's provenance and fitting notes](../../docs/characters/tessa/ANATOMY_PROVENANCE.md).
