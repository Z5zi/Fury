# Integrating the Hale-only checkpoint

The study is isolated from the main checkout. Do not overwrite the main tree with this study, which started from tree `9d424277e99ba37a8c66e77430512d6dc188ab40` before other character passes landed. No external publication or GitHub write is included.

## Owned files

Apply the Hale-owned patch (or copy only its listed files):

- `engine/src/character_hale.cpp`, `character_hale_shirt.hpp`, `character_hale_vest.hpp`, `character_hale_cap.hpp`
- `engine/include/fury/generated/hale_anatomy.hpp` and `hale_hands.hpp`
- `scripts/build_hale_anatomy.py` and `build_hale_hands.py`
- `tests/character_hale_tests.cpp`
- `third_party/hale_anatomy/*`
- The Hale-only reports, rendered images, videos and validation files

The anatomy manifest references the already vendored pinned CC0 files in the existing MPFB/Jon/Owen/Lia/Tessa source directories. It does not require proprietary references, Blender or the GPL addon program.

## Surgical shared changes

Preserve all other workers' declarations, dispatch branches, tests and CMake entries:

1. Add `src/character_hale.cpp` to the engine source list
2. Declare `make_hale_character_profile` in `character_profile.cpp` and dispatch only exact identity `NpcGuard` with role `Security`
3. Register the `fury_character_hale_tests` target, strict `character_hale` test, and explicit `character_hale_ordinary` test (`--ordinary`)
4. In `character_profile_tests.cpp`, replace only Hale's former generic-fallback entry with `unregistered_security`. The dedicated Hale test independently checks the exact guard dispatch, unrelated guards and wrong-role/case fallbacks
5. Add the Hale anatomy include and its four arrays to the shared `character_surface_tests.cpp` face fixture. Keep the other characters' entries

`apps/characterlab/main.cpp`, `tests/characterlab_attention_tests.py` and the review-stage documentation contain diagnostic attention/crouch-walk controls inherited from the earlier shared tooling. Their patch is separate. If the main checkout already has those controls, retain it; do not replace it with this older study's whole file.

No changes belong in NPC movement, patrol/spawn anchors, bank/mission geometry, scenery, controller inputs or shared skinning. `GAMEPLAY_PRESERVATION.json` verifies those files against the exact study base. Preserve the later independently accepted Lia/Owen/Nell corrections on main.

## Verification after integration

Regenerate/check both Hale headers from the pinned assets. Reconfigure to refresh the source fingerprint, build and run the Hale, profile, surface, skinning, animation, character-renderer/LOD, NPC presentation/gameplay/runtime and affected combined-world tests. The material fixture should retain every previously integrated source face.

The private source's exact image/validation fingerprint is recorded in the pass report and evidence manifest. Main's combined fingerprint will differ after serial integration. Re-capture at least the actual original-spawn close-up, entrance view, live motion and Near/Far diagnostics against that combined build rather than treating the isolated images as proof of all integrated actors together.

Known navigation and ground-placement defects are explicitly preserved. No successful model test or private render is a certification that a bank guard respects walls, avoids another guard or stands on the raised visual forecourt.


## Known strict test failure is retained

The default registered `character_hale` test remains **failed** on full crouch-walk trouser/footwear contact, at all heights and both LODs (worst 122 Near / 42 Far). Do not remove that assertion, disable the test, set WILL_FAIL or report a globally green CTest run. `build/tests/fury_character_hale_tests --ordinary` is the separately passing scope: bind and 167 ordinary contact poses, while finite geometry and cuff coverage still exercise all 233 samples. Report the full stress result separately after integration. The final sanitizer uses this explicit ordinary mode; it is not a passing full-stress sanitizer suite.
