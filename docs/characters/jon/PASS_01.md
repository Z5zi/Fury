# Jon Keel: first individual character pass

6 October 2026. `NpcCivB`, Market, 1.70 m. The dedicated session ran
11:35–13:35 UTC; [the work log](WORK_LOG.md) records its actual progress. This is a
reviewed development checkpoint, not finished AAA art or GTA VI parity.

**7 October follow-up:** [Attention and Far-hair corrections](ATTENTION_FOLLOWUP.md)
remove measured lip shear and coarse-shell scalp patches. Final Far is 15,992
triangles / 9,842 vertices; the historical counts and measurements below describe
the original first pass.

## Individual work and shared corrections

Jon has a distinct CC0-derived anatomical head and neck, with a broader chin,
brow/nose support and neck than Mira. The original chestnut hair, muted-rose
shirt, plum trousers and brick apron palette remains. Fury-authored geometry
adds gathered hair with a connected rear knot, a soft work-shirt neckline,
layered rolled sleeves, shaped forearms/hands, a continuous apron, open-top
pocket, neck loop and rear bow/ties. The eyes use a curved iris/pupil surface.
Near hairline/knot cards use a real alpha mask; far LOD retains the opaque mass.

[Pinned source, recipe and licenses](ANATOMY_PROVENANCE.md) document the new
anatomical data. The importer runs only Fury-authored standard-library Python;
no upstream addon logic or proprietary reference asset is copied into the game.
[Viewed references and design criteria](REFERENCE_CRITERIA.md) link official
Rockstar imagery, professional character-art breakdowns and garment construction.

The pass exposed shared technical limits and corrected them:

- Canonical face UV charts occupied little of the old tile. Uniform repacking
  plus a larger Face allocation gives **2.883× effective density**, approximately
  **252×306 central-face texels for Jon**, with chart-local mip padding. Original
  source UV arrays and provenance remain unchanged
- All ten prior surface tiles retain their local pixels/mips after adding the
  masked strand tile. One shared atlas retains **37,746,432 bytes** for all actors
- Optional three-bone influences let the apron bridge pelvis and both thighs.
  Optional dual-quaternion skinning preserves volume; default generic models
  retain byte-identical legacy behavior
- DQ alone did not fix deep-crouch contact. Reviewed posterior leg shaping and
  a bounded knee blend eliminate the sampled calf/thigh crossings. The same
  corrective follow-up was validated for Mira
- Nonperiodic loft noise had left real seams. Integer angular frequencies,
  explicit closed endpoints and a construction guard fix those gaps

[Surface contract](../../CHARACTER_SURFACES.md), [texel audit](../FACE_TEXEL_AUDIT.md),
[skinning mathematics and limits](../../CHARACTER_SKINNING.md), and
[geometry/pose review](GEOMETRY_REVIEW.md).

## Direct rendered inspection

The images below are Fury framebuffer captures. Layout labels/composition do not
retouch the model. Stage images use the production engine under fixed lighting;
they are labeled diagnostic views, not substitutes for the playable world.

- [Actual-game face and unobstructed body comparison](../../images/characters/jon/game-comparison.png)
- [Actual-game three-quarter comparison](../../images/characters/jon/body-comparison.png)
- [Front/back/both sides/both three-quarter faces](../../images/characters/jon/all-side-faces.png)
- [The same six full-body directions](../../images/characters/jon/all-side-bodies.png)
- [Neutral clay geometry](../../images/characters/jon/clay-sides.png)
- [Walk/run/talk/crouch poses](../../images/characters/jon/poses.png)
- [Mira corrective follow-up poses](../../images/characters/jon/mira-corrective-poses.png)
- [Identical-camera near/far LOD](../../images/characters/jon/lods.png)
- [CPU raster/path and actual OpenGL](../../images/characters/jon/backends.png)
- [Populated street and wider world](../../images/characters/jon/population-comparison.png)
- [Twelve-second actual route/turn comparison video](../../videos/characters/jon/game-motion-comparison.mp4)
- [Motion frame contact sheet](../../images/characters/jon/motion-frames.png)
- [Same geometry/light with atlas versus untextured material](../../images/characters/jon/material-comparison.png)

The motion run has 144 frames at a fixed 12 Hz simulation cadence, including the
first route corner. This is playback cadence, not measured rendering FPS. The
before/after runs finish with **identical complete audited gameplay state**:
Jon travels 22.075153 m, reaches waypoint index 2 and ends near
(33.99957, 0.85, 15.92385), at the original speed and duty state. Talk inspection
is enabled; the existing interaction/controller regression also passes.

Some original inspection-camera angles fall inside a facade, and the following
camera briefly occludes Jon during the video. The video retains that interval.
Clear stills use the same valid side/three-quarter camera in both variants,
without moving the NPC or hiding world geometry. Neutral all-side views provide
unobstructed form evidence. This does not claim general camera collision handling.

## Measured cost and verification

Near geometry: **38,716 triangles / 23,426 vertices**. Far: **15,912 / 9,795**,
with the original 8,876-triangle anatomical head retained and masked cards omitted.
The individual LOD threshold remains 12 m. Colliders, mission/controller state,
actor IDs, heights, routes, schedules and dialogue definitions are unchanged.

Measurements use three fresh processes per variant, four frames each, and the
median final CPU-frame duration on this shared cloud CPU. Ray work uses two
workers. The baseline executable preserves the completed Mira pass; the after
variant includes Jon **and** the shared atlas/deformation improvements. These are
observations with visible run-to-run variation, not hardware guarantees.

| Actual game workload | Before | After | Change |
| --- | ---: | ---: | ---: |
| Jon portrait, CPU raster 640×640 | 591.45ms | 667.83ms | +12.9% |
| Populated Metro street, CPU raster 640×360 | 277.61ms | 292.93ms | +5.5% |
| Populated Metro street, CPU path 320×240, 1 sample/frame, 2 bounces | 237.86ms | 300.31ms | +26.3% |

The higher-quality 640×640 path comparison uses 8 samples/frame, 2 frames and 2 bounces;
its last observed frame was 23.08 s during capture activity. It is progressive/offline
evidence. OpenGL ran through CPU Mesa llvmpipe; no physical-GPU performance,
Windows/DX12/DXR execution or physical-speaker claim follows.

- **38/38 full local CTest suites passed**, followed by both final profile checks
  after the last small hair-card fit adjustment
- Each individual profile passes **1,188 sampled poses**, including **594 focused
  knee-intersection checks** across three heights and both LODs
- Static topology, winding, unit normals, source-head/UV preservation, closed
  pants seams, valid weights, stable mesh storage and 186 unaffected fallbacks pass
- Both actual final profiles pass focused ASan/UBSan; source hashes are preserved
- CPU software, CPU ray and actual GL cutout tests pass, with hostile-neighbor and
  unsafe-mip controls. The strand mask has 18 connected strands and 39.53% coverage
- All captured software/ray reports have zero renderer validation errors; both
  anatomy importers reproduce their generated outputs byte for byte

[Full test log](../../validation/characters/jon/full-tests.txt),
[raw benchmarks](../../validation/characters/jon/benchmark.json),
[verified source hashes](../../validation/characters/jon/sanitizer-source-hashes.json),
[capture manifest](../../validation/characters/jon/MANIFEST.json).

## Remaining art limits

The hairline is softer, but the main hair mass still looks cap-like. Skin remains
smooth and can look gray/waxy under the game's cool lighting. The same-geometry
untextured reference retains that cast, so texture density alone does not solve
it. There is no advanced skin subsurface response, facial expression/blink/lip-sync
rig, individual finger animation, strand dynamics or cloth collision simulation.
The apron uses skeletal approximation and can look stiff in deep crouch;
strongly bent knees remain visually soft/rounded even though the sampled
self-crossings are corrected. Further pose art is still needed.

The intersection checks cover sampled knee/calf/thigh poses, not every possible
cloth/body collision or arbitrary motion. Other sixteen actor IDs still await
individual passes; they retain the earlier whole-population upgrade. Passing
these checks or completing a two-hour allocation does not waive the remaining
visual gaps. [Roster coverage and next work](../ROSTER_PLAN.md).
