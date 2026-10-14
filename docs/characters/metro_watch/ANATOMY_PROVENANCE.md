# Metro Watch anatomy and asset provenance

Only original Fury code and pinned CC0 MPFB graphical data are used in the
production profile. No Rockstar/IO mesh, texture, code, logo, likeness or other
proprietary asset is imported. Private reference images are not redistributed.
The source commit for graphical data is
`d0a32e57a7f915cb2f2b95410e2117648c7bbb7e` from
[makehumancommunity/mpfb2](https://github.com/makehumancommunity/mpfb2/tree/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e).
The repository's graphical-data CC0 license is separate from its program-code
license; no MPFB program logic is copied or run.

Actor-specific inputs and generated statistics are in
`third_party/metro_watch_anatomy/`. MANIFEST.json pins every graphical input by
commit URL, SHA-256, byte count and Git blob identity, including shared data
already present in the accepted Fury tree. The actor owns a distinct recipe,
not another guard's unchanged face. The original importer composes the numeric
targets, applies the recorded normalized fit and retains every selected Face
triangle/corner. The complete head has **4,358 positions and 8,660 triangles**,
including the anatomical eyelids, mouth and ears, and one **54-edge** lower
neck attachment cycle. Both LODs preserve that entire ordered Face triangle
stream and source UVs. The graphical parameter names establish no demographic
identity, biography or real-person likeness.

The connected hands retain **3,276 source triangles per hand Near** and **520
per hand Far**. Fury's original endpoint-quadric reduction preserves a single
connected manifold with the locked 20-edge wrist and no extra hole. The fixed
relaxed curl and mirrored right-hand winding are recorded in the generated
hand statistics. These are connected fingers, not independent primitives;
there is no per-finger animation controller.

Production clothing, cap, visor, carrier, radio, blank badge, belt, pouch and
footwear are original code-built surfaces. The carrier panels/straps are cut
from the actual shirt support triangles with matched skin weights, not copied
from a commercial garment. Shared MPFB data and original Fury construction
helpers are reused; no guard geometry is reused unchanged as this face.

Reproduction:

- `python scripts/build_metro_watch_anatomy.py --check`
- `python scripts/build_metro_watch_hands.py --check`

The final report records the exact verification results and source hashes.
