# ChatGPT Remaining Top 8 — Meridian Mutual wishlist

Harbor Metro / HMPD / Meridian Mutual only (no GTA IP). Implements the
“Remaining Top 8” from `vaultline-polish-judgment-chatgpt.md`.

| # | Item | Implementation |
|---|------|----------------|
| 1 | Mission state proof | `meridian::WishlistController::sync_from_heist` / `apply_world_state` — pre_heist / alarm / escape dress the world (civilians, strobes, shutters, guard relocate, HMPD cue, street density, blockers). Logs `Mission world state -> …`. |
| 2 | Navigation readability | Door frames, path lights, landmark chandelier, sightline blockers (`tag=nav`). No arcade arrows. Log: `Wishlist: navigation readability…`. |
| 3 | Security system depth | Extra cams wired into `SecurityNet`, badge gate lobby→vault, terminals, server rack. E near badge/terminals. Log: `Wishlist: security depth…`. |
| 4 | Vault interaction detail | Dial / bolts / LED / maintenance hatch / emergency panel / tool point; staged sequence from `HeistPhase`. Log: `Vault machine stage -> …`. |
| 5 | Audio | Zone beds via `Audio::play_cue` (`zone_lobby`, `zone_security`, `zone_vault`, `zone_alley` + hums). Null/SDL_mixer stubs OK. Log: `Audio zone bed -> …`. |
| 6 | Damage / aftermath | Dropped papers, overturned chair, damaged panel, glass shards (`tag=aftermath`) after alarm. Log: `Wishlist: alarm aftermath layered`. |
| 7 | Performance profiling | `--profile` / `FURY_PERF` / **F3** dump: fps, frame_ms, entity/visible counts, renderer stats, RSS → `docs/MERIDIAN_PROFILE.md`. Log: `[profile] …`. |
| 8 | Cinematic mission capture | `--cinematic` or smoke script keyframes; PPMs under `artifacts/meridian_cinematics/`. Log: `Cinematic beat -> …`. |

## Run

```bash
cmake -S . -B build -G Ninja && cmake --build build --target vaultline
./build/apps/vaultline/vaultline --smoke --soft --profile
# Interactive: F3 profile dump, F12 screenshot; --cinematic for shot sequence
```

Smoke script forces `pre_heist → alarm → escape`, dumps profile, writes cinematic PPMs, then quits.

See also: `docs/HARBOR_METRO_INTEGRATION.md`, `docs/MERIDIAN_PROFILE.md`.
