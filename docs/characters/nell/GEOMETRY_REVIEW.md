# Nell Ash geometry and provenance, pass 01

This is an original Fury character-art checkpoint, not a claim of GTA VI
fidelity. Source anatomy is pinned CC0 graphical data; official Rockstar and
artist/garment reference pixels were inspected for construction and quality.
No proprietary reference images, meshes, textures or code are in this source
patch. Immutable upstream paths, hashes and licenses are in
`third_party/nell_anatomy/MANIFEST.json`.

## Anatomy and identity

The stable ID remains `NpcCivAsh`, Market, 1.75 m, seed `d9d5f382`.
The original seven-color palette is fixed by regression tests. The narrower
shoulders/waist and long oval skull, tapered jaw, actual socket rims and low
compact bun make Nell distinct from the preceding market actor Jon and the
parallel Lia pass. The 12-target recipe is independent; target names are
numerical graphical controls and establish no invented biography.

The head/neck has 4,426 original positions and 8,800 source triangles at both
LODs. All source triangle corners retain original per-corner UVs and source
IDs. The neck uses the original 50-edge attachment loop; eyes are fitted to
actual source eye openings. Source-corresponded nonuniform shape residuals
against Mira, Jon, Tessa, Owen and Lia confirm that this is not a translated or
uniformly rescaled prior head. Those residuals are not visual-quality scores.

Each Near hand has 1,649 source positions and 3,276 triangles, including real
thumb webbing and connected finger roots. Far retains 271 source endpoints
and 520 triangles. Both are one component with a 20-edge wrist opening and
Euler characteristic 1. The reducer locks that wrist, checks the manifold link
condition and rejects triangle reversals. Cross sections at four finger levels
retain 2, 3, 4 and 3 separate contours. This does not prove exact silhouette
identity, so actual palm/back/side closeups were also inspected. The source's
concave thumbnail fold (face 9027) retains its triangle with separate rendering
normal corners. Static relaxed curl and reduced splay are explicit, invertible
shape changes; there is no new finger animation. Forearms fit the actual hand
basis and bridge exactly to its source wrist loop.

## Garment construction and cost

The shirt is a sewn torso/yoke/sleeve surface with open armholes and rolled
cuffs. The collar is supported by Chest and checked against the actual moving
source neck. Trousers have a joined crotch/seat seam and a waistband, with ankle
underlays covering the moving shoe opening. The apron is a closed curved shell
with a finite pocket, cross-back straps, waist tie, left rear bow and draping
tails. Surface offsets use normalized actual geometric normals and matching
weights. There are nine Near and ten Far closed cloth/trim components. Apron volume
divided by area gives effective thickness/height 0.00124636 Near and
0.00124410 Far.

The lower apron follows the nearby thigh with a narrow central blend. Extra
hip fullness is carried to the hem after rejecting an S-shaped side bulge.
This is authored skinning, not cloth simulation. Independent proper-triangle
contact tests cover both LODs across every 1/32-cycle walk, run, crouch and
crouch-walk sample. Earlier rejected versions and intersection witnesses remain
in private evidence, including the trial that passed seam/finite checks while
visibly cutting through knees.

| LOD | Vertices | Triangles | Budget |
| --- | ---: | ---: | ---: |
| Near | 25,273 | 39,464 | 40,000 |
| Far | 10,119 | 15,872 | 16,000 |

The original generic Nell had 2,274 vertices and 2,328 triangles. Far still
retains the full source head/neck but reduces hands, garments and hair. Forced
Near/Far renders at the actual 12 m profile transition are retained; forced
Far closeups expose reduced bun, hand and garment detail. Neither budget was
increased.

## Hair, tests and limits

Opaque cap coverage is tested independently of cards. Near's 1,392-triangle
cap and Far's 360-triangle cap have zero proper crossings, zero uncovered
source samples, and minimum radial gap/height 0.000916477 and 0.00203143 over
more than 20,000 source samples each. A deliberately buried cap is rejected.
The paired controls also reject protruding eyes and a flattened zero-thickness
cloth shell. The logs include the expected failing witness counts before the
unmodified positive checks; those negative counts are not production failures.

Actual runtime source corners/UVs, normals, triangle winding, body height,
seams and storage stability are checked. Each LOD at each of three heights
passes 233 poses: six sampled clips and 35 head turns, totaling 1,398 poses.
The real socket-rim eye depth stays in [-0.003, 0.002] times body height for
both LODs. Geometry tests are necessary regression evidence, not art approval.

CPU path portraits revealed raised card shadow bands missed in raster views.
Gathered cards now fit actual cap triangles; boundary roots nest into the
opaque shell and short tips follow the actual source scalp. Rejected and
corrected matched portraits are retained. Hair still reads too smooth and
cap-like in close views, skin remains simplified and waxy, straps are angular
at some views, and the long apron looks stiff. Hands have no individual finger
animation, the face has no blink/lip-sync/blendshapes, and there is no cloth or
hair physics. These limitations remain visible rather than being concealed by
camera choice or altered gameplay.

## Late LOD corrections and motion envelope

The full eight-side refresh found a Far seat hole caused by a radial sample
count omitting quarter-ring seam points. Far trousers now use 12 columns × 8
rows. Sixty-three actual center-back rays pass per LOD; the preserved old source
library fails the same new check. The Far wrist remains the full 20-edge source
loop. Three longitudinal forearm intervals and omitted subpixel Far laces
provide budget room without deleting connected palm/thumb/finger anatomy.

The coarse shirt also hid sections of the cross-back tape. The tape now samples
important actual support-face transitions along its center and edges, with
small interior surface corrections and matched source support weights. A strict
proper-interior test passes the upper cloth against the real shirt in bind and
sampled walk/run/talk/crouch poses. It excludes physical shared-edge contact.
The Far bib adds modest fullness along the actual shirt normal, fading to zero
at waist and strap attachment. Small steep ribbon-fold faces use independent
rendering corners where averaged normals would face backward. No triangle,
position, UV or bone influence is removed or changed by that shading split.
These checks also reject the preserved pre-correction source library.

At the real 1.9 m/s route speed, a 129-phase stateless pose sweep gives maximum
horizontal vertex radius 0.585523 m Near and 0.582655 m Far. Artificial ±6 rad/s
turn-rate stresses give the same maxima. The values exceed the simple sufficient
0.4 m collider + approximately 0.159 m route-clearance circle; they do not prove
an actual route collision. The independent route audit considers real headings,
turn slowdown and obstacle height bounds. Talk reaches 0.658700 m and run
0.720125 m Near, so dialogue gestures and dynamic vehicles are explicitly not
certified by the static circular-body route gate.

## Additional independent stress probe

A later independent same-leg/cross-leg proper-triangle probe found residual
trouser self-contact in full deep crouch, with Near triangle witness
12688/13137 and Far 2634/2707 in the frozen model. It excludes physical shared
vertices/edges and restricts original trouser height to (-0.40, -0.065) times
body height. This is a **known failed stress check**, distinct from the passing
apron-versus-trouser and upper-cloth-versus-shirt regressions. Deep crouch is
not certified as self-contact-free, and no cloth-physics claim is made. The
frozen model and failing probe are retained for the coordinated follow-up.

The small bright rear sliver near the lower-right of the bun is a grazing
masked wrap-card tip. Actual ray geometry places the dark HairShortStrands
card in front of an opaque HairCrop cap and skin farther behind it. The
exact-source neutral RGB/retained-alpha rear capture shows the same highlight,
confirming a card/shading limitation rather than an exposed-scalp opening.

## Construction cost correction

The first full final raster aggregate passed 37 selected suites but timed out
in `npc_runtime` at its existing 240-second limit. Separate CPU-clock timing
showed 7.018 seconds Near and 1.140 seconds Far spent constructing Nell before
rendering. The source was not signed off on that aggregate result.

A Nell-only conservative X/Y interval rejection now eliminates impossible
shirt-support ray/triangle pairs before intersection math. It preserves
intersection order and uses padding larger than the accepted edge tolerance.
All emitted positions, normals, colors, UVs, bone influences and triangle
indices are **byte-identical** to the frozen reference at both LODs and all
three tested heights (1.35, 1.75, 2.1 m). No approximate comparison is used.
Construction CPU time fell to 2.278 seconds Near and 0.409 seconds Far in the
same executor. The exact-source refresh passed all 38 selected raster suites, all four
actual llvmpipe suites and ASan/UBSan (leak detection disabled for ptrace).
NPC runtime passed in 137.02 s; no timeout was increased. Six actual rendered
controls, including CPU raster/path, llvmpipe and Ashcourt, also have zero
changed pixels. See PASS_01.md for the excluded old route-fingerprint check.
