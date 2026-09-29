# Cycle-18 capture log

- Branch: `aaa-meridian-block-v18` (Z5zi/Fury); C17 snapshot committed first as its own commit
- Blender: `/home/box/blender/blender` 4.2.8 LTS, Cycles CPU (8 threads), OpenImageDenoise
- MPFB 2.0.17 (extensions.blender.org) + MakeHuman system assets CC0 (makehumancommunity.org)
- Ped builder: `scripts/build_harbor_peds_v18.py` → packed `.blend` per ped (beauty source), `hm_ped_*_v18.glb` (engine, 1k tex), studio proofs
- Logo clean-up: `scripts/clean_ped_textures_v18.py` (third-party logos painted out of the CC0 tee texture)
- Beauty: `scripts/render_meridian_block_beauty_v18.py` — PRIMARY evidence; full run ~9 min (log: `logs/beauty_v18_c18.log`)
  - 04: Blender cam (14.2, 0.30, 1.58) → (20.0, 0.12, 1.22), 45 mm, f/5.6, 128 spp; 04_peds_medium 55 mm f/4
  - 01/02/03/05: same cameras/materials as C17
- Ped proofs: `artifacts/aaa_meridian_block/blender_peds/` (full, face, silhouette per ped)
- Beauty outs: `artifacts/aaa_meridian_block/blender_scene_beauty/`
- Self-check before packing: all 04 / 04_medium / silhouette renders inspected; no spikes, cards or shell artifacts; no third-party logos
- Branding: Harbor Metro / HMPD / Meridian Mutual only; no Rockstar/GTA IP
- Timezone: Europe/London (BST)
- Packaged: 2026-09-29 17:56 BST
