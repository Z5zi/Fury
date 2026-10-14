# Ofc. Vale graphical anatomy inputs

This directory describes Fury's original Vale recipe applied to pinned CC0
MPFB2 graphical data. It contains no MPFB add-on code, game-exported character,
commercial texture, copied reference mesh or third-party garment pattern.

- Upstream: https://github.com/makehumancommunity/mpfb2
- Exact source commit: `d0a32e57a7f915cb2f2b95410e2117648c7bbb7e`
- Asset license: CC0-1.0, as distinguished from add-on program logic by the
  unchanged `third_party/mpfb_assets/LICENSE.ASSETS.md` and `LICENSE.md`
- `MANIFEST.json` records all reused files' upstream paths, pinned URLs, sizes,
  SHA-256 and Git-blob SHA-1 hashes. Vale adds no duplicate raw target files.
- `vale_recipe.json` contains 17 explicit graphical target weights. The
  upstream target labels are parameter names, not demographic or biographical
  facts about this fictional character. No relation to another roster actor is
  implied by a surname, shared dataset or numerical comparison.

The existing base OBJ, metadata, licenses and numeric targets are referenced in
place from the shared, Jon, Tessa, Owen and Lia data directories. The head has
its own broad high-cheeked, tapered-jaw and projecting-chin recipe. It is not
Hale's or Renn's profile. The original seeded colors are independent of morph
weights and remain unchanged.

## Reproduce and verify

From the repository root, with Python 3:

```
python3 scripts/build_vale_anatomy.py --check
python3 scripts/build_vale_hands.py --check
python3 scripts/audit_vale_source.py
python3 scripts/audit_vale_hands.py
```

The first two commands independently regenerate expected data and compare it
with the shipped headers without rewriting those headers. The separate audit
reconstructs raw OBJ/target positions, exact source triangles and every UV
corner, then exercises malformed-input rejection. C++ runtime tests verify the
actual compiled geometry and its original-source correspondence.

The anatomical head retains all 4,343 selected source positions, all 8,628
triangles, 4,536 source UVs and the complete 56-edge lower attachment loop.
Nothing is removed from the face or ears for Far LOD. Both LODs attach a separate
under-neck surface to that complete loop. The source head is normalized with
one uniform scale, feet at -0.5 and bald crown at +0.489; the cap reaches the
exact actor top at +0.5. Runtime uses the actor's unchanged 1.86 m height.

Hands are connected source surfaces, fitted by an explicitly documented rigid
basis and a bounded static relaxed-finger shape. Near preserves all source
corners. Far uses Fury's original endpoint-quadric simplifier with a locked
20-edge wrist, manifold link condition and orientation checks. The result is
3,276 Near and 520 Far triangles per hand. No separate finger animation is
claimed. `GENERATED_HAND_STATS.json` includes the fit, topology and contour
checks; `GENERATED_STATS.json` records complete head provenance and landmarks.

The renderer integration, garment fit, motion and world evidence are described
in `docs/characters/vale/`. Reference images are private study inputs and are
excluded from shipped files and curated evidence. This is a bounded original
Fury art pass, not a claim of AAA or GTA VI parity.
