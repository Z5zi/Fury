# Hale actual-pixel review

## Fixed-camera form and identity

The complete eight-angle body and head set is reviewed as one unit, with the same world-fixed light. Front and both quarter views show the preserved blue uniform, blue cap, unlettered neutral badge, radio, hip pouch and brown footwear. The source face has a distinct nose/chin profile and full ears; the cap is a fitted cloth crown with a compact layered peak, not the old flattened disc. Both rear obliques and the straight back show complete shoulder bridges, a continuous vest back, the nape and the sewn sleeve junctions. No rear view is omitted because it is less flattering.

Near and Far retain identical anatomical head topology. At matching close cameras, Far is visibly more angular around cap, sleeves, knees and trouser hem, and loses Near laces and cap panel seams. It is a diagnostic magnification of the actual Far mesh, not an assertion that this loss is invisible. Near hands have separate fingers, thumb webbing and shaped wrist transitions; Far hands keep the digit silhouette with much less surface detail.

The neutral-clay set removes color/normal/roughness detail while preserving the hair alpha masks. It exposes the underlying continuous body shapes and raised uniform layers. The cloth remains quite smooth, with simplified broad elbow and knee folds. The tiny procedural texture variation does not substitute for a commercial scanned/sculpted garment.

## Reference comparison and restraint

The inspected high-quality references show richer skin response, local cloth compression, more natural irregularity, small supported accessories and convincing material-specific reflectance. Their proprietary likenesses, textures, logos, armor shapes and scenery are not imported. Hale's source brief stays a simpler fitted security uniform with a modest cap and protective vest.

A late original sleeve experiment removed the uniform elbow bulge and introduced diagonal elbow/cuff compression. Its strict geometry/contact checks passed, but matched profile and rear-running images showed no clear improvement at either LOD. It was rejected and the previous sleeve source restored exactly. The A/B image, experimental source and tests are retained in `out/hale/resumed-review/cloth-experiment`; passing technical tests alone was insufficient for art acceptance.

## Animation and attachment

Forty-eight actual engine frames each cover rear-quarter running and profile crouch-walking in Near and Far. The checked sequence shows the full back and both armholes through arm swing, with continuous wrists, underarm sleeves, side closures and trouser rise. The Far crouch silhouette has noticeably faceted knees. The boots rotate under the weighted ankle underlay; the fingers pass close to the footwear during deep crouch-walking. These observations motivated a complete phase-by-phase contact sweep, rather than accepting the previous eight phase keys.

That denser sweep found eight proper Near vest/shirt crossings at run phase 2/32, in the rear underarm region, at every stress height. It did not find a hand/footwear crossing. This invalidated candidate 12's sparse contact acceptance and prompted an actual fitted-garment correction. The failure is preserved; the predicate and epsilon are unchanged. Final numerical results and final-pixel acceptance are recorded in the pass report.

The stage uses the existing production animator, but these are diagnostic stress poses. Hale's game AI does not thereby acquire crouching, finger movement, lip-sync or simulated cloth. Static closed-mouth facial expression and fingers remain evident limitations.

## Renderer observations

The software raster and actual offscreen OpenGL images give the face more readable smooth shading than the direct ray view. The latter preserves the cap's cast shadow and strong simple highlights, making the eyes appear much flatter. The 32-sample, four-bounce path portrait retains its sampling grain. They are separately labeled; a favorable raster view is not substituted for the ray/path result.

Offscreen OpenGL is Mesa llvmpipe, whose runtime log explicitly disables directional shadows and reflections. There is no hardware-GPU or DXR evidence. Skin has no subsurface scattering or authored commercial facial texture set; hair is an opaque close scalp and masked edge cards, not an anisotropic strand groom.

## Actual world and retained failed views

Matched old/new bank portraits use the real original spawn and unchanged bench, posts, walls and props. The front body is partly hidden by the bench. The side is partly hidden by a yellow bank object; the straight rear capture sees the wall. The entrance camera reveals the whole side silhouette, including unchanged shoe burial: the rendered bank forecourt is 7.6 cm above the guard's y=0 animation sole in both old and new versions.

The two sixteen-second live captures begin at the original spawn. Their camera is the real player camera and causes investigation/chasing. The original following path enters scenery after about five seconds. The street-side path shows several clear running steps, another civilian passing and ordinary traffic, then enters more scenery later. Full clips and failure frames are retained. End-state equality demonstrates preservation of those sampled game states; it is not proof of correct navigation. The baseline guard still chases in a straight line through solids and can overlap other guards.

Ordinary-time 96-second night captures lead the chasing guard far from the bank, and the close frame contains overlapping Renn. A separate clearly labeled mission-4 finale-night fixture retains the original spawn and existing game lighting. It is not ordinary-time progression and does not alter a user's save, source lighting or actor placement.


## Accepted final radio mounting

The expanded radio/vest check exposed an axis-aligned case edge buried in its curved support. The final radio is fitted to that support's actual right/up/outward frame and connected by a closed narrow rear clip. Native 1200×1600 front, quarter and profile pixels in both LODs show the supported case, its grille and antenna, rather than a box pushed out with no visible attachment. Both radio/vest and hands/radio remain clear through the full dense pose grid. Counts are 39,792 Near and 15,986 Far; one redundant Far case row funds the twelve-triangle clip.


## Final ordinary ankle fit and retained stress result

The frozen ordinary fit preserves source anatomy and the original brown footwear. It provides cuff/upper clearance and a shared local ankle bend field; the instep panel and laces inherit their measured support weights. The Far mesh reallocates existing rows through the ankle bend. Actual front/profile standing and running pixels retain a plausible dressed lower leg; the Far ankle and knee remain visibly more angular. Wider-cuff and other unsuccessful experiments were not accepted merely because they reduced a contact count.

Full crouch-walk is intentionally retained as a visibly imperfect diagnostic. Its strict trouser/footwear assertion still fails, 122 Near / 42 Far triangle-pair crossings at phase 6/32. The complete stress videos are labeled accordingly. Ordinary bind, idle, walk, run, talk and attention contact tests pass, as does stationary crouch in the full run. These results are reported separately, with no body shrink, controller rewrite, weakened predicate or disabled default stress test.
