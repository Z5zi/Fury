# Cass fitting review after recovery

7 October 2026, beginning 02:13:15 UTC. Viewed actual fit02 Near front/profile/
rear head and full-body images individually at 640×800. Rendering may be
superseded; source hashes in each report identify those frames.

## Pixel findings to act on

- Hair closes the previous rear scalp gap. Its top is a smooth swept volume
  with short strand marks; the straight anterior fringe reads too uniform
- Dark rounded rectangular lenses are identity-consistent. The bridge is
  present, but the temples flare visibly out beside both ears in front/back
  views. Ends must sit behind the actual ear, not float outside it
- The profile exposes complete source ear/scalp/neck anatomy. The lip and chin
  planes are distinct, without the original generic face primitives
- The scarf reads as a broad ring and an isolated chest tab. The mauve lapels
  terminate below the collar, leaving a teal break. Fit the scarf to actual
  neck/support triangles and develop a more continuous jacket opening
- Jacket shoulder/rear silhouette is smooth but shallow upper back ripples
  and cape-like collar need clay/pose inspection. Avoid adding decorative
  clutter to hide underlying geometry
- Connected fingers are individually visible and reasonably relaxed; inspect
  close thumb web, nails, palm/wrist transition and run/crouch contacts
- Boots show individual upper/sole and instep laces; inspect contact and ankle
  attachment at crouch and walking phases, including Far

## Initial compiled checks

Near: 39,332 triangles / 23,236 vertices. 59 triangles disagree with averaged
normals. Far: 16,156 / 9,532, over its explicit 16,000 cap. Exact head/hands,
source corner UVs, socket depth and opaque scalp tests pass. Reversed-normal
locations cluster at thin lapel edges, eyelid tubes, a crown fan, scarf cut
edges and lens edges. Rebuild first to eliminate stale-binary ambiguity;
then repair authoring causes rather than relax checks.

## 02:17–02:37 source and pixel follow-through

A target rebuild exposed stale fit02 binaries. Surviving source already fixed
thin-shell cut-edge normals and the crown fan, and reduced Far below 16,000.
The remaining nine Near reversals came from joining opposing eyelid arcs when
sorting mixed source points by X. Ordered source arcs repair the local tubes;
all retained Face and hand corners are unchanged.

Viewed all eight Near material head/body and clay body angles, plus all eight
Far material head angles in fit03 contact sheets. Glasses now follow the ear
more closely. Tail/collar pixels expose the same scarf layering problem found
by exact triangle tests; bind-only green checks were insufficient.

Independent layer checks exposed nose-pad/skin intersections, coarse scarf
facets through skin, lapel facets crossing the body/yoke crease, trouser/boot
intersection at rest, and different boot/pant weights during gait. Nose pads
now conform to actual source skin. Raised hems clear the boot at rest; the
boot shaft, tongue, laces and ankle liner now share a graded shin/foot blend.

A local XY-clipping support fitter measures complete overlay triangles against
actual garment triangles, preserving every shared overlay node and triangle.
This corrects lapel/body and scarf/body facet intersections without relaxing
contact tests or adding unsupported source detail. Final contact acceptance is
still pending, including deep crouch and extreme bounded attention.

## 02:38–02:44 seated/attention review

Individually viewed Near crouch front/profile, Far crouch profile, shipping
front/profile head and an extreme bounded attention head frame. The lower
neck scarf gap and front coat/thigh overlap are genuine pixel findings.
The narrower scarf tail removes the earlier collar-tip intersection; rounded
front jacket quarters clear the static crouch but need additional gait review.
Boot shaft weights must follow the lower leg before the cloth cuff, rather
than allowing an entire shoe shell to turn as a rigid foot. No controller or
route changes are used to hide these findings.

The denser Near scarf still crossed source neck facets after lowering its
hidden return. Far's larger fitting allowance is the more robust construction,
so the trial now applies that clearance consistently and gives the yoke a
matching millimetric allowance. The collar cut end tapers to the sewn root
rather than offsetting a second root inside the yoke. A tiny Far eye-helper
sampling reduction failed the existing socket depth tolerance; restore the
previous sampling rather than weaken the test.

## 02:49–03:08 stricter diagnostics

Read actual bind and posed witness vertices, then viewed full crouch-walk front/
profile, revised bind body/head, and a labeled glasses-removed face diagnostic.
The latter supplements the shipping-glasses views and retains the same source
head; it is not an alternative shipping appearance.

A float-only segment predicate produced isolated false interior contacts at a
sewn collar/yoke corner after a rigid Chest transform. Converting the predicate
arithmetic to double removes these numerical witnesses without displacing
geometry or excluding adjacent triangles. The interior tolerance is now 1e-7,
stricter than the earlier 1e-5. Mixed teal/facing root triangles are classified
using all corners, rather than whichever color is listed first.

Independent strict winding inspection found that the first front-quarter trial
made a few body rows nonmonotonic. The replacement interpolates monotonically
from each angular hem point to the fixed underarm row. The normalized source
head/hands and shared rig/controller remain unchanged.

A full crouch-walk trailing shin approaches horizontal. A free wide trouser cuff
can then intersect the sole despite apparently adequate bind-pose clearance.
The trial uses an explicitly fitted tucked ankle ring inside the open boot
shaft. Further lower-leg/boot and jacket/lap checks remain open; no full-pose
contact certification is claimed until the actual current mesh passes.


## 03:35 material silhouette decision

The complete eight-direction material sheets exposed an over-tapered trouser
ankle from the attempt to avoid the physically extreme crouch-walk calf/toe
contact. That trial made the calf look ballooned above a gathered cuff. It is
rejected. Restore the quieter straight tailored trouser leg and keep the
independently useful graded boot-shaft fitting. The extreme controller pose
remains an explicit failed stress case, rather than distorting the ordinary
silhouette to hide it. No claim is made that its cloth contact is resolved.

## Real shop observations

The first production-world still scan retained the original scene and spawn.
The 3.2 m default-side view is mostly blocked by the brown shop geometry;
90° shows only the head above a stall; -90° is wholly occluded; a 14 m camera
lands behind a brick/window facade. The 1.15 m/45° portrait clearly shows the
shipping head, glasses, scarf and jacket in Ashcourt, with the unchanged stalls,
awning and other NPCs behind it. These are genuine scene limitations, not a
reason to move Cass, delete occluders or silently replace game frames with the
neutral stage. Additional elevated views and live-route frames are being taken.

Regular NPC presentation passes a literal 0.f crouch value at
apps/vaultline/main.cpp:6404–6407. The full-crouch-walk contact case is a bounded
stress diagnostic, not Cass's ordinary fence-route state. It remains explicitly
failed. Precise source vertices, bone matrices and pixel witnesses are retained.

## 04:21–04:26 renewed visible review

Viewed the final-source Near material head/body and clay body eight-direction
sheets, Far material head/body sheets, ordinary/stress pose sheet, actual shop
portrait and full-resolution Near front-quarter head and body. Also re-viewed
the original official Raul 01 reference for seated glasses and continuous
hair-root construction (displayed at 2048×1152, not full 3840×2160).

The actual glasses bridge, temples and ear ends are seated and complete. The
face has a distinct chin/lip/nose profile; it remains an original CC0 recipe,
not the reference likeness. Shoulder, wrist, hand and trouser silhouettes hold
across ordinary phases. The coat collar has a finite edge and rests above the
yoke; its broad stylized shape is retained rather than inventing a different
wardrobe. Full-crouch stress remains visibly exceptional.

The front hairline still read too straight, and the overlapping lock patch was
slightly too bright. A small original off-center rise (up to .0038 normalized
height) and reduced lock pigment contrast improve the short sweep without
adding triangles or changing source face geometry. Four new shipping-glasses
views (Near front and both quarters, Far quarter) were inspected individually
at 640×800 in resumed-final/hair-trial. The variant retains complete cap
coverage, exact head and hand source corners, 38,778/15,898 triangles, strict
bind topology, 1,398 finite pose samples, and 3,000 ordinary layer cases. The
default strict seam and full-crouch contact assertions still fail and remain
recorded separately. Earlier final sheets now precede this small hair-only
change; current-fingerprint capture review is still in progress.

## 04:30–04:45 close details, renderers and actual route

Individually viewed all twelve 640×800 shipping-mesh detail frames: Near/Far
hand front, inner and back; boots quarter and side; jacket quarter. The thumb
web and fingers remain one connected anatomical surface at both LODs. Near
retains the source nail folds and relaxed finger curvature. Far visibly loses
small folds and becomes angular at very close diagnostic distance, while the
five-finger silhouette and wrist loop remain intact. No finger animation or
nail texture is inferred from these images.

The boots have distinct uppers, soles, instep laces and continuous ankle
coverage. Far reduces laces from four to two and the toe/heel becomes coarser.
The dark original footwear palette is maintained. The yoke, collar return,
rolled facing, closure, shallow pocket mouths and continuous hem are visible;
no floating sleeves or wrist caps appear in the inspected images.

Viewed current-fingerprint Near/Far material head/body sheets, clay head
sheets, Far clay body sheet and full Near top/low-body, scalp-top and
under-face views. Scalp coverage is complete at the crown and nape. The
shipping glasses retain bridge, rims, lenses, hinges and temple ends in both
LODs. The separate labeled glasses-removal views confirm fitted eyes, lids,
ears, nostrils, lips and chin underneath; they are not shipping alternatives.

An actual software-renderer regression initially compared a restored evaluated
bind pose to unevaluated source normals. Far differed in one pixel by one
8-bit channel level; positions were identical, and normal renormalization was
1.46000971313e-7 at most. The fixture now establishes bind through skinning,
matching the existing shared renderer regression. Byte-exact held frames and
restoration assertions remain. Software, CPU ray and actual GL/llvmpipe pass
12 motion/attention samples per LOD plus exact hold/restoration. The original
failure and the exact measured difference remain in resumed-final.

The new 216-frame route and 48-frame attention runs use the actual unchanged
Vaultline scene and controller. The 0.5-second route review sheets show clear
walking/turning around 4.5–10 seconds and a brief rear view around 14.5 seconds.
The inherited shop/facade occluders obscure many other angles. The raised shop
view shows most of the body; the rear view shows the full back; the close
45-degree portrait is clear. None of this justifies moving the actor, deleting
scenery or presenting neutral-stage images as game-world captures.

All ten final actor states, entire world states and capture parameters match
the accepted baseline. Twenty-five Vaultline source files and 162 shared
runtime/world-asset files match the accepted base exactly. These checks cover
route, shop/dialogue source, controller, scene inputs and rendering; they do
not claim a literal shop transaction or dialogue branch was operated.

## 04:52–05:12 combined-attention correction in progress

The new ±0.9 yaw/±0.4 pitch front/profile closeups expose a genuine stretched
chin and protruding edge that finite-pose and garment-intersection checks did
not detect. This blocks final visual acceptance. The first source-spatial jaw
blend corrects the visible exterior but fails scarf/skin contacts. Its exact
source was reconstructed and verified against the capture fingerprint, then
preserved as rejected evidence in jaw-fit. A shorter global blend also fails
neck/scarf contact and is preserved in jaw-fit02.

Cass differs from the Jon lip-only defect: its 38-position lip seam is already
above the old .384 blend end, at y=.387203–.389113. The internal-mouth chart
contains 418 positions and 484 runtime UV corners and descends to y=.365186.
A mouth-only rigid-Head correction has zero error in nine attention poses and
rejects the original weighting (maximum error/h .015919), while all ordinary
layer checks pass. But its pixels retain the exterior chin distortion, so it
is also insufficient; source and images are saved in jaw-fit03.

The current iteration retains the full rigid mouth, gives the forward exterior
jaw its own transition below the measured chin and fits scarf influences to
actual anatomical support-triangle barycentric weights. This removes ordinary
talk/yaw and upward-look skin/scarf intersections. Remaining down-look contact
is at the top front band, where the now-correctly-moving chin meets the scarf.
The front band is being fitted below that envelope. No source position, source
UV, rig joint, controller, route or threshold is being changed to pass a test.

## Converged fitting and final-source review

The accepted front band uses high=.359−.011·front and the unchanged lower edge,
with weights sampled from actual neck support triangles. The complete mouth
remains rigid to Head; the forward exterior jaw transition completes below its
measured envelope. The original 58-point lower source attachment has maximum
y=.337727 and retains its old weights. No canonical vertex, UV or triangle was
changed. The accepted fitting is jaw-fit06; the authoritative recaptures use
core fingerprint 2af8d6083e602a537b163ac56303c34209c595676e73eae2af54bd79fdc6e05f
in accepted-final.

Viewed all 32 final bounded-attention head captures through the Near/Far front,
back and both-profile sheets, plus full-resolution bind fronts. The stretched
chin and projecting edge are gone; the scarf stays below the chin during down
look and remains continuous during upward/sideways attention. The mouth-only
trial was correctly rejected despite its green ordinary checks. The independent
308-point exterior-jaw regression now prevents that false acceptance.

The final OpenGL, software and four-sample/two-bounce CPU-ray portrait panels
retain the same silhouette, glasses, hair root and palette. CPU-ray lighting and
noise differ visibly; no pixel equivalence or hardware-GPU parity is claimed.
The explicit Near/Far pairs at 11.5, 12 and 12.5 m keep the identity readable
and show no material silhouette loss at the displayed scale. These are
forced-LOD comparisons, not an automatic-switch animation.

Current source/corner/topology/opaque-scalp checks, all 1,398 finite pose samples,
all 3,000 ordinary layer cases, nine-state complete-mouth/exterior-jaw tests and
negative controls pass. The unchanged strict seam and full-crouch contact
assertions still fail. Fresh software, CPU-ray and actual GL/llvmpipe renderer
checks and the ten relinked common regressions pass. Final sanitizer records
are kept separately from the release/default test results.
