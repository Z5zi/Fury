# Fury

**Fury** is a lightweight, original C++17 game engine with SDL2 window/input and a
lit 3D mesh renderer (OpenGL 3.3 core preferred, CPU software rasterizer fallback).

> Not Unreal. Not Unity. Not a GTA clone. Just Fury.

## Direction: Vaultline

**Vaultline** is the first playable vertical slice on Fury — an *original*
**bank-heist open-world MMO** prototype set in fictional **Harbor Metro**,
featuring **Meridian Mutual** bank, the **Crown & Cutler** jewelry front,
**Ashcourt Market** (ATM heist-lite), and the **Harbor Armored Depot**.

> **Honest scope (v1.2.0):** this is a **playable prototype / vertical slice**, not AAA
> and not GTA parity. Expect colored-box districts, stub AI, localhost net, and a
> Meridian heist you can finish in about **2–5 minutes**. No Rockstar / GTA IP.

This is a direction and a growing slice, not a finished MMO:

| Now (this repo) | Next |
|-----------------|------|
| Harbor Metro + **Ridge Pier** + **Ashcourt Market** + **Armored Depot**; enterable jewelry + ATM alcove + depot cage | Multi-floor interiors / streaming districts |
| Day/night cycle (sun/sky/lamp emissive lerp) | Weather, interior lights zones |
| Wandering civilian NPCs + bank guard (chase when heat high) | Traffic AI, awareness cones |
| Driveable getaway van stub near extraction (`F`/`E` enter/exit) | Full vehicle physics / traffic |
| **Crew stubs** (Rook / Sparrow) follow during heist; loot speed boost; **banter** on phase changes | Full crew AI / role abilities |
| Net stub **crew session roles** (Muscle / Lookout / …) | Replicated crew roster |
| Wanted **heat** meter (rises near guards during breach/loot); **siren** flash when heat high while looting | Stealth scoring, wanted tiers |
| **Mission board** (**M**) + **quest journal** (**J**) — 4 jobs, payouts, completion flags in save | Contract scripting / co-op lobby |
| **Ashcourt fence shop** (**B**) — buy crew perk / heat dampener / loot speed with cash | Full economy / black-market tree |
| **3 save slots** (`[`/`]`) — `vaultline_session_slot{N}.json` autosave | Cloud sync / profile UI |
| Heist: approach → breach → loot → escape → success/fail + audio cue hooks | Full mission scripting / multiplayer heists |
| Audio stub (`null` / optional SDL_mixer) — `heist_start` / `heist_success` | Sample banks, spatial SFX |
| Inventory cash / loot bags, HUD bars (cash/loot/score/**heat**/shop/slots) | Persistent profiles, cloud sync |
| AABB building collision (walk mode); vehicle collision radius | Character controller, cover |
| `NetClient` / `NetServer` **localhost UDP loopback** (pose + heat + phase + **optional cash** → Ghost) | Cross-machine sockets, authority, interest mgmt |
| AO-lite + Reinhard/gamma tonemap, animated water UVs, emissive lamps + **point lights** (nearest 2–3); **directional shadow map** (GL; off on llvmpipe) | Cascaded shadows / LODs |
| **Minimap stub** (top-right; player + objective blips) | Full map / radar icons |
| **Onboarding** — first-run tips + compass breadcrumb (board → target → escape) | Scripted tutorial missions |
| **Presentation** — 1.5s VAULTLINE splash; success/fail banners; **P** FPS toggle | Full UI / menus |

No Rockstar / GTA names, maps, characters, brands, or missions.

### Vaultline controls

| Key | Action |
|-----|--------|
| **WASD** | Move (drive while in van) |
| **Mouse** | Look (click to capture) |
| **Space / Ctrl** | Up / down in fly mode |
| **Shift** | Sprint / van boost |
| **F** | Toggle fly/walk; enter/exit getaway van when near |
| **E** | Breach vault/safe/ATM; reset after success/fail; enter/exit van |
| **M** | Mission board (job list + payout tiers) |
| **J** | Quest journal (missions + completion flags) |
| **B** | Ashcourt fence buy menu (must be near shop to purchase) |
| **1 / 2 / 3 / 4** | Select Meridian / Crown / Ashcourt ATM / Harbor Depot, or buy perks (**1–3**) if **B** open |
| **T** | Cycle heist target when idle |
| **[ / ]** | Previous / next save slot (`vaultline_session_slot{N}.json`) |
| **P** | Toggle FPS overlay + FPS log |
| **Esc** | Release mouse; Esc again quits |

**Heist flow:** open the board (**M**) → pick a job → walk into Meridian Mutual (or
enterable Crown & Cutler / Ashcourt ATM alcove / Harbor Armored Depot) → **E** to breach → loot timer →
follow the compass/minimap to the **green extraction pad** (or drive the getaway van).
Heat rises near the bank guard during breach/loot; max heat fails the job.
High heat while looting flashes **siren** beacons. Rook/Sparrow drop short **banter** lines on phase changes.
Spend cash at the **Ashcourt fence** (**B**) on crew / heat damp / loot speed.
Progress autosaves to the active slot (and on quit).

**HUD:** cash, loot, score, heat, crew, mission tier/board, quest journal, buy menu,
save-slot pips, minimap, onboarding tip bar, crew banter tip, alarm pip, objective compass, success/fail banner, optional FPS.

### Districts map (blurb)

```
                    Ridge Pier (east bridge ~x=70)
                              |
   Ashcourt Market  ← west road ←  Harbor Metro plaza  →  waterfront / pier
   (ATM + fence shop ~x=-90)       (Meridian Mutual @ origin,
                                    Crown & Cutler east,
                                    Armored Depot SE ~58,-48,
                                    extraction pad ~34,30)
```

Stub districts on one continuous ground plane — no streaming. Bridge east to
**Ridge Pier**; road west to **Ashcourt Market**; SE spur to **Harbor Armored Depot**.

## Features

- **C++17** engine library (`fury_engine`) + `fury_demo` + `vaultline` (**v1.2.0**)
- **1.2.0** — crew banter (Rook/Sparrow log+HUD tips on heist phase changes); fourth heist target
  **Harbor Armored Depot** (tier 2, short loot) on the mission board; optional **siren** flashing
  emissive when heat is high during looting; Windows `NOMINMAX` / `(std::min)` safety kept
- **1.1.0** — quest journal (**J**) with persisted mission completion flags; directional shadow map
  on the GL path (auto no-op on soft/llvmpipe / `FURY_SHADOWS=0`); world props polish; Windows
  `NOMINMAX` / `(std::min)` CI fix for MSVC vs `windows.h` macros
- **1.0.0 vertical-slice polish** — onboarding breadcrumbs, balance pass (~2–5 min Meridian),
  title splash + success/fail banners, FPS toggle (**P**), save roundtrip check, clean net quit
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
- **Mission board** — four Harbor Metro jobs with payout tiers (M / 1 / 2 / 3 / 4)
- **Crew stubs** — Rook / Sparrow followers; nearby crew speeds loot; rotating banter; net crew roles
- **Alarm / siren** — flashing emissive beacons when heat ≥ 0.55 during Looting
- **UDP loopback net** — in-process threaded host + client; syncs pose/heat/phase/**cash**
- **Interiors polish** — jewelry enterable props; ATM alcove; armored depot cage; denser bank lobby
- **Economy shop** — Ashcourt fence (**B**); crew / heat damp / loot speed perks for cash
- **Save slots** — 3 local JSON slots; `[`/`]` cycle; autosave active slot
- **Denser district art** — varied facades/heights, night window emissives, gold FX
- **Distance cull** — skip entities beyond ~120 m (+ behind-camera reject)
- **Point lights** — nearest lamps fill dynamic lights; night ambient bumped for readability
- **Minimap stub** — top-right map with player + objective blips
- **Multi-district stub** — Harbor Metro ↔ Ridge Pier (bridge) ↔ Ashcourt Market (west road)
- **Audio stub** — `Audio` interface; null backend always; optional SDL_mixer
- Mesh normals, materials, capsules/boxes; AABB collision; scene solids
- Math: `Vec3`/`Vec4`/`Mat4`, look-at, perspective, transforms; optional **NASM** `dot`
- Heist controller with scoring + inventory; multi-slot session JSON
- Localhost UDP loopback net (session id + synced remote pawn + cash flash)
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
      mission.hpp     # mission board jobs + payout tiers (4 Harbor jobs)
      crew.hpp        # AI crew follow + loot speed boost
      banter.hpp      # Rook/Sparrow rotating phase-change lines
      audio.hpp       # cue hooks (null / optional SDL_mixer)
      collision.hpp   # Aabb + resolve_player_collision
      heist.hpp       # approach → breach → loot → escape → success/fail + score
      inventory.hpp   # cash/loot + SessionSnapshot JSON
      net.hpp         # NetClient / NetServer façades (UDP loopback)
      camera.hpp scene.hpp math.hpp …
    src/              # gl_backend, soft_backend, heist, npc, heat, audio, …
    math/asm/         # optional NASM kernels
  apps/demo/          # simple lit cube smoke demo
  apps/vaultline/     # Harbor + Ridge Pier + Ashcourt + Armored Depot heist slice
```

**Render path:** `Application` uploads meshes once, then each frame sets time +
camera + view/proj + lighting, and draws each visible entity with its `Material`.
OpenGL uses a lit fragment shader (directional + point lights, AO-lite, emissive, tonemap/gamma) and generated
64×64 textures. Water materials scroll UVs over time. HUD overlays use blended
screen-space quads. If GL context creation fails, the window is recreated and the
software rasterizer runs instead.

**Gameplay path:** Vaultline builds Harbor Metro (+ districts) into a `Scene`, drives
`HeistController` + `HeatMeter` + `MissionBoard` + `CrewSystem` + Ashcourt shop perks from
camera position + **E**/`M`/`B`/`[`/`]`, resolves walk-mode collision against solid entity
AABBs, fills nearest lamp point lights, mirrors a UDP-synced remote pawn via `NetClient`
(pose/heat/phase/cash + crew roles), and autosaves the active save-slot JSON on heist
resolve / quit / perk purchase.

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
# Expect timeout exit 124 (still running when killed) — healthy smoke
timeout 3 xvfb-run -a ./build/apps/vaultline/vaultline || test $? -eq 124

# Soft (software rasterizer) smoke — auto-quits after ~2.6s
FURY_SOFT=1 FURY_SMOKE=1 xvfb-run -a ./build/apps/vaultline/vaultline --soft --smoke
```

The engine tries OpenGL first; if context creation or GL loading fails, it
recreates the window and uses the software triangle rasterizer so CI/xvfb still works.
`FURY_SOFT=1` / `--soft` forces the software path; `FURY_SMOKE=1` / `--smoke` auto-quits.

Session files (cwd): `vaultline_session_slot0.json` … `slot2.json` — cash, successes/failures,
score, target index, perk levels, slot id, **mission_complete_0..3** journal flags.
Legacy `vaultline_session.json` migrates into slot 0.

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
(`flags bit0 = in_heist`) plus **optional trailing `float cash`**. Older peers that omit
cash still decode (cash defaults to 0). Synced each frame from the local Operator.

**StateSnapshot** (server→client): `u16 count` + `count` packed states (host +
`Ghost-Loop` bot). The ghost mirrors host heat/phase/**cash** and patrols for MMO plumbing;
Vaultline flashes the Ghost pawn when synced cash increases.

Crew role assigns stay in-process on the embedded host (Muscle / Lookout / …).

Cross-machine sockets / interest management are still next.

## Assembly math

On `x86_64`, CMake enables `ASM_NASM` and `FURY_HAS_ASM=1` when NASM is found.
Kernel: `fury_dot3_asm` — `dot = a·b` for float3.

## License

MIT — see [LICENSE](LICENSE).

## Repository

[https://github.com/Z5zi/Fury](https://github.com/Z5zi/Fury)
