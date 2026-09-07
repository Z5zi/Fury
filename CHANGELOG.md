# Vaultline changelog

Player-facing notes for the Fury **Vaultline** prototype. Honest scope: playable vertical slice, **not** AAA / GTA graphics. Original setting only — no Rockstar / GTA IP.

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
