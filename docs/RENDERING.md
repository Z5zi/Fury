# Fury rendering

Fury's Windows renderer uses Direct3D 12 and DXR 1.1 hardware ray queries. It is
integrated into both the playable Vaultline prototype and a reproducible coastal
rendering lab. OpenGL 3.3 and the CPU rasterizer remain separate available backends.

The visual aspiration is a high-detail original coastal world. The performance
target is 1440p at 120 rendered fps on the RTX 4090, an 8.33 ms frame budget.
Neither that target nor adding ray tracing establishes GTA 6 graphics parity.

## Build on Windows

Install a Visual Studio C++ toolchain, a Windows SDK, CMake, Git and Python 3.
NASM is optional; without it the engine uses its C++ math kernel.

```powershell
.\scripts\build-windows.ps1
.\scripts\fetch-render-assets.ps1
```

`bootstrap-windows.ps1` verifies these pinned dependencies:

| Dependency | Pinned input |
|---|---|
| SDL2 | 2.30.9 MSVC distribution |
| DXC | 1.9.2607, 2026-07-29 Windows distribution |
| AMD FSR SDK | 2.3.0, commit `60f4ea81909200d8542eca14dccb2628b763a9a3` |
| Intel XeSS SDK | 3.0.2 distribution |

The SDK package version and selected algorithm are different things. The AMD loader
reports the actual provider; on the tested RTX 4090 it selects FSR 3.1.5. The Intel
package contains the XeSS-SR 2.0.2 runtime. Both paths call the vendor's real D3D12
context and dispatch APIs. A missing or incompatible requested runtime is an error,
not a relabeled native fallback. The DLLs are loaded beside the executable.

For an existing dependency directory:

```powershell
.\scripts\build-windows.ps1 -DependencyRoot C:\Fury-deps
```

For a conventional OpenGL/software build, leave `FURY_ENABLE_DX12=OFF`. For a manual
DX12 build, set `FURY_ENABLE_DX12=ON`, `FURY_DXC_ROOT`, `FURY_FSR_ROOT` and
`FURY_XESS_ROOT`; the latter two are optional at build time but required to select
their respective runtime modes. Shader bytecode is compiled with DXC and embedded
in the executable, so DXC is not needed on a player's machine.

## Run the coastal lab

Run from its output directory so the existing procedural/file texture assets resolve:

```powershell
cd .\build\apps\renderlab\Release
.\fury_renderlab.exe --width 2560 --height 1440 --upscaler fsr --quality quality --no-vsync `
  --pier ..\..\..\..\out\assets\modular_wooden_pier\modular_wooden_pier_2k.gltf `
  --trees ..\..\..\..\out\assets\island_tree_01\island_tree_01_2k.gltf
```

Omit `--pier`/`--trees` for the procedural fixture. `--gltf asset.glb --gltf-only`
inspects a static imported asset with automatic camera framing.

| Control | Action |
|---|---|
| Hold right mouse | Look |
| WASD | Fly horizontally/in the viewing direction |
| Q / E | Down / up |
| Shift | Faster movement |
| F1 | Ray-traced / path-traced lighting |
| F2 | Native / AMD FSR / Intel XeSS |
| F3 | Reconstruction quality |
| F4 | Beauty / depth / normals / motion / direct / indirect views |
| Space | Animate water and the moving fixture object |
| F11 | Fullscreen |
| Escape | Exit |

`--help` lists capture and test options. SDK quality modes query the vendor for
their input dimensions; equal mode names do not imply identical internal resolution.
Still native frames accumulate progressively. Moving scenes use a shorter temporal
history with depth/normal checks. Direct lighting is preserved separately from
denoised indirect transport; tone mapping and the HUD follow reconstruction.

## Run Vaultline with DX12

```powershell
$env:FURY_RENDERER = 'dx12'
$env:FURY_UPSCALER = 'fsr' # native, fsr, xess
$env:FURY_TRACE_MODE = 'path' # ray, path
cd .\build\apps\vaultline\Release
.\vaultline.exe
```

The **O / Start settings menu** exposes lighting mode, upscaler and upscaling
quality and saves them in `vaultline_settings.json`. Environment values override
the saved upscaler/lighting selection at startup. Unavailable menu selections retain
the previous working settings. `--soft` explicitly selects the CPU renderer even
when `FURY_RENDERER=dx12` is present.

## Validate

Windows Graphics Tools must be installed for `--debug` validation. The renderer
checks the D3D12 info queue and device state, and a GPU error makes the process fail.

```powershell
.\scripts\validate-rendering.ps1 -DetailedScene
.\scripts\validate-rendering.ps1 -DetailedScene -BenchmarkOnly -WithoutDebugLayer `
  -Width 2560 -Height 1440 -SamplesPerPixel 1 -Frames 900 `
  -OutputDirectory .\out\benchmark-1440p120
```

The validation suite checks native/FSR/XeSS, dynamic object deformation, camera
motion/cuts, resize, live SDK switching, alpha visibility changes, neutral static
motion vectors, nonzero moving vectors, depth/normals, and geometry build counters.
Reports include the executable hash, dimensions, samples/bounces, frame counts,
actual provider, unique/instanced triangles, GPU and complete render-call times,
and median/p95/p99. GPU timings include acceleration work, lighting, denoising,
reconstruction and compositing; render-call timings also include CPU submission
and fence waits. Captures are taken before presenting the final frame and are
excluded from measured timing samples. Run performance cases without other builds
or GPU work. Interpolated frames are not included in these results.

Linux prerequisites are `cmake`, a C++17 compiler, SDL2/OpenGL development packages,
Ninja, and Xvfb. Run `bash scripts/validate-linux.sh` for the legacy backend gates.
Hosted CI compiles DX12 but cannot substitute for testing a real DXR GPU.

## Current scope and limits

- Hardware: requires DXR 1.1 and Shader Model 6.5. The validated hardware is the RTX
  4090; AMD/Intel hardware validation remains separate work.
- Static glTF/GLB: node transforms, indexed/non-indexed triangles, sparse floating
  accessors, base color/normal/metallic-roughness/emissive maps, alpha masks and
  stochastic alpha coverage, transmission/IOR factors, PNG/JPEG/embedded images,
  and a shared UV transform. Required unsupported extensions fail explicitly.
- Skinned/morph animation, multiple UV sets, different per-map UV transforms,
  compressed texture formats, and material-specific advanced lobes are not yet
  supported by the glTF importer. Imported assets therefore remain static.
- Texture sets retain their own resolution and mip chains. sRGB mips are filtered
  in linear light and normal mips are renormalized. The current limits are 64
  material texture sets and a 30-million expanded-vertex geometry pool.
- Shared meshes have one BLAS; instances use a TLAS. Call `Mesh::mark_dirty()` after
  geometry edits and before drawing that mesh in a frame. Object IDs carry rigid
  and deforming motion history. There is no world/texture streaming system yet.
- Ray mode uses direct visibility/reflections and an ambient approximation. Path
  mode adds sampled multi-bounce transport. Water uses procedural normal waves,
  reflection/refraction and absorption; it is not a fluid simulation or a caustics
  solver. The atmosphere is a compact single-scattering approximation.
- The current denoiser is an engine implementation, not AMD Ray Regeneration or
  NVIDIA NRD. Residual noise, reflection softness and disocclusion artifacts still
  need visual improvement. No skin/hair-specific renderer is certified.
- The integrated FSR/XeSS feature is **super resolution**. Frame generation and
  vendor latency-control integrations are not implemented in this development stage.
- DX12 is Windows-specific. Linux retains OpenGL/software rendering; Vulkan RT
  and Vulkan temporal reconstruction remain unfinished.

The cabin/boat and several props are still procedural fixture art. Production
characters, animation, interiors, terrain, streamed world content and a controlled
external visual comparison are necessary before claiming the original visual goal.
