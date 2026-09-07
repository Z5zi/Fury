# Vaultline changelog

Player-facing notes for the Fury **Vaultline** prototype. Honest scope: playable vertical slice, **not** AAA / GTA graphics. Original setting only — no Rockstar / GTA IP.

## 3.3.0 — vehicles polish + radio stub (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **Getaway van mesh** — cab + cargo bed + windshield + headlights (replaces single box)
- **Headlights** — emissive at night while driving
- **Stealable civilian sedan** near Ashcourt Market — press **F** (or **E**) when close; second driveable type
- **Radio stub** — while seated, **C** cycles **Harbor Wave FM** / **Ashcourt Night** / **Pierline Pulse** (log + HUD pip + optional beep)
- Version **3.3.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

## 3.2.0 — NPC names + Q dialogue (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **NPC display names** — civilians (Mira Vale / Jon Keel / Tessa Quill / Nell Ash), guard (**Sgt. Hale**), Ashcourt fence broker (**Cass Vesper**), crew (**Rook** / **Sparrow**)
- **Nameplate stub** — when looking near a named NPC, a short HUD bar + **Q** hint appears (approach also logs `[NPC] … press Q to talk`)
- **Dialogue** — press **Q** near a focused named NPC for a **1–3 line** bark panel; unique line pools for **fence / guard / crew** (civilians share a civilian pool)
- Version **3.2.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

## 3.1.0 — water polish + shadow cascades stub (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **Water** — improved wave normal scroll, simple **foam line** near shore (water-plane UV edges), better Schlick-ish fresnel; keeps soft/llvmpipe safe (CPU path matches tint/foam/fresnel without shadow maps)
- **Shadow cascades stub** — **2-cascade** directional shadows on **high** quality only; **single** map on med/low; **disabled** on soft/llvmpipe / `FURY_SHADOWS=0`
- Version **3.1.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

## 3.0.0 — major prototype milestone (2026-09-07)

Honest: **major prototype milestone**, still **not** AAA / GTA. Ships the full **2.x** slice as a documented 3.0 baseline — denser world, co-op lobby sync, photo/replay stubs, skills/dailies, LOD/occlusion-lite — plus README architecture / districts / net / complete controls and H-help covering every hotkey through 2.9.

### For players
- **Docs pass** — README architecture (mermaid), complete controls table, districts list, net modes; CHANGELOG 2.x→3.0 tour
- **H help** — overlay + log legend lists all bindings through 2.9 (move, heist, panels, F6–F10, lobby/chat/ready, Esc)
- Version **3.0.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke (embedded net)

### From 2.0 → 3.0 (feature tour)

Everything that landed across the 2.x line, now treated as the 3.0 content baseline:

| Area | What you get |
|------|----------------|
| **World** | Harbor Metro, Ridge Pier, Ashcourt Market, Harbor Armored Depot, Harbor loft safehouse, **North Quay** (warehouses / cranes / containers) |
| **Density / render** | Mid-block props, parked cars, neon, rooftop AC; **quality** low/med/high (`FURY_QUALITY` / **F6**); **LOD stub** + **occlusion-lite** + material draw-sort |
| **Characters** | Low-poly **humanoid** NPC/crew/player meshes; procedural limb swing; **V** first/third person |
| **Traffic / AI** | Civilian traffic waypoint loops (stop/slow); patrol cars on heat; crew follow + banter |
| **Jobs** | Mission board (**M**) + journal (**J**); Meridian / Crown / ATM / Depot / **Night Vault** / **North Quay Yard** |
| **Heist loop** | Breach → loot → extract; heat, sirens, van, loft cool-off; interior light zones + door Enter/snap |
| **Economy / meta** | Loot chips, inventory (**I**), fence (**B**/**S**), factions (**U**), **skills** (**N**) + XP, **daily contracts** |
| **Presentation** | Splash, intro fly-over, banners, **H** help, **F9** photo mode, **F10** replay scrub, **F8** mute, optional mixer beeps |
| **Net** | Embedded / host / join UDP; pose/heat/phase/**mission**/loot/cash/ready; chat (**Enter**/**Y**); ready (**K**); **lobby** (**L** / auto; host Enter starts); joiner mirrors host heist |
| **Tech** | GL + software fallback; shadows/reflect/bloom stubs; materials; Windows `NOMINMAX`; Release + xvfb smoke |

## 2.9.0 — co-op heist sync + lobby (2026-09-07)

### For players
- **Co-op heist sync** — UDP `PlayerState` now carries **mission index** + **loot progress** (with phase/ready/cash); **join** clients **mirror the host mission** and heist phase/loot (local heist sim skipped while connected)
- **Lobby UI** — pre-heist lobby panel (**L**, or **auto** when local + remotes + crew are ready); shows connected remotes + selected mission; **host Enter** starts (commits mission, clears ready)
- Version **2.9.0**; Windows `NOMINMAX` kept; Release + xvfb 124 smoke (embedded net)

## 2.8.0 — LOD stub + occlusion-lite (2026-09-07)

### For players
- **LOD stub** — street clutter tagged `detail` (mid-block crates/trash/hydrants, neon, rooftop AC, parked-car cabins); beyond mid range (~half cull) detail either **skips** or uses a shared **box proxy** (`lod_mesh`)
- **Occlusion-lite** — draw skips when an entity's world AABB is fully **behind the camera plane** (tighter than the old point test); when **deep indoors** (interior core, not near a door) outdoor props outside the zone volume are hidden
- **Batching note** — draw list sorted by texture/material to reduce binds; README notes future **GPU instancing**
- Version **2.8.0**; Windows `NOMINMAX` kept; Release + xvfb 124 smoke

## 2.7.0 — photo mode + replay stub (2026-09-07)

### For players
- **Photo mode** (**F9**) — freezes gameplay sim, free-fly camera (WASD move + mouse look, Space/Ctrl up/down), hides HUD; **Esc** (or F9) exits and restores pose
- **Replay stub** — ring buffer of the last ~8 s of player transform; **F10** scrub playback (A/D or arrows), rewind camera along path + cyan ghost trail markers; **Esc**/F10 exits
- Version **2.7.0**; Windows `NOMINMAX` kept; Release + xvfb 124 smoke

## 2.6.0 — skill tree stub + daily contracts (2026-09-07)

### For players
- **Skill tree stub** — earn **XP** on successful heists; spend on **Silent Entry** / **Fast Hands** / **Cool Under Heat** (3 nodes, 1 rank each); panel **N** (1/2/3 unlock, 100 XP)
  - Silent Entry: shorter breach; Fast Hands: faster loot; Cool Under Heat: slower heat rise
- **Daily contracts** — one rotating daily (hash of local date) bonus objective (e.g. finish ATM without peak heat > 0.5); HUD pip; cash bonus on claim
- XP / skill ranks / daily claim day persist in save slots; version **2.6.0**; Windows `NOMINMAX` kept; Release + xvfb 124 smoke

## 2.5.0 — interior lighting zones + door triggers (2026-09-07)

### For players
- **Interior lighting zones** — standing inside **bank / jewelry / loft / depot** volumes boosts ambient, enables extra warm point fills, and dims exterior sun contribution
- **Door triggers** — labeled doorway volumes show an **Enter** tip; **E** optionally snaps to an interior spawn (open walk-through doorways still work)
- Scene tags: loft walls use `loft`; depot shell tagged `depot`; ceiling lamps in Meridian + Depot
- Version **2.5.0**; Windows `NOMINMAX` kept; Release + xvfb 124 smoke

## 2.4.0 — character meshes + walk stub (2026-09-07)

### For players
- **Low-poly humanoids** — NPC civilians / guard and crew (Rook / Sparrow) use box torso/head/limb meshes from mesh helpers (replacing capsules)
- **Procedural walk** — limb swing + light bob from movement speed (`sin` phase); no skeletal file format
- **V** toggles **first / third person**; optional player body mesh when third-person and not fly-cam (hidden in fly / van / first-person)
- Version **2.4.0**; Windows `NOMINMAX` kept; Release + xvfb 124 smoke

## 2.3.0 — North Quay + traffic AI (2026-09-07)

### For players
- **North Quay** industrial stub north of Harbor — warehouses, box-mesh cranes, container stacks, water tongue; road/bridge link from the loft waterfront
- **Traffic AI** — 6 civilian cars looping street waypoints across Harbor / Ridge bridge / Ashcourt / North Quay approach (not pursuit); **stop/slow** near the player
- **North Quay Container Yard** optional tier-1 heist-lite (**6** on mission board) — sealed container target; finale still unlocks from the four core Harbor jobs
- Version **2.3.0**; Windows `NOMINMAX` kept; Release + xvfb 124 smoke

## 2.2.0 — audio ambience + mute (2026-09-07)

### For players
- **Optional SDL2_mixer** — when present at build time, tiny **procedural PCM beeps** (in-memory WAV, no OGG bank) for footstep / breach / impact / success / siren; otherwise **null** audio stays silent
- Silent / missing-mixer path **logs each cue name once** (no footstep spam)
- **Ambience volume hooks** for day / night / rain (applied even when silent; scales mixer master when available)
- **F8** toggles master **mute** (HUD tip pip)
- CMake `find_package(SDL2_mixer)` remains **optional** — CI builds without mixer
- Version **2.2.0**; Windows `NOMINMAX` kept; Release + xvfb 124 smoke

## 2.1.0 — denser world + quality toggles (2026-09-07)

### For players
- **Denser streets** in Harbor Metro, Ridge Pier, and Ashcourt Market — mid-block props (crates / trash / hydrants), **static parked cars**, neon signs, rooftop AC boxes filling empty stretches
- **Quality presets** — `FURY_QUALITY=low|med|high` (default med); in-game **F6** cycles (save slots keep `[`/`]`)
  - **low**: cull ~55 m, shadow 512, bloom/reflect off, fog 28–85
  - **med**: cull ~90 m, shadow 1024, bloom/reflect on, fog 40–150
  - **high**: cull ~140 m, shadow 2048, stronger bloom/reflect, fog 55–220
- Draw-distance **fog** tuned per quality (still densifies in rain)
- Version **2.1.0**; Windows `NOMINMAX` kept; Release + xvfb 124 smoke

## 2.0.0 — content-complete prototype polish (2026-09-07)

Best playable prototype so far. Still colored-box districts, stub AI, and localhost net.

### For players
- **H** opens a full **controls help** overlay (Esc / H to close)
- HUD layout pass — mission board, fence, journal, inventory, reputation, chat, and tips overlap less
- **Chat vs inventory focus** — chat closes other panels; panel hotkeys ignore chat/help; inventory and chat no longer fight for keys
- Smoke / CI path **skips splash cutscene and chat** for a clean auto-quit
- Tighter world **cull (~90 m)**; distant NPCs skip sim updates when far
- Optional `FURY_PERF=1` prints a once-per-second perf line (fps, cull, NPC update counts)

### From 1.0 → 2.0 (feature tour)
Everything that landed across the 1.x slice, now treated as the 2.0 content baseline:

| Area | What you get |
|------|----------------|
| **World** | Harbor Metro, Ridge Pier, Ashcourt Market, Harbor Armored Depot, Harbor loft safehouse |
| **Jobs** | Mission board (**M**) + journal (**J**); Meridian / Crown / ATM / Depot + **Meridian Night Vault** finale |
| **Heist loop** | Breach → loot → extract; heat, sirens, patrol cars, getaway van, crew stubs + banter |
| **Economy** | Loot tables (cash + BearerBond / Sapphire / LedgerDrive); inventory (**I**); Ashcourt fence buy/sell (**B**/**S**) |
| **Factions** | Pierline / Metro Watch / Syndicate reps (**U**); shop discount / pursuit pacing |
| **Presentation** | Title splash, intro fly-over cutscene (**Esc** skip), success/fail + finale ending banner |
| **Net** | Embedded / host / join UDP; pose/heat/phase/cash sync; chat (**Enter**/**Y**); ready (**K**) |
| **Meta** | 3 save slots (`[`/`]`), day/night, weather (**R**), minimap, onboarding compass, FPS (**P**) |
| **Tech** | GL + software fallback; shadows/reflect/bloom stubs; materials; Windows `NOMINMAX`; Release + xvfb smoke |

## 1.9.0
Intro cutscene fly-over; Meridian Night Vault finale; ending banner.

## 1.8.0
Materials (brick/metal/glass); water fresnel reflect stub; bloom-lite.

## 1.7.0
Factions stub + reputation HUD; reps in save JSON.

## 1.6.0
Police chase AI; Harbor loft safehouse; pursuit pips.

## 1.5.0
Loot tables; inventory UI; fence sell.

## 1.4.0
Host/join net modes; chat stub; ready check.

## 1.3.0
Movement polish; weather stub; footstep/impact cues.

## 1.2.0
Crew banter; Harbor Armored Depot; alarm sirens.

## 1.1.0
Quest journal; GL shadows; props; Windows `NOMINMAX` CI fix.

## 1.0.0
Vertical-slice polish — onboarding, balance, splash / banners, FPS toggle.
