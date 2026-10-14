# Meridian Mutual — authored audio

Harbor Metro / HMPD / Meridian Mutual only (no GTA IP).

## Cue → WAV map

| Cue | File under `assets/audio/meridian/` | Kind |
|-----|--------------------------------------|------|
| `zone_lobby` | `zone_lobby.wav` | looping bed |
| `zone_security` | `zone_security.wav` | looping bed |
| `zone_vault` | `zone_vault.wav` | looping bed |
| `zone_alley` | `zone_alley.wav` | looping bed |
| `phone_ring` | `phone_ring.wav` | one-shot |
| `printer` | `printer.wav` | one-shot |
| `radio_blip` | `radio_blip.wav` | one-shot |
| `radio_tick` | → `radio_blip` | alias |
| `vault_motor` | `vault_motor.wav` | one-shot |
| `metal_stress` | `metal_stress.wav` | one-shot |
| `police_radio` | `police_radio.wav` | one-shot |
| `footstep` | `footstep.wav` | one-shot (authored preferred) |
| `alarm_klaxon` | `alarm_klaxon.wav` | one-shot |
| `siren` | → `alarm_klaxon` | alias |

Hum / traffic refresh cues reuse the matching zone bed at lower volume.

## Wiring

- Zone changes via `WishlistController::update_zone_audio` swap looping beds and fire lobby phone/printer, security radio blips, alarm beds in alley.
- Vault open → `vault_motor` + `metal_stress`.
- Alarm / lockdown → `alarm_klaxon` (and `siren` alias).
- Escape alley → `police_radio`.
- SDL_mixer loads WAVs when available. Without SDL_mixer, the SDL2 CPU backend loads the same authored bank and plays it through an SDL audio device.
- `FURY_AUDIO_BACKEND=cpu` selects CPU playback explicitly; `null` selects intentionally silent cue logging.
- `FURY_AUDIO_ASSET_DIR` selects an explicit Meridian WAV directory. Otherwise, both backends search next to the executable and in common source/build layouts.
- See [CPU audio validation](CPU_AUDIO.md) for offline WAV capture and regression checks.

`apps/vaultline` POST_BUILD copies `assets/audio/` next to the binary.
