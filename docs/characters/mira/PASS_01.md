# Mira Vale: first individual character checkpoint

6 October 2026. `NpcCivA`, commuter, original height 1.75 m.
This is an individual art checkpoint, not finished character art or GTA VI parity.
The [work log](WORK_LOG.md) records at least 120 minutes across 08:37–10:28 and
11:24–11:33 UTC. The publication approval interruption is excluded.

## Substantial changes

Mira now uses a continuous anatomical head/neck with eyelid, nostril, lip, jaw
and ear topology, fitted eyes and anatomical rig pivots. Fury-authored geometry
adds swept hair, a constructed blue jacket, shaped trousers/shoes, fingers and a
leather crossbody bag. Actor identity, height, dialogue, schedule, routes,
collisions and gameplay controllers remain unchanged.

The anatomical source is pinned CC0 graphical data from MakeHuman Community's
MPFB2 assets. Fury's original standard-library importer combines documented shape
targets, normalizes body height, extracts the head/neck and landmarks, and retains
original per-corner UVs. No upstream addon program logic or runtime network asset
service is included. Clothing, hair, accessory geometry, material fields and
integration are authored in Fury. See [exact provenance](ANATOMY_PROVENANCE.md).

A shared original PBR atlas separates skin, hair, eyes, cloth, leather, rubber
and metal. Face pigment follows anatomical positions through the source charts.
Gutters and a bounded mip chain isolate those material regions. The production
CPU raster, CPU ray/path and actual OpenGL paths receive the same geometry and
material data; different lighting/shadow/post-processing paths produce different
pixels.

## Rendered inspection

These are direct Fury framebuffer captures. Only labels and image composition
were added. No AI repainting, retouching, DCC beauty render or substituted model
is used as runtime evidence.

- [Same-camera game face and gameplay scale](../../images/characters/mira/game-comparison.png)
- [Game full-body and three-quarter views](../../images/characters/mira/body-comparison.png)
- [Six face directions](../../images/characters/mira/all-side-faces.png)
- [Six full-body directions](../../images/characters/mira/all-side-bodies.png)
- [Neutral clay form inspection](../../images/characters/mira/clay-sides.png)
- [Walk/run/talk/crouch poses](../../images/characters/mira/poses.png)
- [Matching near/far LOD views](../../images/characters/mira/lods.png)
- [CPU raster/path and OpenGL comparison](../../images/characters/mira/backends.png)
- [Actual AI route before/after video](../../videos/characters/mira/game-motion-comparison.mp4)
- [Five actual motion frames](../../images/characters/mira/motion-frames.png)

All-side, clay and pose sheets are diagnostic engine-stage views under a fixed
world-space light; the light does not follow the camera. Game images and video
retain the playable world. The motion video has 48 frames at a fixed 12 Hz
simulation cadence, lasting four seconds. Its playback cadence is not measured
rendering throughput. Before/after AI runs finish with byte-identical gameplay
state, including Mira's 8.7892084 m route travel and unchanged waypoint/duty data.

Reviewed saved iterations exposed and corrected invalid curve endpoints, tiny
zero normals, twisting frames, a doll-like head, scalp/eye/neck fitting problems,
exposed shoulder stubs, face UV crossings, and strap/crown geometry defects.
Original UVs and shared seam normals fix 128 face seam crossings. The continuous
strap is fitted to actual jacket triangles. Eight-sided shallow hair locks fix
real winding failures; numerical tests were not relaxed. The resumed final review
also tapers the eyebrows and smooths the shared jacket/yoke boundary normals.

References inspected for anatomy, silhouette, hair, material separation and
clothing construction include official [GTA VI screenshots](https://www.rockstargames.com/VI/media/screenshots),
[Lucia](https://www.rockstargames.com/VI/only-in-leonida/lucia), primary Marmoset
[eyes/hair](https://marmoset.co/posts/how-to-create-realistic-hair-peach-fuzz-and-eyes/),
[skin](https://marmoset.co/posts/creating-realistic-skin-toolbag-saurabh-jethani/),
[clothing](https://marmoset.co/posts/rendering-realistic-clothing-and-armor-materials-in-toolbag/)
and the artist's [Astrid](https://ocnbidou.artstation.com/projects/0OPwE) breakdown.
Proprietary reference assets/images are not redistributed. They do not establish
equivalent implementation or visual quality in Fury.

## Remaining visible gaps

Hair still reads as a fitted cap rather than a convincing strand groom. Skin and
eyes remain smooth and lack advanced subsurface/tear-film response. Eyebrows are
simple geometry, shoulder/collar construction needs more natural shape, and thin
seams/hair detail alias at some raster distances. Clothing and bag use the
existing skeleton without cloth/strap collision simulation, so motion can show
contact limitations. There is no new facial expression rig, blinking, lip-sync,
strand simulation or motion capture. Passing tests and elapsed time do not waive
these art-quality gaps.

## Performance and validation

Final geometry is 35,946 triangles / 21,643 vertices near and 15,970 triangles /
9,756 vertices far. Both meshes preserve the source head's feature topology.

The profile uses a 12 m individual LOD threshold and keeps the intact 8,824-triangle
anatomical head in both meshes. [Geometry review](GEOMETRY_REVIEW.md) records final
counts, budgets, hashes and sanitizer coverage. Atlas maps and all stored mips
occupy 16,776,192 bytes once per process, not per actor.

Clean sequential observations used three fresh processes per variant, a fixed
actual-game Mira portrait and four frames per process; values are medians of the
last CPU frame. CPU ray used two workers. These are observations on this cloud
CPU, not a real-time or hardware performance promise.

| Actual game workload | Previous articulated profile | Individual Mira |
| --- | ---: | ---: |
| CPU raster, 640 × 640 | 416.51 ms | 563.88 ms |
| CPU path, 320 × 240, 1 sample/frame, 2 bounces | 326.57 ms | 358.32 ms |

These final-source medians add approximately 35.4% raster and 9.7% path cost
in this view. The earlier pre-refinement observations remain preserved to show
run-to-run variability. The higher-quality
path comparison uses 640 × 640, eight samples/frame, two accumulated frames and
two bounces: its last observed frame was about 14.16 s during capture activity.
This is progressive/offline evidence. OpenGL ran through CPU Mesa llvmpipe;
physical-GPU, Windows/DX12/DXR and physical-speaker execution are not established.

The complete local suite contains 36 tests, including CPU audio, software raster,
CPU ray/path, actual offscreen GL, world preservation, NPC gameplay/mission smoke,
presentation and runtime capture. The profile regression checks 1,188 skinned
poses, three heights, both LODs, exact head/UV preservation, fitted eye/rig
landmarks, unit normals, winding, grounding and stable mesh storage. All 192
fallback cases preserve other identities exactly. Independent ASan/UBSan checks
cover profile/surface/model/animation sources; LeakSanitizer is unavailable under
the executor's ptrace wrapper. The anatomy generator reproduces its outputs byte
for byte. Captured software/ray reports have zero renderer validation errors.

[Local test log](../../validation/characters/mira/pass-01-tests.txt),
[final raw benchmark](../../validation/characters/mira/benchmark-final.json),
[capture hashes](../../validation/characters/mira/MANIFEST.json),
[remaining 17 individual briefs](../ROSTER_PLAN.md).
