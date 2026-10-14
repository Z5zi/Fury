# Tessa Quill — individual character pass 01

This is a first dedicated character-art checkpoint, not finished AAA character
art. Tessa now has distinct anatomical geometry, a fitted commuter wardrobe and
a compact satchel that clears the sampled limb motion. Gray hair, skin response,
cloth drape and facial performance remain visibly simplified. The dedicated
allocation and subsequent corrective validation are complete at this checkpoint.

## Identity, references and provenance

The stable actor remains `NpcCivC`, display name **Tessa Quill**, `Commuter`,
height **1.65 m**, seed `e78507f3`. Her Ridge Pier/east-bridge route, schedule,
controller, collider, dialogue identity and mission state are unchanged. The
mature feminine presentation is an art choice, not an invented biography.

The deterministic linear base palette is retained: skin `(0.79,0.585,0.415)`,
hair `(0.42,0.44,0.42)`, coat `(0.48,0.42,0.28)`, trousers `(0.25,0.21,0.17)`.
Anatomical pigment zones and darker hair roots modulate those colors; a uniform
skin recolor is not being presented as improved anatomy.

[Reference criteria](REFERENCE_CRITERIA.md) records the official Rockstar GTA VI
images and primary character/garment-maker examples actually inspected. These
are references only. No proprietary reference image, likeness, mesh or texture
is imported or redistributed. [Anatomy provenance](ANATOMY_PROVENANCE.md)
records the pinned MPFB2 CC0 graphical data and the original deterministic
importer/recipe. No upstream add-on program code is copied or executed.

The head retains all **4,486 selected source positions and 8,912 triangles**,
including per-corner canonical UVs and source IDs. The accepted near model has
**25,126 vertices / 39,988 triangles**; far has **9,933 / 15,976**, a 60.05%
triangle reduction. Both retain the anatomical head; these are per-character
budgets, not a promise about whole-scene performance. Compared at identical source
landmarks, its chin is 11.42% wider and its cheek span 8.89% wider than Mira's;
all 4,438 common source positions differ. This is not a scaled or recolored
Mira head. The rig fits Tessa's actual eye, neck and head landmarks.

## What changed and what review rejected

- Replaced the generic head/neck with a broader compact skull, distinct cheek,
  jaw, nose, lip and lower-lid forms; seated the eyes against the actual socket
  seams; fitted brows, iris geometry and restrained wet lid margins
- Built a short ochre buttoned coat with sloped shoulders, a fitted collar,
  turned cuffs, welt pockets, hem and rear panel seams; kept shaped palms,
  individual tapered fingers, thumbs and flat-soled footwear
- Authored gray short-crop coverage and tapered boundary cards. An accumulated
  CPU path portrait exposed heavy shadows from raised internal cards, so the
  accepted crop buries their roots and uses a matte short-fiber surface. It
  still has a coarse close-crop silhouette rather than a production hair groom
- Refit the satchel as a compact front-slung bag, approximately 13.2 × 16.5 ×
  7.9 cm, with a rolled flap, quiet rear, side tabs, rings and a continuous
  fitted strap. The earlier hip placement crossed the forearm/thigh during
  walking, talking and crouching; changing its placement is deliberate
- Localized the anterior/lateral knee blend to 0.100 of height, expanding
  smoothly to 0.220 behind the knee. Knee-centered rings keep the original
  triangle count and preserve the standing contour. Pants V coordinates follow
  physical longitudinal distance, so denser knee rings do not compress weave
- Corrected small lower-neck/coat coverage defects found in rendered shoulder
  views, and attached the lowest neck skin to the chest before blending into
  the neck/head. The accepted fit lifts continuous coat rows before querying radial clearance; both LODs pass the expanded coverage checks
- Identified and corrected a CPU two-sided normal-facing defect behind bright
  ear strips. The decision follows geometric triangle facing, matching OpenGL,
  rather than flipping any interpolated normal that points away from the viewer

The shared atlas gains one `HairCrop` tile in previously unused space. The
1536² atlas and retained **37,746,432 bytes** remain unchanged. Every byte of the
previous eleven regions is unchanged across their three maps and seven levels:
**30,406,848 bytes** compared independently. The new tile uses an approximately
0.08 × 0.16 m cap-sector scale and mean roughness 0.641. This is an isotropic
matte approximation, not an anisotropic hair or subsurface-scattering shader.
See [material validation](../../CHARACTER_SURFACES.md).

## Time and iterations

[Work log](WORK_LOG.md): work began **13:41 UTC on 6 October 2026**. The interval
14:41–14:43 is conservatively excluded for the transient capacity interruption.
The two-hour working allocation was reached at **15:43 UTC**. Corrective review
continued after that point and finished at **16:45 UTC**, approximately **182 minutes** of dedicated modeling, review, validation and evidence work after the known interruption is excluded. This is not a claim of 182 minutes spent sculpting alone.
Parallel checks are not added together to inflate the dedicated time.

## Evidence and runtime checks

The final evidence set includes all eight principal face/body directions under
fixed neutral engine lighting, top/low views, neutral clay with production
alpha masks preserved, near/far views, and actual poses. The diagnostic stage
uses the production renderer/model/animator but is explicitly not world proof.

Actual Vaultline captures use the original NPC route and real world geometry.
Before/after camera, fixed simulation time, resolution and lighting settings
match. Traffic and walls obstruct some camera angles; the usable side view is
selected without moving or hiding scenery. The continuous motion recording
retains its occluded sections and records the full simulation endpoint.

The before executable is the preserved validated Jon-pass build. The after
build adds Tessa, HairCrop and the CPU geometric-facing correction. Thus the
comparison is not falsely described as changing geometry alone. GPU evidence
uses actual OpenGL on Mesa llvmpipe, not a physical GPU. The same rig, atlas
regions and alpha masks are used; OpenGL's postprocessing and lighting still
produce different pixels, so this is compatibility evidence, not a pixel-identical
renderer claim. Ray images use the real
CPU BVH/path renderer. Simulated 12 fps video cadence is not measured live FPS.

Reviewed evidence: [actual-game comparison](../../images/characters/tessa/game-comparison.png),
[eight face directions](../../images/characters/tessa/all-side-faces.png),
[eight body directions](../../images/characters/tessa/all-side-bodies.png),
[clay](../../images/characters/tessa/clay-sides.png),
[poses](../../images/characters/tessa/poses.png),
[near/far LOD](../../images/characters/tessa/lods.png),
[CPU raster/path/OpenGL](../../images/characters/tessa/backends.png),
[populated views](../../images/characters/tessa/population-comparison.png),
[material isolation](../../images/characters/tessa/material-comparison.png),
[accumulated path portrait](../../images/characters/tessa/path-portrait.png),
[top/low views](../../images/characters/tessa/top-low.png), and
[continuous 16-second game motion](../../videos/characters/tessa/game-motion-comparison.mp4).

The before and after movie retain the initial truck obstruction and the brief
wall obstruction near eight seconds. Clear walking and turning appear between
these sections. The camera orbits at +90 degrees, 3.2 m from the actor; this is
a recording camera, not a new collision-aware gameplay camera. The complete
recorded NPC/mission state must match, including Tessa's 26.674 m travel distance
at the 16-second endpoint. Diagnostic poses do not imply normal gameplay uses
every animation, and the talk flag is not a complete dialogue-input test.

## Verification scope

[Geometry review](GEOMETRY_REVIEW.md) documents source preservation, socket
coverage, valid winding/normals/weights, periodic seams, stable storage and
renderer handles, grounded soles, deterministic identity and explicit material
sets. The full profile matrix samples **1,188 actual poses** across both LODs,
three heights, six clips and 33 phases, plus **180 explicit head-turn checks**. The triangle crossing audit excludes
coplanar boundary contact and specified intentional garment attachments; it is
not continuous-time cloth collision or a proof about arbitrary poses.

All **39 integrated suites passed** on the corrected source in **113.79 s**.
They cover renderer/material/asset/audio, animation, locomotion, presentation,
NPC gameplay/runtime and the whole-world runtime harness. The final profile
source SHA-256 is `69441a74a30b6e5addad511a11f522a45848d63511fc1f00c753fdf7e9db61f0`.
The focused optimized character suites passed **4/4**; the full Tessa matrix
also passed **ASan/UBSan**, including the added shoulder/head-turn cases. The
source and all recorded test inputs were verified unchanged after those runs.
Exact logs and all source/evidence hashes are recorded with
[the validation artifacts](../../validation/characters/tessa/evidence-summary.json). LeakSanitizer is unavailable under this runner's ptrace setup;
address and undefined-behavior checks are supported. No fresh hosted CI result
or physical Windows/DXR execution is claimed.

## Measured CPU cost

Alternating sequential fresh processes, three repetitions per variant, four
frames each; the table reports the median final CPU render time. The before
executable is the validated Jon checkpoint, and the after executable has all
changes listed above. Ray work uses two CPU workers. This shared cloud sample
is not a hardware guarantee or a complete interactive-game frame-time profile.

| Actual case | Resolution / settings | Before | After |
| --- | --- | ---: | ---: |
| Isolated Tessa portrait | 640 × 640, raster | 54.4 ms | 236.3 ms |
| Populated Ridge street | 640 × 360, raster | 230.3 ms | 219.7 ms |
| Populated Metro street | 640 × 360, raster | 284.3 ms | 325.8 ms |
| Populated Ridge street | 320 × 240, path, 1 spp / 2 bounces | 242.5 ms | 277.7 ms |

The close portrait costs substantially more than the old generic model. Scene
medians vary, including a small apparent Ridge raster improvement that is not
being claimed as an optimization. [All repetitions and settings](../../validation/characters/tessa/benchmark.json)
are retained. These captures do not establish real-time AAA performance on CPU;
the near/far model transition and shared immutable maps bound cost but do not
remove it. The exact final source fingerprint is
`2a29d5197930562a1d41a39ccd6a7f2da62a2f57d5d108596b4de9047e60ebcf`.

## Remaining quality and gameplay limits

The crop remains simplified, cloth is skeletally deformed rather than simulated,
fit is sampled rather than a complete continuous collision solution, and skin
still lacks production subsurface detail. There is no facial blendshape rig,
blink/lip-sync system, finger animation or individual voice performance. Smooth
cloth and shoulder construction remain art targets even when numerical checks
pass. The game retains authored waypoint navigation and flat-ground support;
this pass does not add a navmesh, traffic avoidance or arbitrary-terrain foot IK.

Mira and Jon have earlier dedicated checkpoints; Tessa is the third allocation.
**Fifteen other actor IDs still need their own passes.** The full game is not
being called complete. GitHub publication remains paused at the existing
approval gate; the latest individual source is delivered through verified
recovery artifacts, not described as already published in PR1.
