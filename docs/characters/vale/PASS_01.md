# Ofc. Vale: individual character pass

**Completed: 120 minutes 15 seconds of verified dedicated art/verification.**

Actor `NpcAlleyHmpd`, Security, 1.86 m. This is an original Fury art pass using
pinned CC0 graphical anatomy data and Fury-authored construction methods.
It is not a claim of GTA VI or AAA parity. No proprietary game asset, copied
likeness, reference texture, new biography, family relation or rank marking is
included.

## Result and preserved identity

Vale keeps the exact actor ID, seed `0xf5cd87d6`, role, height, original palette,
cap, short near-black hair, radio and left belt pouch. His broader head and
shoulder structure relative to Hale are retained. The separate 17-target recipe
builds high cheek planes, a tapered longer jaw, projecting chin and restrained
nose profile; it is distinct from Hale's and Renn's recipes. Graphical target
labels are not demographic facts about the fictional character.

The original patrol base speed is 1.4 m/s and chase speed 3.8 m/s. Spawn is
(16.2, 0.93, -12.0), with the same three waypoints, home, `NightTighten` schedule,
controllers and gameplay. The existing night schedule still compresses the
route by 0.52 and multiplies patrol speed by 1.45. Fifteen relevant gameplay,
renderer and controller files are byte-identical to accepted source commit
`9d424277e99ba37a8c66e77430512d6dc188ab40`; see `UNCHANGED_GAMEPLAY.json`.

The cap mark was restored to its original (0.63, 0.70, 0.66) color after the
surviving WIP had incorrectly reused the chest mark's color. Both remain plain,
unlettered existing equipment marks. `PALETTE.json` and
`IDENTITY_CONSTRAINTS.json` document the preserved base values.

## Geometry and construction

| Final LOD | Vertices | Triangles | Unchanged triangle ceiling |
|---|---:|---:|---:|
| Near | 24,909 | 38,364 | 40,000 |
| Far | 10,765 | 15,928 | 16,000 |

The complete head and ears remain in both LODs: 4,343 source positions, 8,628
original triangles and 25,884 original UV corners. No anatomical face triangle
is removed to meet the budget. Every source position, source vertex/UV/face ID,
triangle order and atlas corner is independently reconstructed and tested.
The full 56-edge anatomical attachment loop continues into a hidden under-neck
surface, with matching physical positions and shared smooth seam normals.

Hands use one connected anatomical surface per hand. Near retains all 3,276
source triangles per hand; Far retains 520 through a bounded, original
endpoint-quadric reduction. The exact 20-edge wrist remains fixed, finger
cross-section counts remain consistent, and the resulting surface is one
oriented manifold disk in both LODs. There is no separate finger animation.
The forearm is sewn to the complete source wrist loop instead of meeting a
separate hand capsule.

The shirt is a connected sewn pattern with genuine armholes and sleeve roots.
The trousers have a connected rise/crotch rather than overlapping leg caps.
Vest, collar leaves, radio support, closures and pocket fronts are fitted to
actual supporting triangles. Lower trouser hems were raised and eased to leave
an intelligible shoe upper while retaining strict boot clearance in movement.
The cap has an opaque cropped-hair backing, fitted crown, band, layered bill and
rear adjustment overlap. Distracting striped nape cards were removed.

The final brim is one exactly closed physical shell. Its top, bottom and all
four edge returns use the same sampled boundary positions. This closes the
buried root and removes side T-junctions while saving 32 Near and 8 Far triangles.
The physical topology check requires paired oppositely directed edges, one
component and Euler characteristic 2. It checks 496 Near and 172 Far bill
triangles, not just a visual silhouette.

## Defects found through review

1. The interrupted build had 45,284 Near / 19,566 Far triangles. Recoverable WIP
   had already reached 38,396 / 16,128, but its Far budget failure stopped later
   checks. Redundant accessory, cap, eye, boot and garment subdivisions were
   reduced without increasing either ceiling or weakening an assertion
2. Fresh Far testing exposed cuff/boot and pocket-trim contact failures.
   Quarter-cycle contact checks then missed a Far underarm crossing at run
   phase 1/16. The final suite evaluates every one of the 112 sampled phases
   per LOD, and local fitted clearances were corrected
3. Attention pixels exposed severe lower-face shear: the old Head/Neck blend
   extended through the mouth. All 38 exact lip-seam vertices had only
   68.25–77.74% Head weight. The blend now ends below the mouth at 0.373h.
   Nine extreme/combined attention samples show zero error against the rigid
   Head transform for every lip corner. A copy with the original weights
   produces 0.0131605h error, approximately 24.5 mm, and is rejected by the
   regression
4. Source closure review found the brim's open buried root and mismatched edge
   subdivisions even though the ordinary views looked closed. This was repaired
   in the final geometry and given a dedicated physical-edge regression

The exact attention before/after witness is in
`evidence/attention-fix/WITNESS.json` and `comparison.png`. Numeric pose, phase,
camera, LOD, image dimensions and renderer match. Original pixel hashes and
source/binary fingerprints are retained. Older `up`/`down` file labels were
replaced by explicit pitch signs to avoid confusing the engine's convention.

A read-only peer check found that some other actor implementations use the same
kind of height-threshold weighting. `ATTENTION_SOURCE_RISK.json` records actual
source indices and weights; it does not claim a visible peer defect. No other
actor model was modified in this pass.

## Verification

- Exact geometry: finite positions/normals/colors, unit normals, valid indices,
  consistent winding, zero degenerate triangles, normalized skin weights,
  material-chart separation, exact height/soles and fixed LOD budgets
- Source: independent reconstruction of all 8,628 anatomical head triangles and
  all 3,276 original hand triangles per hand, with every original UV corner
  and pinned input hash checked; 17 malformed source controls rejected
- Topology: connected sewn shirt and trousers with intentional simple opening
  loops, complete anatomical wrist seams, oriented Near/Far hand surfaces,
  exact closed bill topology, and three corrupt hand-topology controls rejected
- Motion: 112 poses per LOD spanning idle, exact 1.4 m/s patrol, exact 3.8 m/s
  chase-speed gait, crouch, crouch-walk, talk and extreme attention. All phases
  check face/hair, face/collar-cap, vest/shirt, hands/cuffs, hands/trousers,
  boots/trousers and thigh/calf contacts. Stable mesh storage, unchanged UVs and
  colors, finite unit geometry and exact bind reset are also required
- Renderer: the exact Vale profile is rendered in CPU software rasterization,
  CPU BVH ray tracing and actual OpenGL through Mesa llvmpipe. Readback is
  idempotent; changed poses alter actual pixels; held poses preserve pixels,
  revisions and cached BLAS; restoring bind restores exact pixels. No silent
  backend fallback or hardware-ray-tracing claim is accepted
- Sanitizers: AddressSanitizer and UndefinedBehaviorSanitizer pass the full
  Near/Far contact suite and software/CPU-ray individual renderer fixtures.
  LeakSanitizer aborts under this traced executor, so leak detection is marked
  unavailable. The successful rerun disables only leak detection
- Roster regression: 45/45 passed against final source in 230.27 seconds, with no skips.
  The diagnostic attention/crouch-walk CLI checks also passed; full results
  are recorded in `VALIDATION_STATUS.json`

Four old generic-fallback fixtures still named the now-individualized Vale.
Only those exact Jon/Tessa/Owen/Lia entries were changed to
`unregistered_alley_security`. Every assertion and wrong-name/role case was
preserved. The profile suite had no stale Vale entry. These four lines are
provided as a separate shared-test patch.

## Pixel review and honest limits

Eight head and body directions were inspected in material and clay for Near and
Far, followed by walking, running, crouching, crouch-walking, talk and attention.
The exact original game world is also captured with scenery, real guard motion
and the actual day/night cycle intact. Characterlab is always labeled as a
diagnostic stage, never substituted for game-world proof.

The original alley blocks some camera directions. In the initial all-side
world review, body orbits 0 and 45 are occluded and face orbit 0 is partly
occluded. Existing scenery also obscures the boots at the unchanged spawn;
live patrol frames 24–80 show them. Those frames are kept rather than deleting
obstructions, moving the spawn or claiming every world angle is unobstructed.
The 312-frame/4-fps case is twilight/night tightening; the deeper-night case is
identified separately.

Broad guard navigation repair remains explicitly deferred. Existing Vale
behavior crosses BankWallE/KIT_WallR. No navigation geometry, controller,
schedule, route, player collision or gameplay change is included, and this
pass does not recertify navigation.

Far keeps the complete face but has visibly coarser cap, shoulder, knee, shoe
and hand silhouettes when forced into portrait-distance diagnostics. It is
intended for the unchanged 12 m switch. There is no facial blendshape/lip-sync
system, separate finger articulation, dynamic cloth simulation, anisotropic
hair or measured subsurface skin. Mesa llvmpipe is software OpenGL; it disables
some advanced renderer effects. CPU ray/path images have their exact sampling
settings recorded. Simplified trouser folds, rigid carrier edges and plain boot finish remain explicit art limits. The rear-yoke material can show a foreshortened woven pattern. CPU-ray shadows flatten facial contrast, and the recorded path portraits remain noisy at 16 accumulated samples. See `PIXEL_REVIEW.md` for the actual visible critique. The result remains a deliberately stylized Fury character.

## Reproduction and integration

See the pinned input/recipe description in `third_party/vale_anatomy/README.md`,
the source audits, `VALIDATION_STATUS.json`, cost report and capture manifests.
The individual source, generated headers, recipe, tests and evidence are owned
by this pass. Shared changes are narrow: one engine source registration, the
exact identity/role dispatcher entry, test target registration, and the four
stale fallback fixture names. The diagnostic attention/crouch-walk tool patch
was received separately and remains a separate integration dependency.

The dedicated-time ledger is `WORK_LOG.md`. It excludes the uncertain
01:09–02:11 recovery interval, resource-idle waits and final packaging. Host/file
mtimes are one hour ahead and are never used to claim art time.
