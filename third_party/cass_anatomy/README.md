# Cass anatomical graphical data

`MANIFEST.json` pins the source repository commit, CC0 asset license, original
URLs, byte counts, SHA-256 and Git blob identities for every input. Existing
vendored data is referenced in place, avoiding redundant copies. No MPFB addon
program logic is copied, installed or executed.

`cass_recipe.json` is a separate original graphical recipe. It does not assign
an ethnicity, literal age, gender identity or biography to Cass. The standing
skin/hair palette remains the stable game's `NpcFence` appearance.

`scripts/build_cass_anatomy.py` retains source anatomical positions, triangle
winding and per-corner UV correspondence after the documented target mixture
and uniform height normalization. `scripts/build_cass_hands.py` applies the
same source mixture and a documented static rest fit. Far hand simplification
locks the wrist, preserves manifold topology and checks finger-plane gaps.

Both generators support `--check` for exact-source reproducibility. Source
licenses remain in `third_party/mpfb_assets/LICENSE.md` and
`third_party/mpfb_assets/LICENSE.ASSETS.md`. Proprietary study imagery is not
included and grants no asset reuse rights.
