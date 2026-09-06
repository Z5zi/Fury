# Fury

**Fury** is a lightweight, original C++17 game engine with SDL2 window/input and a
lit 3D mesh renderer (OpenGL 3.3 core preferred, CPU software rasterizer fallback).

> Not Unreal. Not Unity. Not a GTA clone. Just Fury.

## Direction: Vaultline

**Vaultline** is the first playable vertical slice on Fury — an *original*
**bank-heist open-world MMO** prototype set in fictional **Harbor Metro**,
featuring **Meridian Mutual** bank and the **Crown & Cutler** jewelry front stub.

This is a direction and a growing slice, not a finished MMO:

| Now (this repo) | Next |
|-----------------|------|
| Denser Harbor Metro streets + enterable Meridian Mutual + vault room | Larger district / multi-floor interiors |
| Crown & Cutler jewelry heist target stub (toggle with **T**) | Mission board / multi-target contracts |
| Waterfront pier, alley escape van pad, ATMs, teller desks, city blocks | Traffic, civilians, vehicles |
| Heist: approach → breach → loot timer → escape → success/fail + scoring | Full mission scripting / multiplayer heists |
| Inventory cash / loot bags, HUD bars, `vaultline_session.json` save stub | Persistent profiles, cloud sync |
| AABB building collision (walk mode) | Character controller, cover |
| `NetClient` / `NetServer` stub (session id + simulated remote pawn) | Real sockets, replication, authority |
| AO-lite + Reinhard/gamma tonemap, animated water UVs, emissive lamps | Cascaded shadows (when not on llvmpipe), LODs |

No Rockstar / GTA names, maps, characters, brands, or missions.

### Vaultline controls

- **WASD** — move · **Mouse** — look (click to capture)
- **Space / Ctrl** — up / down (fly mode) · **Shift** — sprint
- **F** — toggle fly / walk (walk uses AABB collision)
- **E** — interact (breach vault / safe / reset after success or fail)
- **T** — switch heist target (Meridian Mutual ↔ Crown & Cutler) when idle
- **Esc** — release mouse; Esc again quits

Heist flow: walk into Meridian Mutual (or Crown & Cutler) → approach the gold vault /
display safe → press **E** to breach → wait through loot → reach the green extraction
pad / getaway van. Cash and score persist in `vaultline_session.json`.

HUD (screen-space colored quads): cash bar, loot progress, lifetime score.

## Features

- **C++17** engine library (`fury_engine`) + `fury_demo` + `vaultline`
- **Cross-platform** CMake for **Linux** and **Windows**
- **SDL2** window & input; mouse capture
- **OpenGL 3.3 core** lit mesh renderer (directional + ambient, Blinn specular,
  metallic/roughness/emissive, procedural albedo textures, distance fog,
  single-pass SSAO-lite, Reinhard tonemap + gamma, UV scroll for water)
- **Software** fallback with matching AO-lite / tonemap / emissive / HUD rects
- Mesh normals, materials (`albedo` / `metallic` / `roughness` / `emissive` /
  UV scroll / texture slot)
- AABB collision helpers; scene solid collection
- Math: `Vec3`/`Vec4`/`Mat4`, look-at, perspective, transforms; optional **NASM** `dot`
- Heist controller with scoring + inventory; session JSON save/load stub
- Net stubs with session id
- GitHub Actions CI (`ubuntu-latest`, `windows-latest`)

## Architecture

```
Fury/
  CMakeLists.txt
  README.md
  .github/workflows/ci.yml
  engine/
    include/fury/     # public headers
      application.hpp # main loop, collision integrate, scene draw, time
      renderer.hpp    # Lighting + Material + HUD rect API; GL or software
      mesh.hpp        # Vertex, Material (emissive / UV scroll), TextureSlot
      collision.hpp   # Aabb + resolve_player_collision
      heist.hpp       # approach → breach → loot → escape → success/fail + score
      inventory.hpp   # cash/loot + SessionSnapshot JSON
      net.hpp         # NetClient / NetServer façades (stub impl)
      camera.hpp scene.hpp math.hpp …
    src/              # gl_backend, soft_backend, heist, inventory, …
    math/asm/         # optional NASM kernels
  apps/demo/          # simple lit cube smoke demo
  apps/vaultline/     # Harbor Metro bank-heist slice
```

**Render path:** `Application` uploads meshes once, then each frame sets time +
camera + view/proj + lighting, and draws each visible entity with its `Material`.
OpenGL uses a lit fragment shader (AO-lite, emissive, tonemap/gamma) and generated
64×64 textures. Water materials scroll UVs over time. HUD overlays use blended
screen-space quads. If GL context creation fails, the window is recreated and the
software rasterizer runs instead.

**Gameplay path:** Vaultline builds Harbor Metro into a `Scene`, drives
`HeistController` from camera position + **E**, resolves walk-mode collision
against solid entity AABBs, mirrors a stub remote pawn via `NetClient`, and
autosaves session JSON on heist resolve / quit.

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

Session file (cwd): `vaultline_session.json` — cash, successes/failures, score, target index.

## Networking (stub)

`engine/include/fury/net.hpp` defines `NetClient` and `NetServer`. The current
implementation is an **in-process stub** (`create_stub_client` / `create_stub_server`)
that:

- Allocates a **session id** and world name (`Harbor Metro`)
- Simulates a second pawn (`Ghost-Stub`) on a patrol path for MMO-shaped plumbing
- Does **not** open real sockets yet

Real netcode is explicitly next work.

## Assembly math

On `x86_64`, CMake enables `ASM_NASM` and `FURY_HAS_ASM=1` when NASM is found.
Kernel: `fury_dot3_asm` — `dot = a·b` for float3.

## License

MIT — see [LICENSE](LICENSE).

## Repository

[https://github.com/Z5zi/Fury](https://github.com/Z5zi/Fury)
