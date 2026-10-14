# Skinned character renderer regression

`tests/character_renderer_tests.cpp` verifies that character skin changes reach
actual backend framebuffers. It complements the anatomical/model tests; it does
not compare screenshots from different rendering algorithms for equality.

## Scope

The test iterates every current `CharacterRole` through both near and far models.
The role count comes from `CharacterRole::Count`, so the bank staff and future
roles are not silently omitted. A tiny fixed scene contains one 1.8 m actor,
a ground plane, an orthographic three-quarter camera, and constant lighting.
There is no world simulation, weather, animated material, HUD, or changing camera.

The default invocation uses hidden SDL dummy-driver windows and the explicit
Software and CPU-ray factories. It checks the resulting backend kind, so an
unavailable renderer cannot silently fall back and produce a false pass. The
software presenter is additionally verified as SDL software, not accelerated.

`--gl` exercises the real OpenGL 3.3 backend. It returns CTest skip code 77 only
when SDL video, an OpenGL window, or an initial 3.3 context is unavailable. A
shader/backend failure after that successful context probe fails the test.

## Exact frame sequence

For each role/LOD/backend combination:

1. Evaluate and render bind pose, and prove the actor changes at least 250 pixels
   compared with the ground-only scene
2. Apply a seeded idle pose and prove its changed vertices reach the framebuffer
3. Apply an articulated gait pose and require more than 80 changed pixels
4. Apply an attention/dialogue pose and require more than 60 changed pixels
5. Reapply the identical dialogue pose: no dirty revision, byte-identical image
6. Restore the idle pose: byte-identical to the earlier idle image
7. Restore bind pose: byte-identical to the earlier evaluated bind image
8. Apply several poses before the next draw: the framebuffer must match the
   final pose, proving intermediate dirty revisions cannot leave stale geometry

The first bind baseline passes through a common translation and back through
identity skinning. This makes both compared bind frames evaluated skin results,
including identical floating-point blend arithmetic, rather than comparing an
unevaluated source vertex with its rounded weighted equivalent. All poses are
restored through `apply_character_pose`; no test swaps a mesh or resets the
backend to conceal cache invalidation problems.

Every readback is checked for its complete 160 x 128 RGB buffer and then repeated
to establish that readback itself does not alter the frame. Every tested posed
vertex/normal is finite, normals remain normalized, and the actor remains above
the ground plane. Mesh identity, vertex/index storage addresses and index values
must survive the whole sequence unchanged.

## Backend-specific checks

- CPU ray uses `RayTraced`, one sample, one bounce, no denoising and no temporal
  accumulation. The fixed sampling seed therefore reproduces the same image
  after restoration. Telemetry must report software ray tracing, actual traced
  rays, one accumulated frame and zero validation errors
- Changing a CPU-ray skin must rebuild exactly one actor BLAS. Reapplying an
  unchanged pose and holding cached frames must not rebuild it
- OpenGL must upload the posed vertex stream into valid VAO/VBO/IBO handles and
  consume `gpu_dirty`. The entire run must leave no GL API error

## Throttled skin updates

Each backend also advances animation at 60 Hz while sampling skin at 30, 15, and
8 Hz. Twenty-four frames per schedule exercise respectively 12, 6, and 3 changed
frames, and 12, 18, and 21 exact held frames. A changed frame advances the revision
once and changes the image. A held frame preserves the revision and every image
byte, while animation state continues advancing from measured distance. The
8 Hz case uses far geometry.

This tests the renderer-facing contract used by the application's presentation
budget. It does not independently test its distance-based scheduling decisions,
NPC routes, or dialogue selection.

## Running

With the CMake target registered:

```sh
./fury_character_renderer_tests
SDL_VIDEODRIVER=offscreen MESA_SHADER_CACHE_DISABLE=true \
  ./fury_character_renderer_tests --gl
```

On an X11 CI host, `xvfb-run -a ./fury_character_renderer_tests --gl` is another
supported launcher. Configure the optional GL CTest entry with
`SKIP_RETURN_CODE 77`. A 90-second timeout and two-process test budget are ample
for this bounded fixture.

During development this was compiled to `/tmp/fury-character-renderer-tests`
against the existing `out/build-system/engine/libfury_engine.a`, with current
model and animation sources compiled directly. No shared build directory was
modified by the test author.

## Verified result

On the cloud Linux environment, all 11 current roles x 2 LODs x 3 backends passed
(66 model/backend cases), as did all nine throttled-update schedules. Software
and CPU-ray ran with SDL's dummy driver. OpenGL ran on the offscreen Mesa llvmpipe
LLVM 19.1.7 context. Same-pose and restored-pose comparisons were byte-exact.

This establishes geometry propagation, cache invalidation, deterministic
restoration, valid readback and level-ground bounds. It does not claim hardware
GPU validation, terrain-aware foot placement, or replace in-game motion/video
review.
