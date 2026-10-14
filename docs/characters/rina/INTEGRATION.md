# Integrate Rina's individual pass

The private study starts from tree `9e21e9a1aae79fb7abd7a9cf3e177349581ef14d`.
Do not replace the main project's working tree with this complete older checkout.
The main project integrates changes serially and retains all publication decisions.

## Actor-owned paths

Copy only these new paths from the deliverable's `owned/` directory:

- `engine/src/character_rina.cpp`, `character_rina_shirt.hpp`, `character_rina_vest.hpp`
- `engine/include/fury/generated/rina_anatomy.hpp`, `rina_hands.hpp`
- `scripts/build_rina_anatomy.py`, `build_rina_hands.py`
- `tests/character_rina_tests.cpp`, `character_rina_contact_tests.cpp`,
  `character_rina_renderer_tests.cpp`, `rina_anatomy_source_tests.py`
- `third_party/rina_anatomy/*`
- `docs/characters/rina/*`

The raw MPFB graphical inputs are already in the exact baseline. Retain the
shared `third_party/mpfb_assets` files and both upstream license statements.
The Rina manifest pins all 21 required inputs and should be verified after copy.
No proprietary reference images are part of the delivered source or evidence.

## Shared deltas: merge narrowly

`shared.patch` records these exact changes against the private base:

1. Add Rina's three source files to `engine/CMakeLists.txt`.
2. Declare `make_rina_character_profile` and dispatch only the exact
   `NpcCivE` + `CharacterRole::Dock` pair in `character_profile.cpp`.
3. Register the three Rina C++ test binaries with runtime-library staging,
   source/reproducibility Python checks, and CPU/GL renderer tests in
   `tests/CMakeLists.txt`. Preserve all newer actor tests.
4. Remove only `NpcCivE` from the historical generic-roster lists in the Jon,
   Owen, Tessa and original character-profile tests. Those tests must no
   longer assert that an intentionally individualized Rina stays generic.
   Do not restore other already-removed actors from this older study.
5. Merge the small characterlab diagnostic controls only if still absent:
   `crouch-walk`, bounded finite attention yaw/pitch, and their report fields.
   Preserve the main project's existing controls. Add the independent
   `tests/characterlab_attention_tests.py` and its CTest registration, plus
   the matching `docs/CHARACTER_REVIEW_STAGE.md` notes.

The Rina generic-fallback tests use unknown identities, so they do not force
another named actor to remain generic as the roster is completed. All Rina
model source, recipes and tests are separate from other actors' owned files.

## Required post-merge checks

Reconfigure before rebuilding to update the runtime fingerprint. Build with
at most two jobs. Run the full aggregate suite, Rina geometry/contact tests,
the source/reproducibility tests, CPU raster/path and actual GL pixel-update
checks, and diagnostic CLI tests. Regenerate representative final evidence
from the merged binary: a study fingerprint cannot certify a later merged
source tree. Keep the source-input and binary manifests with the evidence.

## Preserved baseline limitation

Rina's route, spawn, home, schedule, speed, radius, dialogue and gameplay remain
unchanged. The authored route passes through Bldg21 and Bldg11, plus a low bridge
pillar and parked car. This is documented in `WORLD_ROUTE_LIMITATION.md` and
visible in actual-game evidence. Do not move waypoints, hide props or change
navigation as part of this art-only integration.
