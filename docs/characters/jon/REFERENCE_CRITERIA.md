# Jon Keel: reference study and original design criteria

Study date: 2026-10-06. This is a design brief, not a claim that the model
already meets these criteria or matches the fidelity of the reference games.
Actual image pixels were downloaded and viewed, including profile and rear
views where identified below. Reference artwork remains outside tracked assets;
this document contains source links and observations only.

## Identity constraints

The [roster brief](../ROSTER_PLAN.md#5-jon-keel--npccivb--first-pass-checkpoint) and
[original character builder](../../../engine/src/character_model.cpp) establish
`NpcCivB`, `Market`, 1.70 m, chestnut rear bun, muted-rose shirt, plum-gray
trousers, brick apron and a relatively narrow waist. They do not establish a
biography, age or ethnicity. Adult male-presenting anatomy is the art direction
for this pass, not a newly discovered gameplay fact.

Keep the stable appearance seed `e6850660`. Original linear RGB base values are
skin `(0.940, 0.765, 0.635)`, hair `(0.360, 0.220, 0.105)`, shirt
`(0.490, 0.340, 0.370)`, trousers `(0.160, 0.150, 0.200)` and apron/accent
`(0.440, 0.190, 0.140)`. Lighting and small material variation may change how
these render; the reference images do not authorize replacing this palette.

## Viewed references and what they actually show

### Rockstar Games: Grand Theft Auto VI

Source: [official screenshot gallery](https://www.rockstargames.com/VI/media/screenshots).
Viewed 3840 × 2160 files: **Jason Duval 01, 02, 03, 05** and **Cal Hampton 01,
02, 03, 04**. These are perspective scenes under varied lighting, not neutral
orthographic measurement references.

- **Jason 01–02:** the jaw has a lower plane distinct from the throat; neck
  forms widen into the upper chest. Eyelids sit inside the brow/cheek structure.
  Nose, lips and chin have separate changes of depth rather than pasted-on dots.
- **Jason 03:** useful head three-quarter/profile, ear-to-jaw relation, narrowed
  wrist, hand gripping a fishing rod, and cloth compressed along a bent elbow.
  The white sleeve is gathered; this image alone does not establish a folded
  shirt-cuff construction.
- **Jason 05:** fingers wrap individually around the object; knuckles and nails
  remain subordinate to palm and finger volumes. Dramatic light exaggerates
  contrast and cannot supply a neutral skin color.
- **Cal 01–04:** relaxed hands, fuller torso and different shoulder/neck
  transitions demonstrate variation among adult figures. Shirt hems lift and
  fold around the body. Cal's build, tattoos, jewelry, facial hair and clothing
  graphics are not Jon's design.

### Sander Flisijn: Bun (Realtime Hair)

Source: [artist's bun presentation](https://sanderflisijn.artstation.com/projects/ZaAVNx).
Viewed **bun-00**, **bun-01** and **bun-wireframe**, each 3840 × 2160. Together
they show both sides, rear three-quarter views and the underlying card layout.

Hair follows several broad curved flows from temples, crown and nape into a
single gathering region. The bun is a wrapped, overlapping form with a narrower
root and an uneven outline. Fine escaping strands soften an otherwise clear
silhouette. The wireframe separates the layered scalp coverage from the curved
pieces that construct the knot. This informs direction and volume hierarchy;
it does not imply that Fury uses that artist's hair-card shader or topology.

### Sander Flisijn: Mafia III janitor uniform

Source: [artist's production work](https://sanderflisijn.artstation.com/projects/Wva3Q).
Viewed **paez-01** front/rear/three-quarter presentation, **paez-04** and the
**janitor** in-game image, each 3840 × 2304. The latter two have occlusions and
serve as pose/context references only.

The turnaround shows the garment wrapping completely around the body, including
shoulder seam, rear waist, pocket rims, trouser side seam, ankle folds and soles.
Large folds concentrate around elbows, waist and trouser bottoms; comparatively
quiet cloth remains between them. Jon needs the same completeness of
construction, without adopting the jumpsuit, branding or uniform identity.

### Darcy Korch: The Last of Us Part I, Joel summer costume

Source: [artist's costume presentation and production credits](https://dkorch.artstation.com/projects/Le1rnv).
Viewed **joel-half** (1600 × 1276), with front and back, and **joel-full**
(1600 × 1066), with front, three-quarter and rear. Korch credits the shirt and
hair to his work with production support, and the face, beard, pants and arms to
Frank Tzeng; this is a team result.

The cuff roll has layered, irregular folds and a thicker terminal edge; each
arm differs slightly. Exposed forearms broaden below the elbow then narrow at
the wrist. Collar leaves turn away from the neck; placket, pocket openings and
hem have thickness and attachment. Jon should retain these construction cues
without adopting Joel's face, beard, age cues, plaid, damage or equipment.

### Merchant & Mills: Workaday apron

Source: [maker's product photographs and construction drawing](https://merchantandmills.com/us/the-workaday-apron-pdf).
Viewed all eleven gallery images: laid-out apron, front poses, side walking
views, rear bow and front/back drawing. These are garment references, not
body/identity references for Jon.

The bib transitions through curved underarm cuts into the lower body. A neck
loop carries the upper corners; waist ties begin at the side edges and can
fasten behind the body. The rear photograph makes the uncovered back, crossing
bow and dangling flat ends clear. Pocket openings form a visible lip and pull
the surrounding fabric when occupied. The heavier tan apron keeps broad planes
and a firm hem; softer red/striped versions have more local wrinkles. The maker
lists canvas, twill and other woven fabrics and optional 25 mm tape for ties.
Jon's shorter apron is an original proportion choice rather than a copied
pattern or an assertion about its fabric specification.

## Original decisions for Jon

These are authoring choices derived from the study, not measurements copied
from any proprietary character or garment.

1. **Body and head:** retain the compact 1.70 m silhouette and narrow waist.
   Build a connected torso with an upper back, rib cage, waist and pelvic
   transition. Use a gently tapered, moderately broad head with a squared but
   softened chin, distinct cheek planes and a subtly uneven brow. The nose
   should have a bridge, rounded tip and small alar transition. Recess the eye
   opening into the face; keep eyelids and lips low-relief, not floating plates.
   Preserve plausible head depth in profile rather than stretching the face to
   fill the front view.
2. **Neck and exposed anatomy:** replace the straight tube with a throat that
   narrows under the jaw and spreads toward the clavicles/trapezius. Keep neck
   tendons understated. Give the elbow a bony turn, the forearm an asymmetric
   muscle belly, and the wrist a visibly smaller cross-section. Form palms as
   flattened, slightly cupped volumes with a thumb root, staggered finger
   lengths and soft knuckle breaks. Hands must read at conversational distance
   without exaggerated nail or vein lines.
3. **Chestnut tied hair:** use a fitted base mass that follows the skull with
   recession around the ears. Gather crown, side and nape flows toward a
   compact rear knot. Form the knot from three or four overlapping bent masses
   with a visible constriction and dark tie at the root. Keep its center behind
   the upper occipital region and its highest point below or near the crown,
   preserving the rear-bun identity. Add only a few tapered temple/nape locks.
   A shaded groove may reinforce a major flow; evenly spaced cords covering
   the entire scalp would overwhelm the hair mass.
4. **Rose work shirt:** construct a small open collar with two independent
   leaves, a restrained central placket, shoulder seam and a back yoke. Roll
   each sleeve to just below the elbow with two or three overlapping fold
   bands. Vary the roll height and tilt subtly between arms. The cuff should
   sit around the forearm, with an inner shadow gap and a turned edge, rather
   than resemble a rigid bracelet. Gather only a few compression folds above
   the roll and around the tucked waist.
5. **Brick apron:** make a continuous bib-to-skirt surface with shaped underarm
   edges and a narrow, slightly bowed top. Continue the neck strap visibly
   around the back of the collar. Wrap the waist edges around the hips and
   attach two flat straps to a small rear bow with distinct overlapping loops
   and unequal hanging ends. Use a shorter, practical skirt ending above the
   knee so it preserves Jon's original compact silhouette. Give the lower
   panel a few broad vertical folds, slight side flare and a doubled hem. A
   single broad patch pocket remains legible, with a raised open top, side
   seams, bottom thickness and a small central division. Keep the pocket off
   the hands' swing path.
6. **Plum-gray trousers and shoes:** form a real waistband, fly, side pockets,
   seat and seam path; maintain the narrow-waist identity under the apron.
   Use quiet thigh planes, restrained knee shaping and a few folds above the
   shoe. Shoes need a distinct toe, heel, vamp and sole thickness visible from
   side and rear. Avoid a uniform tube leg feeding into an oval boot.
7. **Material hierarchy:** the shirt is softer than the apron. Preserve broad
   brick cloth planes and use seams/edge thickness to communicate weight.
   Hair is darker in crevices with restrained directional highlights; skin is
   smoother and less contrasty than hair or cloth. Surface detail must support
   these material differences without camouflaging weak silhouette or anatomy.

## Review criteria for the implementation

- Neutral full-body front, left, right, rear and three-quarter views must show
  the same connected character and stable identity. Include head, hands, cuffs,
  waist fastening and hem close-ups. A flattering lit front view is insufficient.
- In side view, distinguish forehead, nose, mouth, chin, throat, rear skull,
  tie and bun. The bun must have a supported root and visible depth rather
  than read as an unrelated ball or a flat disk.
- Trace the neck loop, waist ties, pocket edges, collar and sleeve rolls without
  unexplained terminations. The lower apron should read as hanging cloth from
  both front and side, while the shirt remains visible across the back.
- Inspect a walk, torso bend and conversational gesture for apron/leg contact,
  hand/pocket clearance, cuff/elbow separation, wrist continuity and collar/neck
  intersections. Still-image success does not establish deformation quality.
- Check the face and apron at normal gameplay distance as well as close up.
  Seam detail is secondary to readable head planes, hands, cloth volume and
  the chestnut/rose/brick/plum color arrangement.
- Record specific remaining limits in the pass review. These references set
  qualities to study; they do not support claims of AAA parity, copied physical
  measurements, complete animation coverage or a finished Jon asset.
