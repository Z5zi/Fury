# Vaultline changelog

Player-facing notes for the Fury **Vaultline** prototype. Honest scope: playable vertical slice, **not** AAA / GTA graphics. Original setting only — no Rockstar / GTA IP.

## 4.9.0 — screenshot stub + replay share (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **Screenshot stub** (**F12**)
  - Dumps the current framebuffer (GL `glReadPixels` or software color buffer / `SDL_GetRendererOutputSize`) to `vaultline_shot_N.ppm` in the cwd
  - Works in photo mode; HUD tip pip on save
- **Replay share** (**F11**)
  - Exports the ~8 s replay ring buffer to portable `vaultline_replay.json`
  - **Optional load tip** — press **F11 again** within ~4 s to load the share file into the scrub buffer (then **F10** to scrub)
- Version **4.9.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

## 4.8.0 — i18n stub + bitmap font (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **i18n stub** (EN + ES string table)
  - Subset of **HUD tips** (onboarding board / target / escape / done) + **mission names** (all 6 Harbor jobs)
  - Cycle **language** in settings (**O**) — last row; Left/Right or Enter; persists in `vaultline_settings.json`
  - Tip logs + mission-select logs use the active language
- **Bitmap font stub** — simple **5x7** glyphs for a few on-screen labels
  - **Cash** amount on the cash bar; **FPS** readout (**P**) as `F###`
  - Selected mission name strip on the **M** board; onboarding tip abbreviations
  - Falls back to geometric **bars** if a string is too long / unsupported
- Version **4.8.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

## 4.7.0 — lifetime stats + achievements stub (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **Lifetime** (lifetime career)
  - **Heists** completed, **cash earned**, **distance walked** (on-foot), **time played**
  - **F4** toggles a HUD stats panel (bars + achievement pips)
- **Achievements stub** — unlock banners (geometric HUD) for:
  - **First Score** — first successful extract
  - **Quiet Withdrawal** — Ashcourt ATM with peak heat ≤ 0.50
  - **Night Vault Cleared** — finale clear
  - **Harbor Millionaire** — $1,000,000 lifetime cash
  - **Career Operator** — 10 successful heists
  - **Lesson Learned** — first failed heist
  - Flags persist in save slots (`ach_*` + `distance_walked_m` / `time_played_sec`)
- Version **4.7.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

## 4.6.0 — SDL gamepad bindings (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **Gamepad** (SDL GameController / Xbox-layout)
  - **Left stick** — move (analog; combines with WASD)
  - **Right stick** — look (works without mouse capture)
  - **A** — interact (same as **E**: breach / doors / vehicles / breakers)
  - **B** — crouch (walk) / descend (fly) — same as **Ctrl**
  - **X** — sprint / vehicle boost — same as **Shift**
  - **Y** — cycle district **map** → mission **board** → closed
  - **Start** — settings (**O**)
  - **LT / RT** — optional boost (ORs into sprint/boost)
- Bindings documented in README controls table + **H** help overlay logs
- Version **4.6.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

## 4.5.0 — save export/import + cloud stub (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **Save export / import**
  - **F5** exports the **active save slot** to portable `vaultline_export.json` (cwd)
  - **F7** import — first press shows a **confirm tip** (HUD + log); **F7 again** within ~4 s loads the export into the active slot (overwrites) and autosaves
- **Cloud stub** — set `FURY_CLOUD_DIR=/path/to/folder` to mirror slot JSON into that directory on every autosave (local folder sync only — **not** real cloud)
- Version **4.5.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

## 4.4.0 — NPC schedules + shop hours (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **NPC schedules** (day/night cycle segment ~0.22–0.78)
  - **Civilians** denser daytime (extra plaza/pier walkers); thinner at night (most day-only off-shift; one always-on)
  - **Bank guard** patrol **tighter + faster** at night
  - **Fence Cass Vesper** only on duty during open hours (hidden / no Q at night)
- **Shop hours**
  - **Ashcourt fence** (**B**) **CLOSED** at night — HUD tip + buy/sell blocked; menu shows CLOSED banner
  - **Loft craft** (**G**) always available
- Version **4.4.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

## 4.3.0 — particles expand + decals stub (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **Particles expand**
  - **SmokePellet (X)** — grey rising **smoke puff** (was gold burst)
  - **Breach** — hot **sparks** on vault/safe entry
  - **Driving** — **tire dust** when moving above a low speed threshold
  - **Rain** streaks unchanged (clear / rain / storm)
- **Decals stub** — flat ground quads that **fade** (cap **64**)
  - Dark **bullet-hole-like** marks around breach impacts
  - **Skid marks** when boosting / hard braking in a vehicle
- Version **4.3.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

## 4.2.0 — dynamic music stub + stingers (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **Dynamic music stub** — layered intensity **0–1** from heat + heist phase (idle/approach/breach/loot/escape) and chase signals (patrol / alarm / Enforcer); loft damps the bed
  - With **SDL_mixer**: procedural ambient-idle vs chase beep **pattern / tempo** (slower soft pulses → faster chase ticks)
  - Null backend: silent, logs intensity band transitions (ambient / tension / chase)
- **Stingers** — short procedural cues on **heist success**, **heist fail**, mid-loot **complication**, and **Syndicate Enforcer** spawn
- Version **4.2.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

## 4.1.0 — denser interiors + district signage (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **Denser interiors**
  - **Meridian Mutual vault room** — deposit-box shelves, drawer rows, gold trays, ledger rack
  - **Crown & Cutler** — extra tall/wall display cases, glass tops, jewel trays + spark props
  - **Harbor loft** — bed, bookshelf + books, wardrobe, chair, plant, screen, rug
  - **Harbor Depot cage** — mesh bars, lockers, pallets, cones, forklift stub
- **District signage** — name **billboards** + **street-name blades** at Harbor / Ridge Pier / Ashcourt / Depot / Loft / North Quay; colored text-panel quads tagged `signage` with **emissive night** glow
- Version **4.1.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

## 4.0.0 — major prototype milestone (2026-09-07)

Honest: **major prototype milestone**, still **not** AAA / GTA. Ships the full **3.x** slice as a documented 4.0 baseline — stealth/cameras, district map + loft FT, vehicles/radio, crafting/fence upgrades, storm/lightning, heist complications + Syndicate Enforcer, settings/a11y — plus README Vaultline 4.0 banner, refreshed controls, and H-help covering every hotkey through 3.9. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **Docs pass** — README banner **Vaultline 4.0**; controls table refreshed for 3.x (stealth, craft, map, settings, radio, Q dialogue, etc.); CHANGELOG **3.x→4.0** tour
- **H help** — overlay + log legend lists all bindings through 3.9 (move/crouch, heist/breaker, Tab map / loft FT, G craft / X smoke, O settings / a11y, F6–F10, lobby/chat/ready, Esc)
- Light stability pass (help row count / version strings); still a playable vertical slice
- Version **4.0.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

### From 3.0 → 4.0 (feature tour)

Everything that landed across the 3.x line, now treated as the 4.0 content baseline (plus the 2.x/3.0 world already shipped):

| Area | What you get |
|------|----------------|
| **Stealth** | **Ctrl** crouch (walk); visibility meter; site **security cameras** + **breaker** (**E**) at Meridian / Crown / Depot |
| **Map / travel** | **Tab** district map (rects + blips; click/**1–6** focus); loft **fast travel** (**Enter**, $250, cooldown) |
| **Vehicles** | Getaway van cab+bed + night headlights; stealable Ashcourt sedan (**F**/**E**); in-vehicle **C** radio stub |
| **NPCs** | Display names + look-near nameplate; **Q** bark dialogue (fence/guard/crew pools) |
| **Craft / fence** | Loft workbench (**G**: SignalJammer / SmokePellet); **X** SmokePellet; fence **Better Payouts** / **Quieter Tools** |
| **Weather / water** | Storm on **R**; lightning flash + thunder; puddles when wet; water foam/fresnel; **2-cascade** shadows on high |
| **Heist spice** | Mid-loot **complications** (flicker / extra guard / lock jam / call-in); rare **Syndicate Enforcer** |
| **Settings / a11y** | **O** menu — sens / FOV / volume / quality / subtitles / invert Y; colorblind HUD / HUD scale / reduce flash; `vaultline_settings.json` |
| **Baseline kept** | Harbor jobs + North Quay + Night Vault; skills/dailies; photo/replay; lobby co-op sync; LOD/occlusion-lite |
| **Tech** | GL + software fallback; Windows `NOMINMAX`; Release + xvfb 124 + soft smoke |

## 3.9.0 — settings menu + accessibility (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **Settings menu (`O`)** — adjust and persist:
  - Mouse sensitivity, FOV, master volume (mixer gain when available)
  - Quality preset (same low/med/high as **F6**)
  - Subtitles/tips toggle (onboarding, banter, complication tips)
  - Invert Y look
- **Accessibility**
  - Colorblind-ish HUD palette toggle (remaps red/green accents)
  - Larger HUD scale option
  - Reduce flash (disables lightning screen flash / ambient spike; thunder still plays)
- Settings persist to `vaultline_settings.json` (saved on change/close/quit)
- Version **3.9.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

## 3.8.0 — heist complications + Syndicate Enforcer (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **Heist complications** — random mid-loot events (HUD tip when one fires):
  - **Power flicker** — lights dim briefly
  - **Extra guard** — Metro Watch spawn near the vault (chases)
  - **Lock jam** — loot progress pauses **1.5 s**
  - **Civilian call-in** — heat spike
- **Boss stub** — rare **Syndicate Enforcer** on high-tier jobs (tier ≥ 3): faster chase; downed by **SmokePellet** (**X**) or by escaping
- Version **3.8.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

## 3.7.0 — lightning + puddles + storm (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **Lightning** — during rain/storm, occasional **screen flash** + **thunder** audio cue and a brief ambient spike
- **Puddles** — flat reflective/dark patches on asphalt when wet (Harbor Metro + Ashcourt Market)
- **Storm** weather mode on **R** cycle (clear → rain → **storm** → auto-drizzle): heavier rain streaks + more frequent lightning
- Version **3.7.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

## 3.6.0 — loft crafting + fence upgrades (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **Crafting stub** — Harbor loft **workbench**: press **G** near the bench to open craft UI
  - **1 SignalJammer** — craft from **BearerBond + LedgerDrive** (passive: reduces camera heat while owned)
  - **2 SmokePellet** — craft from **Sapphire + BearerBond**; press **X** anywhere for a one-shot heat dump
- **Fence upgrades** — permanent Ashcourt shop unlocks with cash (**B**, near shop):
  - **4 Better Payouts** (+10% job payout)
  - **5 Quieter Tools** (shorter breach; stronger with **Silent Entry** skill)
- Craft gear + fence upgrades persist in save slots; version **3.6.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

## 3.5.0 — stealth meter + security cameras (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **Stealth / crouch** — hold **Ctrl** in walk mode (not fly): slower move, lower eye, quieter heat rise, softer footsteps
- **Visibility meter** — HUD bar rises near guards and active security camera cones (slower while crouched); decays when clear
- **Security cameras** — props at **Meridian Mutual**, **Crown & Cutler**, and **Harbor Depot**; standing in a camera cone raises heat (crouch avoids cam heat)
- **Breaker boxes** — stand near the yellow panel and press **E** to cut that site's cameras (lens dims)
- Version **3.5.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

## 3.4.0 — map UI + loft fast travel (2026-09-07)

Honest: still a **prototype** — not AAA / GTA. Original Harbor Metro only — no Rockstar / GTA IP.

### For players
- **Map UI** (**Tab**) — fullscreen-ish district map: colored district rects, player blip, objective blip; **click** or **1–6** to focus a district (**M** stays mission board)
- **Fast travel** — from **Harbor loft** safehouse only: with map open, **Enter** warps to the focused district hub (**$250**, **45 s** cooldown); loft hub itself is not a travel target
- Version **3.4.0**; Windows `NOMINMAX` kept; Release + xvfb 124 + soft smoke

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
