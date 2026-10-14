# Owen Pike graphical anatomy data

This directory contains seven CC0 numeric MPFB morph targets, a separate Owen
recipe, a complete pinned-input manifest, generated statistics and an audit
record. Shared base/metadata/license inputs and five existing targets remain
in their original directories and are referenced without modification.

Upstream: https://github.com/makehumancommunity/mpfb2

Pinned commit: `d0a32e57a7f915cb2f2b95410e2117648c7bbb7e`

The shared [license boundary](../mpfb_assets/LICENSE.md) and
[CC0 asset license](../mpfb_assets/LICENSE.ASSETS.md) cover graphical data.
No upstream GPL addon program code is included or executed. The target labels
identify shape controls, not Owen's ethnicity, literal age or biography.

Generate with `python3 scripts/build_owen_anatomy.py` from the repository root;
verify with `python3 scripts/build_owen_anatomy.py --check`. The importer uses
only Python's standard library. See the complete
[provenance and fitting report](../../docs/characters/owen/ANATOMY_PROVENANCE.md).

`GENERATED_STATS.json` includes ordered source eye/mouth/neck seams, exact
attachment positions and closed/open contour counts. In particular, the
neck-chart sections at Y 0.34–0.355 are complete closed contours; higher and
lower listed sections may be partial surfaces. `VALIDATION.json` records the
39 negative-input checks, source-corner/UV correspondence, cutoff evaluation,
shape comparisons and C++17 header verification for this authored recipe.
