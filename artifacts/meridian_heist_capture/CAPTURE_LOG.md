# Meridian Mutual heist capture log

Harbor Metro / HMPD / Meridian Mutual only.

| t (s) | Beat | Events |
|------|------|--------|
| 2.7 | 01_exterior | exterior spawn; zone_alley bed |
| 13.0 | 02_lobby | lobby enter; zone_lobby; phone_ring; printer |
| 25.6 | 03_disable_security | disable security; cone hit; patrol alert; badge/terminal |
| 40.3 | 04_vault_open | vault open; vault_motor; metal_stress |
| 52.3 | 05_alarm | alarm; lockdown ON; alarm_klaxon |
| 68.1 | 06_escape_alley | escape alley; lockdown OFF; police_radio |
| 82.5 | 07_hmpd | HMPD arrival; response vehicles |

## Summary

Shots written: 7
Duration: ~90s simulated
Audio: authored Meridian WAVs when SDL_mixer present; null backend logs cues.
Security: camera cones, guard investigate→escalate, lockdown on/off.
