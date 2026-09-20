# Meridian Mutual — ~90s heist capture

Harbor Metro / HMPD / Meridian Mutual only (no GTA IP).

## Run

```bash
cmake -S . -B build -G Ninja && cmake --build build --target vaultline
./build/apps/vaultline/vaultline --heist-capture --soft --profile
```

Soft renderer is OK. Simulated timeline is ~90s. Exit code **0** on success.

## Beats → stills

Written under `artifacts/meridian_heist_capture/`:

| Beat | File | Notes |
|------|------|-------|
| Exterior spawn | `01_exterior.ppm` | alley bed |
| Lobby | `02_lobby.ppm` | phone + printer |
| Disable security | `03_disable_security.ppm` | cone hit, patrol alert, badge/terminal |
| Vault open | `04_vault_open.ppm` | vault motor + metal stress |
| Alarm | `05_alarm.ppm` | lockdown ON, klaxon |
| Escape alley | `06_escape_alley.ppm` | lockdown OFF, police radio |
| HMPD arrival | `07_hmpd.ppm` | response |

Timeline + audio/security events: `CAPTURE_LOG.md`.

## Security gameplay exercised

- Camera vision cones (log `Security: camera cone hit`) + translucent wedge visuals
- Guard patrol investigate → escalate
- Alarm lockdown sealing lobby↔vault until badge/terminal bypass
