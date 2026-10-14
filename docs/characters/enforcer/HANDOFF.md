# Syndicate Enforcer individual pass

The private Enforcer model is complete and frozen: **38,774 Near / 15,976 Far
triangles**, original `NpcEnforcer` identity, Enforcer role, seed `0xbb361ac4`,
1.95 m height and role palette. Verification is complete with the explicit strict crouch-walk exception below.
Publication remains paused.

## Source and integration

- Study: `/workspace/scratch/4d97e78a29f3/enforcer-study`
- Immutable accepted-eight tree: `b7bb006f1748fbe4f0e6ce46a19a7631a6166e0a`
- Private baseline commit: `f4122ae1e3843454b9b7daeed34eeaa31d18188d`
- Frozen art/code commit: `cf19fd234051df7f3e1f2c3b49a37f7a7dfa6b7b`
- Final documentation revision: `60428a95d3236a0f7f4aa30d707d84db6f2b8d74`
- Final production fingerprint: `e191104a36c4a57913843e065bdcbe3fff009c67c50230f5fc7b4ef7d1d447e0`
- Exact original binary fingerprint: `8015066ee082524146d5888024447c1ed5198d719529a98c1484942ce8da856c`

`SOURCE_MANIFEST.json` records every owned/shared path and hash. Extract
`enforcer-owned-source.tar.gz` for the 33 new owned source, test, diagnostic and
reference-provenance files. Apply these small shared patches serially:

1. `shared-art-and-tests.patch`: engine registration, exact dispatcher, test
   registration, and five obsolete generic-fallback test IDs renamed to
   `unregistered_enforcer`. All original assertions and case counts remain.
2. `shared-spawn-order-repair.patch`: the separately authorized functional fix.
3. `shared-lifecycle-diagnostics.patch`: optional audit/capture flags and driver
   hook; needed for the supplied lifecycle regression/capture commands.

`RECONSTRUCTION_CHECK.json` proves these reconstruct all 42 changed/new files
byte-for-byte against the immutable baseline. **488 of the original 497 files
remain byte-identical.** Original animation, controllers, NPC roster, scene,
mission, morph data and other actor implementations are untouched. There is no
main-checkout or external write. The private baseline manifest and original
pre-edit executables remain at `out/character-study/enforcer/baseline/` in the
study; their hashes are copied into `reports/`.

## Individual art and evidence

Credited dedicated individual art/active QA is **2h00m22s**, with dated UTC
intervals and exclusions in `docs/characters/enforcer/WORK_LOG.md`. Shared
lifecycle/cache tooling, outages/context recovery, passive waits and packaging
are excluded. Later verification is conservatively uncredited.

The pass adds a distinct source-derived face, complete connected fingers and
wrist attachments, fitted sewn shirt/trousers, supported finite carrier/pads,
and source-fitted scalp and attached bun. Every original face position, triangle
and UV corner remains intact in both LODs. All 20 pinned input records verify;
only existing CC0 MPFB graphical data and Fury's own importer/geometry code are
used. No addon code or proprietary asset is executed or shipped. Saved official
GTA VI and professional guard references were visually studied; their images
are excluded from this package.

The final neck fit follows this actor's actual jaw/neck contour and keeps the
entire external head, jaw, mouth, sockets and 48-vertex seam rigid. The retained
old global-Y assertion rejects exactly **23 true neck vertices / 140 source
corners**, independently verified against raw pinned OBJ UV ownership. No
protected face vertex is reclassified. Low-jaw and mouth-to-Neck corruption
controls detect errors of about 0.035–0.048 normalized units; the historical bad
lip-weight control also fails. A lower source-derived skull pivot and fitted
front collar resolve the observed attention/collar conflict. Rejected fits and
strict witnesses remain in the study.

`review-images/` contains 22 curated before/after, all-side material/clay, Far,
attention, crouch, detail, actual-world and real-pursuit images. The complete
final source capture set is in the study: `final-stage/` has 126 original frames;
`final-poses/` has 100 attention/height/forced-LOD frames. Original backend RGB
pixels are preserved losslessly as PNG. Labelled sheets are reduced views;
individual originals remain available. See `EVIDENCE_MANIFEST.json` for paths,
hashes and the exact comparison scope.

## Validation and actual gameplay

- Full final CTest: **62/63 pass**, 437.70 s, one pinned CPU. The sole failure is
  the explicitly retained strict full crouch-walk boot/calf crossing at Near
  1.95 m, mode 4, phase 4/24, groups 5/6, triangles 18037/15996.
- Ordinary actor scope: **1,440 pose/contact samples pass**, both LODs and
  heights 1.35/1.95/2.10 m, including actual 2.4/5.6 m/s samples, attention and
  crouch. Source reconstruction, finite geometry/normals, topology, palette,
  exact height, triangle budgets and source wrist/head checks pass.
- Exact-profile software, CPU-ray and real offscreen OpenGL renderer checks pass,
  including exact hold/restore, dirty geometry, latest revision, and 30/15/8 Hz
  updates. Standard character-stage walk/run images use its height-normalized
  speeds; exact-profile renderer tests and gameplay cover the actual speeds.
- Final ASan/UBSan: **ordinary 1,440-sample scope passes**, exit 0, 221.31 s
  wall / 221.29 CPU s, with no sanitizer diagnostics. Default strict scope
  exits 1 at the same boot/calf assertion after 28.41 s, without sanitizer
  diagnostics; this is not a complete 1,728-sample stress pass. The build
  takes 224.12 s and reuses 42 read-only cache
  objects only after exact compiler-option and every transitive dependency byte
  checks; six differing/owned/dispatcher/build-info inputs are compiled. The
  explicit direct-object link map prevents stale profile/build-info selection.
  Sanitizer GL=1/DX12=0 is recorded separately from release GL=0 configuration.
  LSan is unavailable under ptrace; no leak certification is claimed.

The real original progression failure is preserved separately: the rare roller
set `enforcer_alive=true` before the spawn function's old flag-only early return,
leaving zero Enforcer agents. The narrow repair skips only a request that already
has both the alive flag and an actual named agent. It preserves odds, tier,
timing, canonical location, original patrol 2.4/chase 5.6 m/s, and cleanup behavior.

The deterministic driver controls the test RNG seed/elapsed timer and camera/input;
it traverses the normal production transition functions without changing their
odds/tier logic or the runtime callback. It passes **12 production cycles across all six
missions**: four eligible Enforcer cycles and eight correct lower-tier absences.
It verifies production prerequisites, E/crouched breach, rare spawn, first spawn,
repeated calls/ticks without duplication, pursuit, failure/extraction cleanup,
reset, cache/material reuse and the unchanged 2e-6 m pose-reset contract.

For the final model-only art comparison, both executables use the same authorized
repair and lifecycle driver; only Enforcer dispatch differs. All **eight** real
Looting/pursuit frames have exactly equal actor, camera, heat, all-NPC and crew
state. Before fingerprint:
`2e6eea294813e479a459f38ea228767f041e135017e42975e8a62de900049f9f`.
Static views at +30/+90/+135 degrees also match the exact original executable's
camera and complete NPC/crew/heist state. The real progression save unlocks the
actual NightVault lighting used in night views. Staged `--npc-motion` failures
are retained and are not offered as production-spawn proof.

## Measured cost and limits

Three 24-frame runs per case, 320×480, CPU 7 only, one worker. Values are medians
of per-run mean frame-loop time. OpenGL is CPU llvmpipe; shared-host interference
is possible. Original generic model and new model use the same final stage.

| Renderer | LOD | Original ms/frame | Enforcer ms/frame |
|---|---|---:|---:|
| OpenGL llvmpipe | Near | 16.30 | 56.87 |
| OpenGL llvmpipe | Far | 14.17 | 41.00 |
| SDL software | Near | 23.13 | 66.51 |
| SDL software | Far | 21.58 | 64.50 |

The original model has 2,464/1,120 triangles; Enforcer has 38,774/15,976. The new
model has 26,628/11,058 render vertices. Detailed measurements preserve total
process/startup times separately; these are not isolated skinning timings or
production-game FPS. GL's zero CPU-frame counter is unavailable data, not zero
cost. No native-GPU, DXR, 1440p/120 fps or AAA/professional parity is claimed.

The full strict crouch-walk gate remains failed, with plausible anatomy retained.
Selected contact-pair checks are not an exhaustive all-surface certificate.
Magnified hair, eyes, Far fingers/collar and posterior neck remain simplified;
fingers/hair are static authored shapes. The unchanged BankFloor at Y=0.08 hides
part of boots whose NPC sole origin is Y=0. Existing scenery occlusion and NPC
pathing remain visible. No actor or scenery was relocated to conceal them.
