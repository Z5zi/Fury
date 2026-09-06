# Fury

**Fury** is a lightweight, original C++17 game engine with SDL2 window/input and a
3D mesh renderer (OpenGL 3.3 core preferred, CPU software rasterizer fallback).

> Not Unreal. Not Unity. Not a GTA clone. Just Fury.

## Direction: Vaultline

**Vaultline** is the first playable vertical slice on Fury — an *original*
**bank-heist open-world MMO** prototype set in fictional **Harbor Metro**.

This is a direction and a small slice, not a finished MMO:

| Now (this repo) | Next |
|-----------------|------|
| 3D walk/fly scene with streets + **Meridian Mutual** bank | Larger district / interiors |
| Heist state machine: approach vault → timed loot → escape pad | Full mission scripting |
| `NetClient` / `NetServer` stub (in-process, simulated remote pawn) | Real sockets, replication, authority |
| FPS + position + heist/session logs | HUD, inventory, matchmaking |

No Rockstar / GTA names, maps, characters, or missions.

### Vaultline controls

- **WASD** — move · **Mouse** — look (click to capture)
- **Space / Ctrl** — up / down (fly mode) · **Shift** — sprint
- **F** — toggle fly / walk · **E** — interact (start vault crack / reset after end)
- **Esc** — release mouse; Esc again quits

Heist flow: walk to the golden vault box → press **E** → wait through loot timer →
reach the green extraction pad.

## Features

- **C++17** engine library (`fury_engine`) + `fury_demo` + `vaultline`
- **Cross-platform** CMake for **Linux** and **Windows**
- **SDL2** window & input; mouse capture
- **OpenGL 3.3 core** mesh renderer (SDL_GL + minimal loader) with **software** fallback
- Math: `Vec3`/`Vec4`/`Mat4`, look-at, perspective, transforms; optional **NASM** `dot`
- Mesh / scene entities, fly-cam, heist controller, net stubs
- GitHub Actions CI (`ubuntu-latest`, `windows-latest`)

## Layout

```
Fury/
  CMakeLists.txt
  README.md
  .github/workflows/ci.yml
  engine/
    include/fury/     # public headers (math, mesh, camera, scene, renderer, heist, net…)
    src/              # engine sources (+ gl_backend, soft_backend)
    math/asm/         # optional NASM kernels
  apps/demo/          # simple 3D cube smoke demo
  apps/vaultline/     # Harbor Metro bank-heist slice
```

## Dependencies

| Platform | Packages / tools |
|----------|------------------|
| Linux | `cmake`, `g++`, `libsdl2-dev`, `libgl1-mesa-dev`, `nasm`, `pkg-config` |
| Windows | CMake, MSVC/Clang, SDL2 (vcpkg or official VC zip), NASM; OpenGL from system |

### Debian / Ubuntu

```bash
sudo apt-get install -y cmake g++ libsdl2-dev libgl1-mesa-dev nasm pkg-config xvfb
```

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Binaries:

```text
build/apps/demo/fury_demo
build/apps/vaultline/vaultline
```

### Windows (SDL2 VC zip / vcpkg)

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSDL2_DIR=C:/SDL2/cmake
cmake --build build --config Release
```

## Run

```bash
./build/apps/vaultline/vaultline
# or
./build/apps/demo/fury_demo
```

Headless / CI smoke:

```bash
xvfb-run -a ./build/apps/vaultline/vaultline
timeout 3 xvfb-run -a ./build/apps/vaultline/vaultline || test $? -eq 124
```

The engine tries OpenGL first; if context creation or GL loading fails, it
recreates the window and uses the software triangle rasterizer so CI/xvfb still works.

## Networking (stub)

`engine/include/fury/net.hpp` defines `NetClient` and `NetServer`. The current
implementation is an **in-process stub** (`create_stub_client` / `create_stub_server`)
that:

- Allocates a session id and world name (`Harbor Metro`)
- Simulates a second pawn (`Ghost-Stub`) for MMO-shaped plumbing
- Does **not** open real sockets yet

Real netcode is explicitly next work.

## Assembly math

On `x86_64`, CMake enables `ASM_NASM` and `FURY_HAS_ASM=1` when NASM is found.
Kernel: `fury_dot3_asm` — `dot = a·b` for float3.

## License

MIT — see [LICENSE](LICENSE).

## Repository

[https://github.com/Z5zi/Fury](https://github.com/Z5zi/Fury)
