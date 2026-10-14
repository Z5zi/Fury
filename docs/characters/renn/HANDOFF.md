# Ofc. Renn: frozen individual model and verified scope

Private study based on accepted tree `9d424277e99ba37a8c66e77430512d6dc188ab40`.
This is an integration handoff, not a publication or an all-green release claim.
`WORK_LOG.md` certifies **120 min 26 s** of dedicated reference, design,
authoring and active review. Outages, unclosed historical intervals, passive
waits, archival and packaging are excluded.

## Frozen result

Renn preserves `NpcDeskGuard` / Security, nominal 1.82 m, seed `0xb242d872`,
the original appearance palette, original route, speeds, mission state and
shared animation/controller behavior. Near is 25,841 vertices / 39,940 triangles;
Far is 10,421 / 15,992. Profile distance remains 12 m.

His independently fitted CC0 source face/hands, broad uniform, lined protective
vest, closed cap crown/band/peak, rolled collar, connected trouser rise and
service boots replace the primitive baseline. Exact source head corner/UV
correspondence and every Near hand source triangle are retained. The Far hand
uses the documented endpoint reduction with locked wrist and finger contours.

The final bounded clothing/footwear study retained only a modest finite welt,
deeper arch and scalar variation of the original rubber-sole tint. Two cloth
experiments and an unnecessary vamp crease were rejected; the original boot
upper, quiet trousers and carrier remain. No triangles or rig changes were
needed. See `out/renn/uniform-study/ART_CRITIQUE.md` for matched pixels and
rejection reasons.

Production source fingerprint:
`4084290d1911f8a1323125e6d9d58258e32548adb4335561532acec221de051c`

Final application binaries:

- `build/apps/characterlab/fury_characterlab`:
  `08601824252cbe1da6dbb6e0d8a38d99b433599ebc4db0493da5006a0ac8d74f`
- `build/apps/vaultline/vaultline`:
  `c8bbb48d8d45bf8c8cf1d22511159efc970a1111db623f4ff110ba9c47c67741`

## Integration ownership

Use `SOURCE_MANIFEST.json` for exact per-file SHA256s, baseline shared hashes,
source dependencies, preserved paths and evidence hashes. Owned source files:

- `engine/src/character_renn.cpp`, `character_renn_shirt.hpp`,
  `character_renn_equipment.hpp`
- `engine/include/fury/generated/renn_anatomy.hpp`, `renn_hands.hpp`
- `scripts/build_renn_anatomy.py`, `build_renn_hands.py`
- `tests/character_renn_tests.cpp`, `renn_anatomy_source_tests.py`
- `third_party/renn_anatomy/` and `docs/characters/renn/`

Shared deltas are supplied separately as `shared-integration.patch`, based on
the accepted tree. Integrate these serially against the coordinator's current
roster; do not replace shared files wholesale:

1. Compile `character_renn.cpp` and add the exact identity/role dispatch
2. Register the new strict default `character_renn` test
3. Replace the now-registered `NpcDeskGuard` entry with
   `unregistered_desk_guard` in five generic-fallback fixtures: profile,
   Jon, Tessa, Owen and Lia. No assertion changes
4. Characterlab-only `crouch-walk` and finite, bounded attention controls;
   `tests/characterlab_attention_tests.py` validates old-default and explicit
   zero-control pixel/state identity, invalid input rejection and phase advance

No shared renderer, controller, animation, navigation or world source changed.
All nine files in `PRESERVATION.json` still match exactly.

## Verification

Final-source logs are `out/renn/uniform-study/conservative-*.log` and
`conservative-*-exits.txt` / `conservative-exits.txt`. Earlier control logs
remain under `out/renn/final-checks/`, including the initial LSan limitation.

- Optimized build succeeds. Final CTest: **43 of 44 pass**. The sole failure is
  strict `character_renn`, specifically the stress contact below
- `fury_character_renn_tests --ordinary`: passes all three heights (1.35,
  1.82, 2.10 m), both LODs and 15 ordinary/attention contact pairs. This option
  excludes only full crouch-walk *contact* cases; all 233 sampled pose,
  vertex, normal, storage, ground and sewn-corner checks still run
- Independent source audit and both generator `--check` runs pass;
  `conservative-source-audit.log`, `conservative-head-generator.log`,
  `conservative-hand-generator.log`
- GCC 14.2, `-O1 -g0 -fsanitize=address,undefined` ordinary run passes with
  `detect_leaks=0`, `halt_on_error=1`; `conservative-asan-ordinary.log`
- Default sanitizer run returns 1 for the same strict stress assertions. It
  continues through the later height 2.10 checks without ASan/UBSan diagnostics;
  `conservative-asan-default.log`. This is not a passing default test
- Initial `detect_leaks=1` logs remain preserved: LSan cannot complete under
  this ptrace environment. Leak detection is **not certified**
- Actual CPU raster, OpenGL llvmpipe and CPU BVH ray captures succeed. Ray
  reports explicitly assert the backend, fingerprint, triangle count and zero
  validation errors; no silent renderer fallback is counted as a pass
- Diagnostic controls pass against the saved before binary; see
  `out/renn/final-checks/diagnostic-controls.log` and
  `diagnostic-controls/summary.json`. The diagnostic CLI source is unchanged
  by the final footwear-only selection

The sanitizer build disables OpenGL because its configure did not find the GL
development package. Sanitizer geometry checks and normal-build actual GL
pixel checks are separate evidence, not a claim of GL sanitizer coverage.

## Deliberately unresolved limits

The strict default remains red for trousers/boots in the unchanged full
crouch-walk gait at nominal height. Maximum crossing pairs among the tested
stress poses are Near **64** (mode 5, frame 16, triangles 14756/17536) and Far
**34** (mode 5, frame 4, triangles 3130/3828). All other tested pairs, including
hands/boots, pass. The shared solve keeps the sole planted while bringing a
shin nearly horizontal across it. Visibly distorted calf/ankle candidates were
rejected. No thinner artificial calf, altered rig height, weakened contact
assertion, raised existing timeout or controller rewrite conceals this result.

The production pose cache uses component tolerances; tiny cached-corner
differences can occur even where independently evaluated geometry coincides.
The tests distinguish uncached equality from that bounded cache behavior.
This art pass does not fix or claim bit-exact cache restoration.

Actual bank spawn and chase still clip into the original wall, desk/terminal,
curbs and other solids. Camera occlusion appears in retained movie frames.
Those world/controller defects are not corrected by the model pass.

The face has no blink, expression or finger animation. This prototype does not
have production subsurface skin, strand hair, detailed face maps, cloth
simulation or motion capture. It is visibly improved source-based game art,
not GTA VI/AAA parity. Far portrait shots deliberately exaggerate its coarse
cap/collar/garment silhouette; native 12.2 m views remain available.

## Evidence to review first

- `out/renn/final-selected/near-eight-views.png`, `far-eight-views.png`:
  eight current final material directions at each LOD
- `out/renn/final-selected/{near,far}-ordinary-motion.png`: final walk/run
  phases; the individual talk/crouch, clay and 12.2 m images are in that folder
- `out/renn/final-selected/ray-{near,far}-{face,rear,run,crouch}.png`:
  actual CPU BVH ray tracing, explicitly checked against the final fingerprint
- `out/renn/uniform-study/conservative-profile-comparison.png` and
  `conservative-ray-quarter-comparison.png`: matched native control/final
  sole construction, with the original upper restored
- `out/renn/uniform-study/conservative-world-{day,night,side}-comparison.png`:
  matched actual-world crops. Complete state and camera JSON match before/after;
  changed pixels are confined to the boots. `side` is the orbit-90 request,
  not a side-facing actor: original chase behavior turns him toward the camera
- `out/renn/uniform-final-{day,night,side}/frame.png`: full current world frames,
  logs and manifests. Night uses the existing mission-4 cue in an isolated save
- `out/renn/curated/{before,final}-bank-night-face.png`: the earlier matched
  bank-interior broad art upgrade. These belong to control fingerprint
  `3dc6ed7f…`; its face/upper-body source is unchanged by the final sole work
- `out/renn/curated/`: retained high-resolution control material/clay/portrait,
  GL, Near pose and attention sheets. They are control evidence, not relabeled
  as captures of the new final fingerprint
- `out/renn/final-bank-motion/motion.mp4`: retained 96-frame control world
  sequence, including unhidden scenery/camera clipping. Its source fingerprint
  is the earlier control; it is not a new final-variant video
- `out/renn/curated/stress-side.png`: illustrative retained full-crouch-walk
  failure. Exact final numerical scope and witnesses are in the current logs

The original control Near/Far/clay sheets are **CPU raster**, despite an inactive
`trace=ray` field in the old wrapper options. Actual report backend/trace fields
are authoritative. Current ray evidence is separately and explicitly named.
OpenGL is actual llvmpipe; its driver disables directional shadows/reflections.

Reproduce with the saved capture scripts and exact command/environment manifests.
`out/renn/final_conservative_review.py` makes the current final stage set;
`capture_uniform_world.py` accepts the saved control or final executable for
matched world captures. Use a new world-case directory: the script refuses
an overwrite. In this shared environment use CPUs 3–4, two CPU jobs and `-j2`.

The 48 completed uniform-study control/A/B/C raw PPMs were replaced with
byte-verified lossless `.ppm.gz` archives, while PNGs remain. Restore details
are in `out/renn/uniform-study/LOSSLESS_PPM_ARCHIVE.json`. Current final-stage
captures also retain exact raw PPM hashes and lossless gzip paths. No further
broad cleanup or redundant build-copy expansion was performed.

The source asset manifest pins MPFB graphical data to commit
`d0a32e57a7f915cb2f2b95410e2117648c7bbb7e` under CC0-1.0. The original importer,
fitting and clothing construction are Fury code. Rockstar and official footwear reference pixels
under `out/renn/private-reference/` are private study only and must not be
copied into a shipping source/evidence package.

## Cumulative integration update

7 October 2026: the owned art files are now applied to the cumulative main source.
The original handoff above records the private freeze. Shared registration and
generic-fallback fixtures were merged surgically; no assertion predicates were
weakened. Integration captures and complete-state comparisons are in
`docs/validation/characters/integrated-thirteen-20261007/` after curation.
Private validation is not substituted for those cumulative checks.
