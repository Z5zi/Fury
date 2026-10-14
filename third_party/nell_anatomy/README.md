# Nell Ash anatomical graphical data

The character has an independent recipe over existing pinned MakeHuman/MPFB2
CC0 graphical assets. MANIFEST.json verifies immutable URLs, sizes, SHA256 and
Git blob hashes. Shared originals, source metadata and original license files
are referenced in place; no upstream addon program logic is copied or run.

- `nell_recipe.json`: original Nell shape; graphical target labels establish
  no biography, age, ethnicity, gender identity or palette
- `GENERATED_STATS.json`: head/neck topology, original source vertex/UV/face IDs,
  exact source eye-opening seams, neck sections and50-edge attachment cycle
- `GENERATED_HAND_STATS.json`: connected source hand selection, source frame,
  explicit static relaxation, original/retained topology and finger sections

Fury's own standard-library scripts reproduce both generated C++ headers:

    python3 scripts/build_nell_anatomy.py --check
    python3 scripts/build_nell_hands.py --check

Near hands preserve every selected source triangle, corner UV and boundary
winding. Source face9027 contains a sharply concave thumbnail-fold triangle;
it receives separate rendering normal corners instead of averaging through
the fold. No source triangle is removed to hide it. The source hand is fitted
rigidly and mirrored with reversed winding for the other hand. Static rest
curl/compression is explicit in the report; this adds no finger animation.

Far uses Fury's original endpoint-quadric reduction. It keeps source vertex
positions after that explicit rest shaping, locks the entire20-edge wrist,
checks the manifold link condition and rejects triangle inversions. Both
LODs have one connected component, one wrist opening and Euler characteristic1.
Four independent cross sections retain matching finger-contour counts. This
is a bounded graphical reduction, not a claim of perfect shape equivalence.

The head preserves every selected source position, triangle and per-corner UV
at both LODs. Eyes are fitted to Nell's actual source sockets; the mouth joint
is internal and is never treated as a lipstick placement point. The connected
neck underlay reuses the complete source attachment loop.
