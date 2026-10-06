# Harbor Metro peds v18 — MakeHuman CC0 via MPFB2 (Gate C rebuild)

Cycle-17 (4.4) failed Gate C: the hand-built Antonia garment shells (Solidify +
Displace) and layered hair cards produced triangular/needle spikes on shoulders,
hair and hems at beauty distance. Cycle-18 removes that pipeline completely.

Builder: `scripts/build_harbor_peds_v18.py` (Blender 4.2 + MPFB 2.0.17 extension)
- Body: MakeHuman base mesh, per-ped phenotype (gender/age/height/weight/ethnic mix), subdiv 1
- Garments: MakeHuman CC0 fitted clothes (diffuse + normal + AO textures), fitted by mhclo
  - Rae: blouse + pencil skirt, Dane: two-piece suit + tie, Suki: denim shirt + jeans,
    Noah: field jacket + shirt + jeans, Ivy: tee + jeans; per-ped shoes
- Hair: MakeHuman alpha-textured hair meshes (ponytail / short / bob / long), per-ped tint,
  roughness 0.58, low specular, hashed alpha. NO solidify/displace/spike geometry.
- Skin: MPFB ENHANCED_SSS per-slot (body/ears/lips/nails) with pore normal, matte
  clearcoat 0.03, roughness 0.52; lips slightly glossier
- Eyes: MPFB procedural eyes (cornea, iris depth, per-ped iris colour), eyebrows, eyelashes, teeth
- Rig: MPFB default skeleton; authored idle (arms relaxed at sides, elbow flex, curled fingers,
  hip-width stance, pelvis/spine weight shift, head turn)

Proofs (Cycles, flat studio background):
- `hm_ped_<name>_full.png` / `_face.png` — key/fill/rim studio
- `hm_ped_<name>_silhouette.png` — 3/4 view on flat light-grey background at hero-shot
  distance, used to check for spikes/cards in silhouette (none found)

Assets: `assets/meshes/harbor_metro/peds/hm_ped_<name>_v18.glb` (engine, 1k textures, skinned).
Beauty appends the packed `.blend` (full shader fidelity) from
`/workspace/vaultline-blender/assets/peds_v18/` — regenerate with the builder.

Licenses:
- MakeHuman system assets (base mesh, skins, clothes, hair, eyes, brows, lashes): CC0
  (makehumancommunity.org, "makehuman_system_assets_cc0")
- MPFB2 is a GPL Blender tool; it is not shipped, only its CC0 asset output
- Harbor Metro / HMPD / Meridian Mutual branding only. No Rockstar/GTA IP.
