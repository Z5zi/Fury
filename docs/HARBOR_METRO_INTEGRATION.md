# Harbor Metro → Vaultline integration

Content bridge for **Harbor Metro / HMPD / Meridian Mutual** authored meshes into the Vaultline heist vertical slice. No GTA / Rockstar IP.

## What landed (phases 0–5 + AAA polish)

| Phase | Piece | Location |
|-------|--------|----------|
| 0 | Asset registry `name → glb/obj/lod` | `apps/vaultline/harbor_assets.hpp/.cpp` |
| 1 | Meridian Mutual modular heroes + KIT_* densifiers | `spawn_meridian_mutual()` via `build_meridian_mutual()` |
| 2 | Bank-block street dressing | `spawn_meridian_block()` |
| 3 | HMPD cruiser v12b pursuit visuals (material groups) | `apps/vaultline/main.cpp` + `PatrolCar` visual hooks |
| 4 | Rear-alley getaway (`hm_civ_van_v3`, material groups) | `spawn_meridian_getaway()` @ `(12, 0, -20)` |
| 5 | Traffic civ sedan/hatch/van v3 (first-material merge) | traffic setup in `main.cpp` |
| polish | Heist route markers, mission lighting, NPC anchors | `harbor_assets.cpp`, `interior.hpp`, `main.cpp` |

## Interior source of truth (kit vs modular)

**Choice: modular heroes + KIT_* densifiers (not both stacked).**

The full `hm_bank_interior_kit_v2` embeds teller / security desk / vault door / deposit / chairs / cameras as `TC_` / `SD_` / `VD_` / `DB_` / … prefixes **and** environment densifiers as `KIT_*`. Spawning the whole kit on top of modular heroes double-drew the route.

- **Gameplay route owners:** modular hero GLBs (`bank_teller_counter`, `bank_security_desk`, `bank_vault_door`, queue, chairs, cameras, panels).
- **Atmosphere only:** kit prims whose names start with `KIT_` (floor/walls/lights/brochures/vents/scuffs/emergency/logo) plus `bank_trim_kit` and small procedural paper/monitor/cart stubs.
- Log line: `Interior source of truth: modular heroes + KIT_* densifiers (no stacked kit heroes)`

## Playable heist route

Floor pads + signage (tag `route`) mark:

```text
street → entrance → lobby → security → vault corridor → vault
→ escape side → escape alley → getaway → HMPD cue
```

Aligned with heist objectives: `vault_position ≈ (0,0,-15.2)`, `escape_position = (12,0,-20)` (Meridian rear alley). Restricted corridor stripe + entrance Meridian Mutual plaque for readability. No GTA IP.

## Mission lighting

- Interior catalog splits Meridian into **vault** (dramatic gold/cool), **security** (cooler cyan), **lobby** (warm brass) zones — most-specific-first for `zone_at`.
- Emissive ceiling lamps tagged `lamp` in lobby / security / vault / escape alley (picked up by night point-light path).
- Alarm: `alarm_lamp` strobe + red/blue emergency point light while looting/alarm active.

## Vehicle materials

| Role | Load path | Notes |
|------|-----------|--------|
| Getaway van / HMPD cruiser | `load_harbor_material_groups` | One entity per material — paint, glass, trim preserved |
| Traffic civs | `load_harbor_mesh` (first-material merge) | Density OK |

## NPC anchors (#6 light)

Teller (`NpcTeller`), desk guard (`NpcDeskGuard`), lobby customer (`NpcBankCust`), alley HMPD (`NpcAlleyHmpd`) — existing humanoid NPC path.

## How to run

```bash
cmake -S . -B build -G Ninja
cmake --build build --target vaultline
# from a cwd that can resolve assets (binary stages meshes beside itself):
./build/apps/vaultline/vaultline
```

Assets are copied next to the binary under `assets/meshes/harbor_metro/` on build.

## Expected launch log lines (Test A + polish)

```text
Interior source of truth: modular heroes + KIT_* densifiers (no stacked kit heroes)
Meridian Mutual loaded
Vault door loaded
Meridian mission lighting loaded
Meridian heist route markers loaded
HMPD cruiser loaded
Getaway vehicles loaded
Street kit loaded
```

Supporting lines (GLB path):

```text
GLB prims loaded: harbor_metro/hm_bank_interior_kit_v2.glb (Meridian Mutual KIT densifiers, N parts)
GLB prims loaded: harbor_metro/hm_bank_vault_door_v2.glb (vault door, N parts)
GLB material groups: harbor_metro/hmpd_cruiser_v12b.glb (HMPD cruiser, N materials)
GLB material groups: harbor_metro/hm_civ_van_v3.glb (getaway van, N materials)
GLB loaded: harbor_metro/hm_street_props_kit_v2.glb (street kit)
```

## Fallback behaviour (Test D)

Never crash from missing art. Pattern:

```text
WARNING missing vault door …
Using fallback
```

(or `WARNING missing <label> glb … — trying OBJ / fallback`). Game continues with procedural boxes via the same philosophy as `mesh_obj_or()`.

## Acceptance notes

- **Test B — Bank walkthrough:** Enter south doorway → lobby (teller / queue / chairs) → security desk (west) → corridor → vault door hero → deposit boxes. Route pads readable; no double-stacked kit+hero teller/vault.
- **Test C — Screenshots:** exterior props + traffic/HMPD; lobby kit densifiers + modular heroes; vault door hero with dramatic lighting; escape with getaway + cruiser multi-material.
- **Test E — Perf:** Traffic still first-material merge. Hero vehicles use material groups (tens of parts, not hundreds of prims). Bank modular + KIT densifiers. Simple **box colliders** for gameplay only.
- Pursuit / traffic / heist **logic unchanged**; visuals, lighting, route dressing, escape pad location only.

## Scale / material gaps to watch

- Civ v3 GLBs ship with large `ground_walk` / `ground_curb` helper meshes — **filtered on load**.
- Authored meshes are grounded at `y = 0` (pursuit/traffic `ground_y` updated from the old 0.85–0.9 box centers).
- Audio / cinematic cameras remain future polish (ChatGPT judgment §7–8).

## Branding

Harbor Metro · HMPD · Meridian Mutual only.
