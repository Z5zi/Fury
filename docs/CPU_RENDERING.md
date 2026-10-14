# Portable CPU rendering (5.7)

Fury now has two explicitly selectable CPU renderers. Neither requires a GPU,
OpenGL context, Direct3D, DXR, CUDA, FSR, or XeSS. Both run with SDL's dummy video
driver in automated tests. The existing OpenGL and Windows DX12 backends remain.

This is an engine/prototype upgrade. It does not turn the current box-heavy city,
64×64 texture library and mostly static assets into a finished AAA game or MMO.

## Choose a renderer

| Backend | Command | Appropriate use |
|---|---|---|
| Software rasterizer | `vaultline --soft` or `FURY_RENDERER=software` | Portable game fallback, per-pixel materials, low-resolution interactive use |
| CPU ray tracer | `FURY_TRACE_MODE=ray vaultline --cpu-ray` | Actual visibility/shadow/reflection/refraction rays; direct-light preview |
| CPU path tracer | `FURY_TRACE_MODE=path vaultline --cpu-ray --photo` | Progressive static previews with multi-bounce indirect lighting |
| OpenGL | Existing default when available | Existing GPU/llvmpipe raster path |
| Windows DX12/DXR | `FURY_RENDERER=dx12` | Existing hardware ray/path tracing and vendor upscalers |

Use a modest resolution on CPUs. For example:

```sh
FURY_AUDIO_BACKEND=cpu FURY_TRACE_MODE=ray ./build/apps/vaultline/vaultline \
  --cpu-ray --width 640 --height 360 --spp 1 --bounces 3

# Deterministic real game capture with a frozen scene and no HUD
FURY_AUDIO_BACKEND=cpu FURY_TRACE_MODE=path ./build/apps/vaultline/vaultline \
  --cpu-ray --photo --view storefront --no-hud --width 640 --height 360 \
  --spp 4 --bounces 6 --frames 16 --capture out/storefront.ppm

# Coastal fixture: 16 frames × 4 samples = 64 accumulated samples per pixel
./build/apps/renderlab/fury_renderlab --backend cpu-ray --mode path \
  --width 640 --height 360 --spp 4 --bounces 6 --frames 16 --warmup 2 \
  --hidden --capture out/coastal.ppm --report out/coastal.json
```

`FURY_CPU_THREADS=1..64` sets the ray-worker count; default is at most four.
There is no universal frame-rate promise: scene complexity, resolution, samples,
bounces, transparency and CPU affect cost. More workers can contend with other
programs. `--spp` is 1..64 and `--bounces` is 1..16. `FURY_TRACE_MODE=ray|path`
overrides the saved Vaultline trace setting; the settings screen can switch modes.
FSR/XeSS are rejected on CPU rather than displayed as working options.

`--frames N` uses a fixed 1/60-second simulation step for reproducible bounded
captures; normal interactive timing is unchanged. `--capture file.ppm` requires
`--frames`. `--photo` skips the intro and freezes simulation, day/night lighting
and render animation time, allowing static samples to converge. Camera movement
still resets history. `--view bank|storefront` selects a reproducible photo camera.

## Software rasterization changes

- Six-plane homogeneous clipping, including triangles crossing the camera/near
  plane. The huge ocean plane no longer disappears when a corner is behind you
- Perspective-correct world position, normal, UV, color and opacity interpolation
- Per-pixel GGX/Schlick metallic-roughness lighting, linear-light bilinear albedo
  and emissive sampling, material maps and UV-derived normal-map tangent frames
- Inverse-transpose normals under nonuniform scale and mirrored tangent handedness
- Alpha masks, opaque depth and sorted translucent compositing; HUD/readback
  flushes pending transparent geometry before overlaying the UI
- Indexed vertices transformed once per draw, trivial clip acceptance/rejection
- SDL software presentation, truthful dimensions/triangle/instance/CPU timing

Intersecting transparent triangles still use conventional depth sorting, not an
order-independent transparency algorithm. Software rasterization does not gain
ray-traced shadows or reflections; select `cpu-ray` for those.

## CPU ray/path implementation

- Immutable mesh-local triangle BVHs shared by instances; geometry revisions
  rebuild only affected BLASes. A top-level BVH is reused while instance bounds
  are unchanged, then rebuilt when necessary
- Object-space traversal keeps transformed ray directions unnormalized so hit
  distances remain correct with nonuniform scale. Mirrored sidedness, alpha masks,
  primary near/far clipping and transmissive exit faces are covered by tests
- Real shadow rays to directional and bounded point lights, alpha/transmission
  shadow visibility, rough metallic reflections and dielectric refraction
- Diffuse/GGX importance-sampled multi-bounce paths, Fresnel and Russian roulette
- Imported base-color, metallic/roughness, normal and emissive maps; procedural
  slot textures, vertex colors, wetness, water normals and fog
- Deterministic per-pixel random streams independent of worker scheduling
- Progressive linear-light accumulation invalidated by camera, lighting,
  geometry, instance, material or relevant animation changes
- Optional small depth/normal-guided spatial filter; ACES-fitted display transform
  followed by sRGB encoding. Filtering never feeds back into accumulated samples
- Beauty, depth, normals, direct and indirect debug views. Unsupported motion view
  and GPU debug-layer requests fail explicitly
- Threaded row batches with disjoint output ownership; software surface present
  and deterministic readback before/after HUD

`cpu_frame_ms`, `cpu_threads`, `rays_traced`, `software_ray_tracing`, BVH builds,
unique/submitted triangles and accumulated frames are reported separately from
GPU time. A zero GPU time is **not** a GPU performance measurement.

### Important limits

The environment is a simple hemispherical sky, not the DX12 atmospheric model.
The denoiser is a small spatial filter, not a learned or motion-vector temporal
denoiser. There is no emissive-light importance sampling/MIS, volumetric media,
caustics solver, spectral dispersion, full nested-medium tracking or spectral
transport. Transparent shadow attenuation is an inexpensive straight-line
approximation; refracted camera/continuation rays are traced. Thin/intersecting
glass and very rough transmission remain approximations. High-contrast glossy
paths can need many samples. Dynamic scenes restart accumulation when they change.

CPU acceleration uses portable scalar C++ and median-split BVHs, not Embree/SIMD,
GPU compute, a production SAH builder or a task-persistent thread pool. Detailed
instanced scenes fit better than flattened copies, but CPU memory and frame-time
budgets still matter. Current game distance/LOD/sector rules remain and can omit
far-away reflected geometry; camera-facing culling is disabled for ray backends.

Imported material extensions and asset limitations are listed in
[the complete asset audit](ASSET_AUDIT.md). The existing DX12 path was preserved;
this document makes no claim of new Windows hardware-DXR measurements.

## Higher-resolution surface detail

Original 512px tileable PBR detail is now attached to eligible static surfaces,
with mip filtering and selective hero-material integration. See
[the implementation](SURFACE_DETAIL.md), [actual game placements](HERO_SURFACES.md)
and [runtime before/after evidence](SURFACE_VALIDATION.md).
`FURY_SURFACE_DETAIL=0` disables it; `FURY_SURFACE_DETAIL_RES=128|256|512` controls
map memory without changing physical tile size.

## Measured results

See [the runtime comparison and validation report](CPU_VALIDATION.md) for actual
CPU/OpenGL captures, frame times, test scope and remaining platform checks.

## Reproduce validation

```sh
./scripts/validate-cpu.sh build out/cpu-validation
ctest --test-dir build --output-on-failure

# Runtime previews of every shipped GLB and OBJ, with neutral lighting
python3 scripts/preview-assets.py --help
```

The CPU test suites verify actual pixels, hierarchy/instance behavior, near/far
clipping, alpha visibility, perspective interpolation, glass exit refraction,
history invalidation, resize, deterministic multi-threaded output and both actual
application runtimes. Audio has its own [offline/device validation](CPU_AUDIO.md).
The oblique glass regression was mutation-tested against a deliberately broken
exit-face culling implementation and rejected it.

## Full-world inspection

World art is enabled by default. `FURY_WORLD_ART=0` restores the earlier scene
art while keeping renderer correctness fixes; only `0` and `1` are accepted.
`--world-audit out/world.json` builds the real static world, emits coverage,
geometry/LOD diagnostics and original gameplay-field preservation, then exits.
Fixed `--view` choices include all six districts, street-level views and
`world-overview`; `--help` lists them. The overview uses a deliberately extended
500 m draw/LOD range and distant fog, so it is an inspection view rather than a
normal gameplay-performance preset. See [full-world validation](WORLD_VALIDATION.md).

CPU ray mode now continues glossy dielectric reflections at roughness <= 0.35,
weighted by IOR-derived Fresnel reflectance. Rough diffuse ray-mode surfaces
still use bounded direct/ambient lighting. Path mode continues full sampled
diffuse/specular transport. Mirrors and water therefore need actual geometry
in the submitted world; ray/DXR visibility deliberately retains off-camera
shadow and reflection contributors within the draw range.
