# Rina Holt: reviewed visual iterations

This record concerns actual production-renderer pixels. Source/geometry tests
support the review; they do not substitute for it. The final source-matched capture set is under `out/rina/final`; the
source-integrity manifest pins all 158 runtime inputs. The embedded fingerprint
is `b2234ce8a46584c2cf0d18fc7447bb0751537c1b60c476a3da80e618c569c992`.

## Baseline actually inspected

The exact Owen-tree Rina model was rendered at all eight world-axis views for
both head and body, 480×640, with fixed stage lighting and framing. It contains
2,216 vertices and 2,248 Near triangles. The renderer reports the generic Dock
profile with no texture maps. Actual-game front, oblique, side, rear and face
views were also inspected at the authored spawn.

Visible defects: a deep rectangular rear scalp gap, an egg-like faceted head,
long cylindrical neck, tiny disconnected facial features, round mitten hands,
straight tubular limbs, two rigid vest slabs with no rear garment, and a block
cargo pocket. These are pixel observations, not an inferred biography.

## Corrections driven by inspection

1. The first connected anatomical model supplied a separate face, seated eyes,
   complete neck, human fingers, continuous shirt, complete vest, tapered legs
   and shaped footwear. Its first cap had a stepped crown and the hands faced
   forward with excessive apparent splay. The model also exceeded the Near
   budget (40,576 triangles), so it was retained as a rejected iteration.
2. Continuous source-height scalp fitting removed the crown step. Rina's hands
   were fitted inward by 58 degrees in an explicit static rest frame. A compact
   round-bound shirt neckline replaced visually awkward collar tabs. Body/head
   turnarounds confirmed full posterior skull/nape, ear clearance, consistent
   wardrobe colors and a quieter silhouette.
3. Actual llvmpipe OpenGL and CPU path portraits exposed different limits.
   Path light made raised short cards appear as dark bars even when raster was
   quieter. Card points were fitted to the actual polygonal cap, with nested
   edge roots and only small exposed-tip clearance. Reducing the opaque root
   too far produced real scalp crossings and was rejected by dense sampling;
   a small measured margin was restored. Hair remains visibly cap-like.
4. High-resolution body pixels revealed that fixed-position laces were buried
   by the revised boot upper. Laces now project onto each actual LOD surface,
   with a separate tongue and narrow welt. The anatomical hands remain static;
   no grasp/finger animation is implied by the stronger silhouette.
5. Running and crouch-walk views were reviewed with actual pose/contact checks.
   A larger armhole and local side ease clear the shirt's full arm swing. A
   shorter curved vest hem clears the raised thigh during crouch-walk and
   stays close to the original compact work-vest length. The shirt's sewn
   boundaries, vest lining and original left cargo attachment are retained.
6. The broad full-pose audit exposed more than the earlier sampled views:
   posterior calf/thigh compression, inner-seat folding, and a Far cargo panel
   crossing the underlying trouser mesh. Knee ring placement, separate front/
   rear knee weight spans, a shallower crotch rise, actual cargo triangle
   correspondence and a properly sampled Far crotch resolve those sampled
   failures. Independent full three-height contact and seam sweeps pass.

7. The first final Far turnaround was rejected: independently gridded pale
   tapes visibly disappeared through the coarse vest faces. The replacement
   tapes are clipped from actual vest triangles, and their narrower central
   placement avoids the high arm-weight transition. Simply raising the old
   placement did not pass motion contacts and was rejected. The final strips
   retain only a 0.00075h bind clearance in both LODs. Independent rays inspect
   576 tape/vest locations per model, and the contact sweep includes these
   real triangles plus a tenth injected crossing control.

## Actual-world review and its limit

A complete 35-second actual-game patrol capture travelled 61.4791 m through the
unchanged four-waypoint circuit. It includes visible building occlusion and
confirmed baseline scene intersections. [The route limitation](WORLD_ROUTE_LIMITATION.md)
records the exact protected building, pillar and car collider evidence. The
actor, props, routes and AI were not moved or hidden for these images.

## Remaining visual limits

The short hair is a fitted cap plus bounded masked fibers. Skin lacks a proper
subsurface model or individual scanned maps. Eyes have fitted static geometry,
with no blink or facial system. Hands have a modeled relaxed rest curl but no
finger rig. Cloth is fitted and skinned, with finite sampled contact checks;
there is no cloth or hair simulation. The face and wardrobe are a substantial
improvement over the generic baseline, not a claim of GTA VI parity.

## Final frozen pixel review, resumed 7 October

The final Near and Far surface/clay head and whole-body turnarounds were
inspected in all eight directions: front, front-right, right, back-right,
back, back-left, left and front-left. That is 64 final source-matched stills
at 480×640. Matching front/oblique/back/left baseline comparisons show the
closed posterior head, socketed eyes, distinct ears, anatomical fingers,
complete rear vest and a continuous trouser seat. No deleted garment panels
or temporary hidden triangles were used for these captures.

The final six-mode pose sheets and three-height review include idle, walk,
run, talk, crouch, crouch-walk and maximum diagnostic attention controls.
Eight new 48-frame sequences cover Near/Far walk, run, talk and crouch-walk.
Phase-spaced contact sheets were inspected from each sequence. The sampled
vest opening and pale tapes remain supported during stride; the crouched
seat is continuous and the armholes retain their lining. The deep crouch is
still an authored skinned shape, with no claim of simulated cloth drape.

Fresh actual CPU-path and OpenGL llvmpipe portraits/body views were also
inspected for each LOD. Path light exposes a dark edge at the coarse Far
hairline and emphasizes the vest thickness. Raster/GL are visually quieter.
The individual face reads consistently across these backends, while the
coarse close crop remains the clearest limitation. llvmpipe is software
OpenGL, not a hardware-GPU performance or visual validation.

Six matching actual-game before/after cameras retain the production buildings,
props and original spawn. Their pixels show the revised actor at the same
location. The continuing patrol evidence intentionally retains all baseline
world overlaps documented separately; model QA does not certify navigation.

Final whole-route refresh: both 420-frame actual-game sequences were recaptured
from the pinned before/after executables at 320×240 and inspected as contact
sheets. Both travel exactly 61.4791092873 m; final gameplay-state JSONs are
identical. The visible building/pillar/car overlaps and fully occluded intervals
remain in both videos. This is explicit evidence of the preserved navigation
limitation, not a claim that the route is collision-free.
