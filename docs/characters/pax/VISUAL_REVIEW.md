# Pax rendered review

The preserved final stage contains 126 raw engine captures. Each has the
same compiled source fingerprint listed in `GEOMETRY_REVIEW.md`. The main
side matrix is Near/Far × material/clay × full body/head × front, back,
left, right and four obliques. It also includes above-floor low views, top
body/scalp views, upward face views, 960×1280 body details, idle, walk, run,
crouch, crouch-walk and two talk/head-turn directions. Contact sheets only
resize and label engine pixels; they do not retouch the character.

## Visible improvement and remaining gaps

The original procedural actor had a generic simplified face, segmented
body/clothing and mitten-like hands. The new face has coherent ears, jaw,
lips, eye apertures and a continuous neck. The broad-waisted silhouette
reads separately from the earlier slim characters. Connected fingers,
forearms and cuffs make the hands legible from both sides. The vest now
has actual thickness, an underlap/closure, separate hem, cleared armholes
and fitted tape. Raised knees retain their trouser surface and the cargo
pocket follows its thigh. The boots have an instep and toe instead of the
initial blunt cylinders.

The side and top images also show why this remains a work checkpoint:

- Vest shoulder/armhole cut lines are angular, especially in Far closeups.
  Boundary clearance and closed topology do not make these edges art-ready.
- The opaque gray hair cap covers the scalp but still looks cap-like. Strand
  roots and the bun's gathering lack natural irregularity; Far is noticeably
  faceted at portrait distance.
- Skin has limited color variation and still looks waxy/smooth. The face is
  neutral, with no expressive facial rig, lip synchronization or pores.
- Cloth panels and sleeves remain too rigid and uniformly smooth; the
  authored folds do not replace material-specific cloth deformation.
- Shoes remain stylized/chunky. Fingers are connected but statically curled,
  with no knuckle or individual-finger animation.
- The frame comparisons establish improvements in this engine. They do not
  establish GTA VI parity, finished AAA art or physical-GPU performance.

Official Rockstar Jason/Cal images were studied for connected anatomy,
eye/neck transitions, body proportions and layer contact. Anton Kozlov's
production jacket turnaround was studied for seams and garment volume;
Marmoset's William Paré-Jobin hair breakdown for opaque roots and strand
grouping; actual Portwest and Red Wing product images for vest edges and
boot toe/sole construction. These were private visual references. No source
image, likeness, brand mark, tattoo or proprietary model is a shipping asset.
See `REFERENCE_CRITERIA.md` for source links and specific study criteria.

## Actual world context

The early and final spawn captures preserve the original scene. Pax begins
at (-6, 0.86, 28), radius 0.4, speed 2.2, DayOnly, with the unchanged route
(-6,28) → (8,28) → (8,14) → (-6,14). A continuous 360-frame live run at a
fixed simulated 12 Hz covered 55.995 metres of that 56-metre loop in 30
simulated seconds. Nine other agents also recorded positive travel.
No actor was frozen and no prop or building was hidden or moved.

The scene includes real camera occlusion: a car crosses the view around
frame 34 / 2.75 s, and another actor occupies the foreground around frame
197 / 16.33 s. These pixels remain in the evidence. No static building or
prop intersection with Pax was established by the inspection. Camera
occlusion alone is not an intersection claim or proof of route clearance.

The `--npc-talk` live closeup shows the existing presentation gestures and
head attention. It is not a real Q/dialogue-input test. Existing NPC runtime
and interaction tests passed; no new claim of manual dialogue playtesting
is made. Stage animations are explicitly separate from gameplay evidence.

OpenGL captures use offscreen SDL and Mesa llvmpipe. Encoded 12 fps is a
chosen cadence, not measured live FPS. GL audit renderer-time and triangle
fields are currently zero/unimplemented and are not evidence of zero work.
CPU raster/path costs and the actual 12 m transition are reported separately.

The supplemental final set adds six matched 480×640 CPU-raster stage views,
four Far talk/head-turn views, and the clear actual-world LOD pair. The
before/after sheet uses the original six-view baseline settings rather than
mixing different renderer or framing settings. The Path benchmark images
are intentionally low-sample cost evidence and visibly noisy; they are not
clean art-presentation images.

A supplemental 320×400 CPU path portrait used 4 spp, 3 bounces and 8
accumulated frames (32 samples per pixel). Its actual pixels reinforce the
same critique: the nose/ear/neck are connected, but skin variation is weak,
strand roots cast stiff repeated shadows and shoulder edges remain angular.
It is a material-response diagnostic, not evidence that the art gaps are
resolved. The low-sample live world path image remains visibly noisy.

All six matched CPU views were checked against their baseline JSON for exact
image dimensions, camera position/target, sun direction, pose, LOD, height
and seed; all of those fields match. Every captured stage pose reports a
floor-aligned lower bound, which is a diagnostic-stage convention and must
not be promoted to a new gameplay foot-IK or ground-contact feature.
