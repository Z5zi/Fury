# Fury

**Fury** is a lightweight, original C++17 game engine with SDL2 window/input and a
lit 3D mesh renderer (OpenGL 3.3 core preferred, CPU software rasterizer fallback).

> Not Unreal. Not Unity. Not a GTA clone. Just Fury.

## Direction: Vaultline

**Vaultline** is the first playable vertical slice on Fury — an *original*
**bank-heist open-world MMO** prototype set in fictional **Harbor Metro**,
featuring **Meridian Mutual** bank, the **Crown & Cutler** jewelry front, and
**Ashcourt Market** (ATM heist-lite).

This is a direction and a growing slice, not a finished MMO:

| Now (this repo) | Next |
|-----------------|------|
| Harbor Metro + **Ridge Pier** + **Ashcourt Market** district stubs | Multi-floor interiors / streaming districts |
| Day/night cycle (sun/sky/lamp emissive lerp) | Weather, interior lights zones |
| Wandering civilian NPCs + bank guard (chase when heat high) | Traffic AI, awareness cones |
| Driveable getaway van stub near extraction (`F`/`E` enter/exit) | Full vehicle physics / traffic |
| Wanted **heat** meter (rises near guards during breach/loot) | Stealth scoring, wanted tiers |
| Crown & Cutler + Ashcourt ATM heist targets (cycle with **T**) | Mission board / multi-target contracts |
| Heist: approach → breach → loot → escape → success/fail + audio cue hooks | Full mission scripting / multiplayer heists |
| Audio stub (`null` / optional SDL_mixer) — `heist_start` / `heist_success` | Sample banks, spatial SFX |
| Inventory cash / loot bags, HUD bars (cash/loot/score/**heat**), session JSON | Persistent profiles, cloud sync |
| AABB building collision (walk mode); vehicle collision radius | Character controller, cover |
| `NetClient` / `NetServer` stub (session id + simulated remote pawn) | Real sockets, replication, authority |
| AO-lite + Reinhard/gamma tonemap, animated water UVs, emissive lamps | Cascaded shadows (when not on llvmpipe), LODs |

No Rockstar / GTA names, maps, characters, brands, or missions.

### Vaultline controls

- **WASD** — move · **Mouse** — look (click to capture)
- **Space / Ctrl** — up / down (fly mode) · **Shift** — sprint (or boost while driving)
- **F** — toggle fly / walk; near getaway van (or while seated) enter/exit vehicle
- **E** — interact (breach vault / safe / ATM / reset after success or fail); also enter/exit van when close
- **T** — cycle heist target (Meridian Mutual → Crown & Cutler → Ashcourt ATM) when idle
- **Esc** — release mouse; Esc again quits

Heist flow: walk into Meridian Mutual (or Crown & Cutler / Ashcourt ATM) → approach
the vault / safe / ATM → press **E** to breach → wait through loot → reach the green
extraction pad (or drive the getaway van). Heat rises if a guard is nearby during
breach/loot; max heat fails the job and shortens escape time. Cash and score persist
in `vaultline_session.json`.

HUD (screen-space colored quads): cash, loot progress, lifetime score, **heat/wanted**.

## Features

- **C++17** engine library (`fury_engine`) + `fury_demo` + `vaultline` (**v0.6.0**)
- **Cross-platform** CMake for **Linux** and **Windows**
- **SDL2** window & input; mouse capture
- **OpenGL 3.3 core** lit mesh renderer (directional + ambient, Blinn specular,
  metallic/roughness/emissive, procedural albedo textures, distance fog,
  single-pass SSAO-lite, Reinhard tonemap + gamma, UV scroll for water)
- **Software** fallback with matching AO-lite / tonemap / emissive / HUD rects
- **Day/night cycle** — sun direction/color, sky clear, fog, lamp emissive
- **NPC agents** — civilians + guard, street waypoints, guard chase on high heat
- **Vehicles stub** — box/van enter/drive/exit near extraction
- **Heat / wanted** — rises near guards in Breach/Looting; decays when hidden/escaped
- **Multi-district stub** — Harbor Metro ↔ Ridge Pier (bridge) ↔ Ashcourt Market (west road)
- **Audio stub** — `Audio` interface; null backend always; optional SDL_mixer
- Mesh normals, materials, capsules/boxes; AABB collision; scene solids
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
      mesh.hpp        # Vertex, Material, capsule/box helpers, TextureSlot
      day_night.hpp   # sun/sky/lamp lerp over time_of_day
      npc.hpp         # wandering AABB agents + waypoint paths + chase
      heat.hpp        # wanted / heat meter
      audio.hpp       # cue hooks (null / optional SDL_mixer)
      collision.hpp   # Aabb + resolve_player_collision
      heist.hpp       # approach → breach → loot → escape → success/fail + score
      inventory.hpp   # cash/loot + SessionSnapshot JSON
      net.hpp         # NetClient / NetServer façades (stub impl)
      camera.hpp scene.hpp math.hpp …
    src/              # gl_backend, soft_backend, heist, npc, heat, audio, …
    math/asm/         # optional NASM kernels
  apps/demo/          # simple lit cube smoke demo
  apps/vaultline/     # Harbor + Ridge Pier + Ashcourt heist slice
```

**Render path:** `Application` uploads meshes once, then each frame sets time +
camera + view/proj + lighting, and draws each visible entity with its `Material`.
OpenGL uses a lit fragment shader (AO-lite, emissive, tonemap/gamma) and generated
64×64 textures. Water materials scroll UVs over time. HUD overlays use blended
screen-space quads. If GL context creation fails, the window is recreated and the
software rasterizer runs instead.

**Gameplay path:** Vaultline builds Harbor Metro (+ districts) into a `Scene`, drives
`HeistController` + `HeatMeter` from camera position + **E**, resolves walk-mode
collision against solid entity AABBs, mirrors a stub remote pawn via `NetClient`, and
autosaves session JSON on heist resolve / quit.

## Dependencies

| Platform | Packages / tools |
|----------|------------------|
| Linux | `cmake`, `g++`, `libsdl2-dev`, `libgl1-mesa-dev`, `nasm`, `pkg-config`; optional `libsdl2-mixer-dev` |
| Windows | CMake, MSVC/Clang, SDL2 (vcpkg or official VC zip), NASM; OpenGL from system; optional SDL2_mixer |

### Debian / Ubuntu

```bash
sudo apt-get install -y cmake g++ libsdl2-dev libgl1-mesa-dev nasm pkg-config xvfb
# optional: sudo apt-get install -y libsdl2-mixer-dev
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
