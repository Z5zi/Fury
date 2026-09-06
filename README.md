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
| **Crew stubs** (up to 2 AI) follow during heist; loot speed boost nearby | Full crew AI / role abilities |
| Net stub **crew session roles** (Muscle / Lookout / …) | Replicated crew roster |
| Wanted **heat** meter (rises near guards during breach/loot) | Stealth scoring, wanted tiers |
| **Mission board** (**M**) — Meridian / Crown / Ashcourt ATM + payout tiers; **1/2/3** select | Contract scripting / co-op lobby |
| Heist: approach → breach → loot → escape → success/fail + audio cue hooks | Full mission scripting / multiplayer heists |
| Audio stub (`null` / optional SDL_mixer) — `heist_start` / `heist_success` | Sample banks, spatial SFX |
| Inventory cash / loot bags, HUD bars (cash/loot/score/**heat**), session JSON | Persistent profiles, cloud sync |
| AABB building collision (walk mode); vehicle collision radius | Character controller, cover |
| `NetClient` / `NetServer` **localhost UDP loopback** (transform + heat + heist phase → remote pawn) | Cross-machine sockets, authority, interest mgmt |
| AO-lite + Reinhard/gamma tonemap, animated water UVs, emissive lamps + **point lights** (nearest 2–3) | Cascaded shadows (when not on llvmpipe), LODs |
| **Minimap stub** (top-right; player + objective blips) | Full map / radar icons |

No Rockstar / GTA names, maps, characters, brands, or missions.

### Vaultline controls

- **WASD** — move · **Mouse** — look (click to capture)
- **Space / Ctrl** — up / down (fly mode) · **Shift** — sprint (or boost while driving)
- **F** — toggle fly / walk; near getaway van (or while seated) enter/exit vehicle
- **E** — interact (breach vault / safe / ATM / reset after success or fail); also enter/exit van when close
- **M** — open/close mission board (HUD job list + payout tiers)
- **1 / 2 / 3** — select Meridian vault / Crown jewelry / Ashcourt ATM (when idle)
- **T** — cycle heist target (same three jobs) when idle
- **Esc** — release mouse; Esc again quits

Heist flow: walk into Meridian Mutual (or Crown & Cutler / Ashcourt ATM) → approach
the vault / safe / ATM → press **E** to breach → wait through loot → reach the green
extraction pad (or drive the getaway van). Heat rises if a guard is nearby during
breach/loot; max heat fails the job and shortens escape time. Cash and score persist
in `vaultline_session.json`.

HUD (screen-space colored quads): cash, loot progress, lifetime score, **heat/wanted**, crew nearby, mission tier / board, **minimap** (player + objective).

## Features

- **C++17** engine library (`fury_engine`) + `fury_demo` + `vaultline` (**v0.8.0**)
- **Cross-platform** CMake for **Linux** and **Windows**
- **SDL2** window & input; mouse capture
- **OpenGL 3.3 core** lit mesh renderer (directional + ambient + **point lights**, Blinn specular,
  metallic/roughness/emissive, procedural albedo textures, distance fog,
  single-pass SSAO-lite, Reinhard tonemap + gamma, UV scroll for water)
- **Software** fallback with matching point lights / AO-lite / tonemap / emissive / HUD rects
- **Day/night cycle** — sun direction/color, sky clear, fog, lamp emissive
- **NPC agents** — civilians + guard, street waypoints, guard chase on high heat
- **Vehicles stub** — box/van enter/drive/exit near extraction
- **Heat / wanted** — rises near guards in Breach/Looting; decays when hidden/escaped
- **Mission board** — three Harbor Metro jobs with payout tiers (M / 1 / 2 / 3)
- **Crew stubs** — up to 2 AI followers; nearby crew speeds loot; net crew roles
- **UDP loopback net** — in-process threaded host + client; syncs pose/heat/phase
- **Denser district art** — varied facades/heights, night window emissives, gold FX
- **Distance cull** — skip entities beyond ~120 m (+ behind-camera reject)
- **Point lights** — nearest lamps fill dynamic lights; night ambient bumped for readability
- **Minimap stub** — top-right map with player + objective blips
- **Multi-district stub** — Harbor Metro ↔ Ridge Pier (bridge) ↔ Ashcourt Market (west road)
- **Audio stub** — `Audio` interface; null backend always; optional SDL_mixer
- Mesh normals, materials, capsules/boxes; AABB collision; scene solids
- Math: `Vec3`/`Vec4`/`Mat4`, look-at, perspective, transforms; optional **NASM** `dot`
- Heist controller with scoring + inventory; session JSON save/load stub
- Localhost UDP loopback net (session id + synced remote pawn)
- Distance / cheap frustum cull (~120 m)
- CPU particle burst on heist success; night window strips; richer vault gold
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
      mission.hpp     # mission board jobs + payout tiers
      crew.hpp        # AI crew follow + loot speed boost
      audio.hpp       # cue hooks (null / optional SDL_mixer)
      collision.hpp   # Aabb + resolve_player_collision
      heist.hpp       # approach → breach → loot → escape → success/fail + score
      inventory.hpp   # cash/loot + SessionSnapshot JSON
      net.hpp         # NetClient / NetServer façades (UDP loopback)
      camera.hpp scene.hpp math.hpp …
    src/              # gl_backend, soft_backend, heist, npc, heat, audio, …
    math/asm/         # optional NASM kernels
  apps/demo/          # simple lit cube smoke demo
  apps/vaultline/     # Harbor + Ridge Pier + Ashcourt heist slice
```

**Render path:** `Application` uploads meshes once, then each frame sets time +
camera + view/proj + lighting, and draws each visible entity with its `Material`.
OpenGL uses a lit fragment shader (directional + point lights, AO-lite, emissive, tonemap/gamma) and generated
64×64 textures. Water materials scroll UVs over time. HUD overlays use blended
screen-space quads. If GL context creation fails, the window is recreated and the
software rasterizer runs instead.

**Gameplay path:** Vaultline builds Harbor Metro (+ districts) into a `Scene`, drives
`HeistController` + `HeatMeter` + `MissionBoard` + `CrewSystem` from camera position + **E**/`M`,
resolves walk-mode collision against solid entity AABBs, fills nearest lamp point lights,
mirrors a UDP-synced remote pawn via `NetClient` (pose/heat/phase + crew roles), and autosaves session JSON on
heist resolve / quit.

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

## Networking (localhost UDP loopback)

`engine/include/fury/net.hpp` defines `NetClient` / `NetServer`. Vaultline uses
`create_loopback_client()` which starts an **in-process threaded UDP host** on
`127.0.0.1` and a client socket that talks to it (same process = host+client).
You can also `create_loopback_server()` + `start` / `start_threaded` and connect a
client separately.

### Protocol (v1, little-endian)

| Field | Size | Notes |
|-------|------|-------|
| magic | u32 | `0x564C544C` (`VLTL`) |
| version | u16 | `1` |
| type | u16 | `Hello=1`, `Welcome=2`, `PlayerState=3`, `StateSnapshot=4` |
| payload_bytes | u32 | size of following payload |

**Hello** (client→server): `u32` client protocol version.

**Welcome** (server→client): `u64 session_id`, `u32 local_player_id`, `u32 max_players`.

**PlayerState** (client→server): packed `id, px,py,pz, yaw, heat, heist_phase, flags`
(`flags bit0 = in_heist`). Synced each frame from the local Operator.

**StateSnapshot** (server→client): `u16 count` + `count` packed states (host +
`Ghost-Loop` bot). The ghost mirrors host heat/phase and patrols for MMO plumbing.

Crew role assigns stay in-process on the embedded host (Muscle / Lookout / …).

Cross-machine sockets / interest management are still next.

## Assembly math

On `x86_64`, CMake enables `ASM_NASM` and `FURY_HAS_ASM=1` when NASM is found.
Kernel: `fury_dot3_asm` — `dot = a·b` for float3.

## License

MIT — see [LICENSE](LICENSE).

## Repository

[https://github.com/Z5zi/Fury](https://github.com/Z5zi/Fury)
