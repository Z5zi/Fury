# Owen Pike — individual character pass 01

**First dedicated checkpoint completed 6 October 2026 at 20:13 UTC.** Owen now
has distinct anatomical geometry, a sewn jacket, a fitted side satchel and
verified live motion. This is a tested individual art checkpoint, not finished
AAA character art. Skin, hair, cloth and facial performance remain simplified.

## Identity and substantial changes

Owen remains `NpcBankCust`, `Commuter`, height 1.74 m, deterministic seed
`0x485fc311`. His Meridian Mutual lobby route, schedule, speed, collider,
dialogue and mission identity are preserved. The blue-gray jacket,
olive-brown trousers, auburn close crop and brown side satchel retain the
existing appearance. The individual anatomical recipe is art direction,
not an invented biography.

The connected CC0-derived head retains 4,368 source positions, 8,680 triangles,
all five UV charts and original per-corner UV correspondence. It has a narrower
skull, tapered chin and restrained nose width distinct from Jon, with eyes
fitted to its actual eyelid seams. A continuous 54-edge skin underlay joins
the imported neck cutoff to a hidden lower boundary. The new original jacket
has a sewn torso, yoke and sleeves with shared physical armhole boundaries,
turned cuffs/hem, shallow pockets and a seated collar. The hands, trousers,
knees, shoe uppers and soles have shaped geometry rather than the original
capped primitives.

The opaque close crop and short boundary cards use an additional physically
scaled `HairShortStrands` region in previously unused atlas space. The previous
12 material regions remain byte-identical across all three maps and seven
retained levels: 31,455,360 bytes independently compared. The complete atlas
allocation remains 37,746,432 bytes. Reduced short-card relief and roughness
quiet the old bright tips; the result remains a simplified cap/card hairstyle.

The satchel has a rounded gusset, finite rolled flap, attachment tabs, rings
and a continuous front/shoulder/back strap. Its complete flap uses one uniform
skin transform so the separated layers do not shear through one another.
The accepted side-facing fit uses a modest carrying bind pose: the shoulder
stays fixed, the right elbow moves outward by 0.032h and the wrist by 0.025h.
Segment-aligned deformation carries the sewn sleeve and hand coherently; the
original animation clips and skin influences remain. The inner yoke supports
the collar from Chest rather than inheriting sleeve motion beneath its rim.
The opaque Far cap initially cut through the convex scalp between its coarse
vertices. The final cap receives a 0.003h radial envelope through the existing
crown taper, moving160 Far cap positions by at most5.22 mm at 1.74 m. It keeps
all triangle counts and the exact height; Near and every other surface remain
bit-identical. A new actual Face/cap crossing and source-surface coverage check
rejects the old source. Matched renders confirm the sliver is gone; Near pixels
are identical and the silhouette change at 12 m is small.

A prior front fit was explicitly rejected despite passing contact tests:
its back center floated roughly 73 mm from the coat. Numerical nonintersection
alone is not visual acceptance.

## Reference study and provenance

[Viewed reference criteria](REFERENCE_CRITERIA.md) records actual official
Rockstar GTA VI imagery, the character artists' published breakdowns, and
jacket/satchel makers' images used to inspect anatomy, hair coverage, garment
construction and attachment. Proprietary images and likenesses are not
shipping assets. [Anatomy provenance](ANATOMY_PROVENANCE.md) records the pinned
MPFB2 CC0 graphical inputs, independent original importer and character recipe.
No upstream add-on program code is copied or executed. This checkpoint does
not claim GTA VI fidelity.

## Actual-world defect found during review

Owen's original swept lobby route intersected both inner waiting chairs.
The bounded correction moves `MMChair1` and `MMChair2` together with their
colliders from Z −4.9 m to −3.95 m, preserving X, scale, yaw, solid flags,
identities and the 1.30 m central aisle. The exact 1,871-entity comparison
changes only those two Z coordinates. [Lobby clearance](LOBBY_CLEARANCE.md)
records the actual radius, all route segments and original/corrected scene
checks. The baseline and a one-chair-only correction each fail the regression;
the two-chair correction passes. An existing guard route still intersects a
chair before and after; this pass does not rewrite navigation.

The live recording camera influences nearby security because the game treats
it as the player. The original eye-height view became blocked by approaching
guards. A bounded diagnostic `--npc-elevation` option rotates the existing
camera offset while preserving its radius/look-at; default 0 leaves the old
pixels and gameplay state exact. At30 degrees elevation,45 degrees orbit and
3.2 m distance, the selected 16-second live sequence keeps Owen's head and torso
visible throughout. Foreground vehicles/scenery intermittently obscure his
legs. Guards and all scenery remain active. The clean Tessa-era before build
receives only this same camera patch for a matched comparison; all 966 original
source-tree files were checked and only main.cpp differs. The earlier blocked
views are retained as intermediate evidence, not hidden or called a full
visible route review.

## Evidence, validation and measured cost

The final source fingerprint is
`62952af167c952d605a68e2206bc24d143847f0ed644cc8759f8bd9f07adacd3`.
Owen source is `1ae91ff9558309c3a4930978b8acdfed3ad7cad296fb12c6d9d35872ff656027`;
the jacket header is `800652894e683f1882152f2a3b781acf2abfba717c44abba3117e40b7e4fb09d`.
Near has **24,622 vertices / 39,620 triangles**; Far has **9,813 / 15,996**,
a 59.63% triangle reduction. Both retain the individual anatomical head.

All **40 integrated suites pass**: 36 non-GL suites in 150.38 seconds and all 4
actual offscreen OpenGL suites in 11.11 seconds. The expanded focused profile
suite passes optimized in 27.53 seconds and ASan/UBSan in 194.96 seconds,
including **1,188 ordinary poses plus 180 head-turn samples** across both LODs
and three heights. Its frozen test hash is
`f70d6e5504bc1cbb41c98e1ffa6590c7c38f2ad38cd9f8524c70fc5ac801eaac`.
The historical faulty cap fails the new geometry/coverage negative control;
the corrected Far cap's minimum sampled clearance is about 0.671 mm at 1.74 m.
LeakSanitizer alone is unavailable under the established ptrace restriction;
address, undefined-behavior and allocation-mismatch checks remained enabled.
[Geometry review](GEOMETRY_REVIEW.md) describes exclusions, finite sampling,
intentional attachment contacts and local deformation strain. Passing those
checks does not certify realistic art or continuous cloth collision safety.

Reviewed evidence includes [actual lobby comparison](../../images/characters/owen/game-comparison.png),
[eight face directions](../../images/characters/owen/all-side-faces.png),
[eight body directions](../../images/characters/owen/all-side-bodies.png),
[clay](../../images/characters/owen/clay-sides.png),
[poses](../../images/characters/owen/poses.png),
[near/far](../../images/characters/owen/lods.png),
[12 m cap transition](../../images/characters/owen/cap-correction-12m.png),
[the cap closeup correction](../../images/characters/owen/cap-correction-closeup.png),
[actual world backends](../../images/characters/owen/backends.png),
[populated views](../../images/characters/owen/population-comparison.png),
[material isolation](../../images/characters/owen/material-comparison.png),
[top/low review](../../images/characters/owen/top-low.png),
and [continuous 16-second actual-game motion](../../videos/characters/owen/game-motion-comparison.mp4).
The [separate stage walk](../../videos/characters/owen/stage-walk.mp4) is explicitly
a diagnostic animation view. Encoded 12 fps is simulated cadence, not measured
live frame rate. All pre-existing agent, crew and heist fields match at the
before/after full-route endpoint, including Owen's 9.2413 m travel distance;
the new radius audit field is independently verified. The rendering uses the
real authored lobby, ordinary AI and unchanged route. No actor or scenery is
hidden to improve the evidence. See the [evidence summary and exact commands](../../validation/characters/owen/evidence-summary.json).

The before build is the validated Tessa first pass, with only the same camera
control added for the elevated sequence. The after also includes the short-hair
material, two chair placements and [Tessa's separate flap correction](../tessa/FLAP_CORRECTION.md).
Historical Tessa reports/images still describe her original checkpoint; Mira
and Jon's model sources remain unchanged. The [shared skin-normal experiment](../SKIN_MATERIAL_REVIEW.md)
is unshipped, not silently applied to these images.

Alternating fresh before/after processes, three repetitions and four frames
per case; the table shows median final CPU rendering time. Other heavy local
build/render work was paused for this short window. These remain shared-cloud
observations, not hardware guarantees or whole interactive frame-time profiles.
All repetitions are retained in [benchmark.json](../../validation/characters/owen/benchmark.json).

| Actual case | Resolution/settings | Before | After |
| --- | --- | ---: | ---: |
| Isolated Owen portrait |640×640 raster|54.3 ms|265.4 ms|
| Populated lobby |640×360 raster|319.4 ms|309.6 ms|
| Populated Metro street |640×360 raster|272.7 ms|275.8 ms|
| Populated lobby |320×240 path, 1 spp, 2 bounces, 2 workers|360.3 ms|384.9 ms|

The detailed isolated portrait is substantially more expensive. Small scene
changes vary between runs; the slightly lower lobby raster median is not an
optimization claim. These observations do not establish real-time AAA CPU
performance. The [actual interior path image](../../images/characters/owen/world-path-128.png)
accumulates 128 samples/pixel over four 32 spp frames at 320×320 and 2 bounces;
its last accumulation frame cost 16.43 seconds. It remains grainy and darker
than raster. The [32-sample stage portrait](../../images/characters/owen/path-portrait.png)
uses different authored lighting and cannot replace the real interior result.
OpenGL is actual Mesa llvmpipe, whose engine mode disables directional shadows
and reflections; its zero timing fields are unimplemented metrics, not measured
zero cost. No physical-GPU, pixel-identical renderer or Windows/DXR claim is made.

## Time and remaining limits

The [work log](WORK_LOG.md) begins at **16:47 UTC**. The genuine two-hour
allocation reached **18:47**, before parallel work on Lia began. Corrections
and verification continued until the **20:13** checkpoint, a 206-minute elapsed
work window. The later interval includes shared inspection-camera tooling,
validation, evidence and roster coordination; it is not 206 minutes of sculpting.
Helper time is not added together, and packaging after 20:13 is not charged as
extra modeling time. Lia's independent pass began 19:04:28 and Nell's 20:16:29;
neither is silently counted as completed Owen work.

Skin still lacks a production subsurface model and face-specific scanned
material detail. There are no facial blendshapes, blink/lip-sync system,
individual voice performance, finger animation, cloth simulation or hair
physics. The skeleton and authored waypoints remain simplified. CPU closeup
cost, populated-scene cost and LOD changes must be measured rather than inferred
from video cadence. OpenGL evidence uses Mesa llvmpipe; it does not establish
physical-GPU or Windows/DXR performance. A fresh manual end-to-end heist and
hosted CI run have not been completed for this pass.

GitHub publication remains independently paused at the existing approval gate.
The last verified public source is `348bdd900482b45918d753d17dc08e284a095453`.
Local source and verified recovery artifacts are not described as already
published in the draft pull request. Four first checkpoints are complete; Lia and Nell are in separate active passes,
and twelve IDs have not yet started. The roster and full game remain incomplete.
