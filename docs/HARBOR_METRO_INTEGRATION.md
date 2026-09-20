# Harbor Metro → Vaultline integration

Content bridge for **Harbor Metro / HMPD / Meridian Mutual** authored meshes into the Vaultline heist vertical slice. No GTA / Rockstar IP.

## What landed (phases 0–5)

| Phase | Piece | Location |
|-------|--------|----------|
| 0 | Asset registry `name → glb/obj/lod` | `apps/vaultline/harbor_assets.hpp/.cpp` |
| 1 | Meridian Mutual interior kit + vault hero route | `spawn_meridian_mutual()` via `build_meridian_mutual()` |
| 2 | Bank-block street dressing | `spawn_meridian_block()` |
| 3 | HMPD cruiser v12b pursuit visuals | `apps/vaultline/main.cpp` + `PatrolCar` visual hooks |
| 4 | Rear-alley getaway (`hm_civ_van_v3`) | `spawn_meridian_getaway()` @ `(12, 0, -20)` |
| 5 | Traffic civ sedan/hatch/van v3 | traffic setup in `main.cpp` |

## How to run

```bash
cmake -S . -B build -G Ninja
cmake --build build --target vaultline
# from a cwd that can resolve assets (binary stages meshes beside itself):
./build/apps/vaultline/vaultline
```

Assets are copied next to the binary under `assets/meshes/harbor_metro/` on build.

## Expected launch log lines (Test A)

```text
Meridian Mutual loaded
Vault door loaded
HMPD cruiser loaded
Getaway vehicles loaded
Street kit loaded
```

Supporting lines (GLB path):

```text
GLB loaded: harbor_metro/hm_bank_interior_kit_v2.glb (Meridian Mutual kit)
GLB loaded: harbor_metro/hm_bank_vault_door_v2.glb (vault door)
GLB loaded: harbor_metro/hmpd_cruiser_v12b.glb (HMPD cruiser)
GLB loaded: harbor_metro/hm_civ_van_v3.glb (getaway van)
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

- **Test B — Bank walkthrough:** Enter south doorway → lobby (teller / queue / chairs) → security desk (west) → corridor → vault door hero → deposit boxes. No placeholder cube as the vault hero when the GLB is present.
- **Test C — Screenshots:** exterior props + traffic/HMPD; lobby kit dominance; vault door hero; escape with getaway + cruiser.
- **Test E — Perf:** Vehicles merge filtered glTF prims (drops `ground_walk` / `ground_curb` helpers). Bank kit places multi-prim parts (materials preserved). Simple **box colliders** for gameplay only.
- Pursuit / traffic / heist **logic unchanged**; only visuals + escape pad location moved to Meridian rear alley.

## Scale / material gaps to watch

- Civ v3 GLBs ship with large `ground_walk` / `ground_curb` helper meshes — **filtered on load** so sedan/van/hatch sit at ~2×5 m.
- Merged vehicle meshes use the **first non-helper material**; multi-material fidelity is best on bank/prop prim placement, not merged traffic/patrol bodies.
- Authored meshes are grounded at `y = 0` (pursuit/traffic `ground_y` updated from the old 0.85–0.9 box centers).
- Interior kit + individual hero pieces may overlap slightly; prefer kit for density and vault door / teller / security desk for readable route.
- Lighting / NPC / audio still the next bottlenecks after this content bridge (see ChatGPT plan §6).

## Branding

Harbor Metro · HMPD · Meridian Mutual only.
