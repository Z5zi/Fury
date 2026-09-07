# Fury rendering development

The target is an original, high-detail coastal world with physically based lighting,
water, foliage, characters and materials, hardware ray tracing, path tracing, AMD
FSR and Intel XeSS. The supplied visual reference is an art-quality target, not a
benchmark result or an asset license. No claim of GTA 6 parity is established.

The user's performance target is **2560 x 1440 at 120 rendered frames per second**
on the RTX 4090: an 8.33 ms frame budget. Native/FSR/XeSS output must be identified;
interpolated frames do not count toward that baseline. Report median, p95 and p99,
not only average FPS, and distinguish GPU time from complete rendered-frame time.

## Baseline (2026-09-07)

Fury 5.5.0 builds and completes its Windows Vaultline smoke run on an RTX 4090.
The renderer is OpenGL 3.3 plus a CPU rasterizer. Shadows, water reflections,
ambient occlusion and bloom have explicitly simplified implementations. There is
no temporal reconstruction, modern graphics API backend or hardware ray tracing.

## Delivery gates

1. **DX12 rendering:** real GPU ray queries, geometry/material submission from
   Fury, bounded resource ownership, resize handling and deterministic captures.
2. **Lighting:** GGX materials, traced visibility and reflections, multi-bounce
   transport, temporal accumulation/denoising and a controlled coastal scene.
3. **Temporal upscaling:** official FSR and XeSS SDK context creation and dispatch;
   HDR color, non-jittered motion, depth, jitter and history reset; UI after
   reconstruction; native fallback explicitly identified. SDK version and actual
   provider must be reported. Frame generation is a separate capability.
4. **Regression evidence:** math/unit checks, original game smoke, native/FSR/XeSS
   captures, motion/camera-cut/resize checks, GPU validation, reproducible builds,
   and frame-time median/p95/p99 with scene and settings recorded.
5. **Visual production:** high-detail licensed assets, calibrated materials,
   animation/skin/hair, foliage, atmosphere, streamed geometry/textures, water
   transmission/caustics and art-directed coastal content.
6. **Production certification:** Windows and Linux backend coverage, AMD/Intel/
   NVIDIA hardware matrix, memory budgets, long-run stability, packaged releases
   and independently reviewed visual/performance comparisons.

Stages are accepted by running output, not by configuration flags or SDK headers.
The existing OpenGL/software routes remain available while the DX12 route matures.
The first DX12 implementation is Windows-specific; Linux parity needs a Vulkan
backend and its own validation. Do not label software traversal as hardware DXR,
upscaling as frame generation, or a procedural test scene as finished game art.

## SDK authority

- AMD FSR SDK 2.3.0: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/tree/v2.3.0
- Intel XeSS SDK 3.0.2: https://github.com/intel/xess/releases/tag/v3.0.2
- DirectX Shader Compiler: https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.9.2607

SDKs are downloaded separately and used under their respective licenses. Their
binary libraries and authentication credentials do not belong in this repository.
