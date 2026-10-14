# MPFB graphical source data for Mira

This directory vendors **data only**, from the MakeHuman Community's MPFB2
repository at commit `d0a32e57a7f915cb2f2b95410e2117648c7bbb7e`.
The base mesh, targets, and JSON mesh information are explicitly covered by
the upstream **CC0 1.0 Universal** asset license. `LICENSE.md` describes the
boundary between CC0 graphical assets and GPLv3 addon program logic;
`LICENSE.ASSETS.md` contains the complete CC0 text. Both are unchanged copies.
No addon Python, addon configuration/program logic, or Blender dependency is
included, installed, or executed.

`MANIFEST.json` contains the immutable URL and SHA-256 of every vendored source.
`mira_recipe.json` is Fury's reviewable geometry recipe, independent of skin
color. `GENERATED_STATS.json` records the exact output counts, landmarks,
normalization, topology checks, and generated-header hash.

To rebuild with Fury's original standard-library-only importer:

```sh
python3 scripts/build_mira_anatomy.py
python3 scripts/build_mira_anatomy.py --check
```

Only the generated head/neck data is included in Fury's runtime. The source
full-body OBJ and its helpers remain here for reproducibility. The importer
uses the morphed full body's height for a uniform normalization, extracts joint
landmarks, then retains only body faces above the configured neck cutoff.
No upstream meshes or helpers are loaded at runtime.

See [Mira anatomy provenance](../../docs/characters/mira/ANATOMY_PROVENANCE.md)
for the recipe, coordinate conventions, integration caveats, and validation.
