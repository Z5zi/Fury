# Cass Vesper: geometry, attention and validation scope

Final core fingerprint: `2af8d6083e602a537b163ac56303c34209c595676e73eae2af54bd79fdc6e05f`.
Private base: `22be1bee7a5c66c25db0ce38ab4cc526a6493382`.
Current evidence is `cass-evidence/accepted-final`; all earlier fitting folders
are historical. Final sanitizer/renderer closeout is complete and recorded in that folder.

## Source and ordinary geometry

Near has 38,778 triangles and 24,152 vertices; Far has 15,898 triangles and
10,072 vertices. The original limits remain 40,000 and 16,000. Both retain all
8,696 head/ear/neck source triangles and their exact position/corner-UV stream.
Near retains complete connected source hands; Far uses the documented
endpoint-quadric reduction with a locked 20-edge wrist loop. Source generators
check exact reproduction against the pinned graphical inputs.

The model checks all three heights (1.35, 1.78 and 2.10 m), both LODs, source
correspondence, UV regions, normalized weights, finite geometry, winding,
nondegenerate triangles, eye/socket fit without relying on the lenses, and
actual opaque scalp coverage. The independent runtime-dump audit checks
nonpositive normal agreement, welded manifold edges, connected components,
closed scarf/lens/frame construction and intact hand/wrist boundaries.

The explicit `--ordinary-geometry` mode passes 1,398 sampled animation/attention
poses and 3,000 proper layer-intersection cases. It explicitly excludes the
strict cache-seam assertion and the full-crouch-walk contact assertion; those
remain failed in the unchanged default mode. This is sampled, bounded
validation, not a global self-intersection proof or cloth simulation.

## Visible jaw defect and corrected attachment

Extreme ±0.9-radian yaw with ±0.4-radian pitch exposed a stretched exterior chin
and a protruding edge. This was a real pixel defect despite green finite and
ordinary-contact checks. Cass's 38-position lip seam is already above the old
neck blend: its y range is .387203–.389113. The complete internal-mouth chart
reaches y=.365186, so a lip-only test would miss the defect.

The final fitting keeps all 418 internal-mouth positions and 484 UV corners
rigid to Head, with source-chart derivation rather than altered geometry. A
separate, independent exterior-chin witness selects 308 non-mouth source
positions within .020 normalized distance of the actual jaw landmark. Both
regions show zero rigid-Head error in nine attention states at each height/LOD.
Injecting the original weights produces maximum normalized errors of about
.015919 for the mouth and .038654 for the exterior chin, and is rejected.

The exterior jaw blend follows the forward jaw envelope; the posterior neck
stays gradual. All 58 source attachment-boundary points lie at y=.330018–.337727
and retain their old weights. The scarf follows the barycentric influences of
actual supporting neck triangles. Its top front edge dips below the moving
chin while preserving the original mauve wrap and tail. The rig, pose matrices,
controller and canonical source positions/UVs are unchanged.

Rejected spatial, shortened-global-blend and mouth-only trials are preserved
under `cass-evidence/resumed-final/jaw-fit*`, together with actual pictures and
contact failures. `JAW_MOUTH_SOURCE_WITNESS.json` records exact generated and
upstream source indices, UV corners, positions and matched before/after views.

## Deliberately unresolved default failures

- The shared pose-update cache can retain one duplicate shading/UV corner while
  updating another. The strict 1e-7 m seam assertion therefore remains failed.
  The independent 5e-7 m reproducer and actual submicron witnesses remain in
  `cass-evidence/seam-before`. Its proposed shared-engine patch is unapplied,
  excluded from the deliverable, and not needed for the visible art correction
- Full-crouch-walk places the trailing shin about 78.4369 degrees from vertical
  while its foot remains about .0108 degrees from level. The ordinary tailored
  calf/boot and jacket/thigh contact failures are retained as stress evidence,
  with exact posed vertices, all 17 matrices and actual frame-4 pixels. The
  rejected distorted-calf trial is not shipped. Ordinary NPC presentation uses
  zero crouch weight; no controller or route change hides the stress failure

The default Cass CTest remains a failure, without `WILL_FAIL`, altered
predicates, skipped witnesses or raised timeouts. Passing the explicit
ordinary mode does not mean the full default test passed.

## Rendering and world evidence

Actual software rasterization, CPU ray tracing and OpenGL/llvmpipe evaluate
12 motion/attention samples per LOD, held frames, storage identity and exact
restoration. The reference bind pose is evaluated through skinning, matching
the existing shared renderer test. Before that fixture correction, one Far
software pixel differed by one byte level because source normals were
renormalized; positions were unchanged. Original failure evidence is retained.

The final stage includes eight-direction material/clay head/body images,
low/top views, complete hand/boot/jacket details, ordinary locomotion, bounded
front/back/profile attention, renderer portraits and explicit LOD-distance
pairs. Glasses removal is separately labeled and never substitutes for the
shipping identity. These are diagnostic engine-stage images, not game frames.

Actual Vaultline evidence retains the shop, scenery, other actors and live
controller. Ten matched capture cases preserve the complete audited world
state and parameters against the accepted baseline. Many shop angles are
obscured by inherited stalls/facades. The portrait, raised/rear shop views and
clear route segments provide useful character views without removing scenery.
The attention trigger is a presentation diagnostic, not proof of operating a
literal dialogue branch or shop transaction.

## Reproduction

Run `scripts/build_cass_anatomy.py --check` and
`scripts/build_cass_hands.py --check`. Build the Cass model/renderer tests and
Characterlab/Vaultline. Run the model test both with `--ordinary-geometry` and
without arguments, retaining their distinct exit codes. Use `--dump PREFIX`
followed by `scripts/validate_cass_mesh.py --mesh-prefix PREFIX --output REPORT`.
The renderer test supports software/CPU-ray by default, `--gl`,
`--detail-evidence DIR`, `--cpu-detail-evidence DIR` and `--stress-evidence DIR`.
The exact executed capture, test and sanitizer commands are in final records.

ASan/UBSan use `-O1 -g1 -fsanitize=address,undefined -fno-omit-frame-pointer`.
Leak detection is disabled because the traced executor cannot support the
required LeakSanitizer inspection; no leak-clean claim is made. Only a zero-exit
current-source ordinary sanitizer record is a passing certification; a default
run that exits on the retained assertions is reported separately.

This remains a stylized, source-anatomical character. It does not add scanned
skin, facial performance, independent finger motion, cloth/hair simulation,
optical lens refraction or commercial-game fidelity.

Final current-source sanitizer outcome: the explicit ordinary mode exits 0
in 216.64 seconds. The default mode completes with exit 1 in 245.90 seconds
and the same twelve retained assertion failures (six seam, six stress-contact).
Neither completed log contains an AddressSanitizer or UBSan diagnostic. This
does not turn the failed default test into a passing geometry certification.
All frozen source hashes remain unchanged through closeout.
