# Selective runtime hero surfaces

The original generated PBR profiles described in [Surface detail](SURFACE_DETAIL.md)
are connected to actual Vaultline and Coastal scene entities. They are not
replacement GLBs or a detached material-board-only demonstration. Original asset
files, source base-color/scalar factors and geometry remain unchanged. Source
vertex UVs are retained except on image-free bench timber's copied runtime mesh,
where explicit longitudinal UVs provide the correct grain direction and density.
The renderer resolves selected profiles only when `FURY_SURFACE_DETAIL` is enabled.

## Audited placement

`harbor::hero_surface_material` uses an explicit asset/primitive-name allowlist,
applied **before** material grouping. It sets detail profile/physical scale and,
for bench timber, `detail_use_mesh_uvs`. Group equality includes these fields and
`detail_rotation`, so identical original factors cannot cause signage to inherit
a neighbouring material's detail or merge differently oriented surfaces.

- **Bldg3 storefront:** the authored brick shell and brick side piers; concrete
  belts, pilasters, sills, lintels, door surround and roof; timber front door;
  coated window frames, alley door and ladder; metal handles, kickplate, roof
  equipment, flashing and drainage hardware
- **Bank heroes:** the vault's brushed door/rings/threshold and structural
  hardware; deposit-box doors, dividers and rails; teller metal trim and handles;
  security-desk metal top/riser; trim-kit wainscot/baseboards; selected KIT wall,
  ceiling and ventilation parts
- **Two gameplay benches:** separate wood slats, concrete legs and coated frames.
  Their original named collision roots, poses and culling flags are retained;
  nine material groups per bench preserve the asset plates and wear overlays
- **Coastal cabin:** 26 timber body/siding instances with quarter-turned grain
  running horizontally across front and side weatherboards, plus 44 coated-metal
  roof slats. The 26 existing steel hardware instances
  receive a steel profile. Existing legacy wood still uses the shared renderer's
  wood-detail path; deck and door keep the unrotated longitudinal grain

Brick, concrete, wood and coated-metal profiles repeat every two world metres;
brushed metal repeats every metre. World projection is evaluated after placement,
so the storefront's fitted dimensions do not stretch the physical texture scale.
Bench timber deliberately uses baked asset-local UV0 instead: V follows the
bench's longitudinal X axis projected onto each geometric face, and U is
`cross(V, faceNormal)`. On nearly perpendicular cut ends, local Y supplies a
stable in-plane V fallback. Both UV axes retain 0.5 tiles/metre. The mapping moves
with each bench, so the two 0°/90° placements have the same seat/back grain
alignment. Runtime-only UV seams duplicate triangle-corner records without
changing positions, normals, colors, triangles, source files or colliders.
The grain maps introduce no extra plank joints; the actual slat geometry already
provides them. No new decorative mesh, collision volume or interaction target is
added.

## Deliberate exclusions

Glass/transmission, emission, alpha-masked or blended materials, existing legacy
texture slots, preassigned detail/UV choices and **any** authored image map are
protected. An imported glTF marker without images is eligible. glTF's default
emission strength of one with a black emission factor is correctly treated as
non-emitting; actual colored emission remains excluded.

Signs, logos, asset/number plates, screens, paper, fabric, awnings, rubber, grease,
source dirt/scuff overlays and polished bank stone retain their source materials.
Coastal water/glass, all ten white-painted window casings, canvas canopy, smooth
white boat hull and the five scalar material probes are unchanged. The casings
remain source-only because no white-painted timber detail profile is available.
Generic traffic and other one-material merged
props receive no semantic profile, because doing so would flatten unlike surfaces
into one finish. Optional imported pier/tree materials are not modified.

## Verification

`fury_hero_surface_tests` / CTest `hero_surfaces` runs from the repository root.
It checks:

- Actual GLB primitive selection and unchanged source factors, maps, UVs and
  baked geometry across six bank hero/kit assets
- Positive and negative semantic names, all four authored map channels,
  transparency/emission guards, and preservation of existing material choices
- A synthetic same-factor bench proving profiles are selected before grouping,
  protected branding remains separate and repeated grouping is deterministic
- Real storefront profile families, footprint containment, collision invariance,
  idempotence and the existing 28,372-triangle fitted geometry
- Actual grouped bench geometry and original collider placement/dimensions
- Real bench seat/back/end-face UV orientation and physical density at both
  placement yaws, including finite, noncollapsed mapping on bevels/cut ends and
  per-triangle-corner preservation of position/normal/color/opacity
- The actual Coastal cabin/roof/hardware instances and protected material probes

The bank test observes 22 vault, 96 deposit-box, 11 teller, ten security-desk,
four trim-kit and 38 KIT primitives selected. All six assets also retain
unselected primitives.

A separate bounded comparison against the pre-change implementation at
`bb2d03f863e3db7d7b6bc041b414afbe5c8a91b3` was compiled during development.
It verified all 494 Coastal instances/seven meshes
with identical vertex data, transforms and original factors; identical bank
world geometry (67,616 triangles, ten colliders); identical street world geometry
(269,392 triangles, 22 colliders); and identical storefront world geometry
(28,372 triangles, one collider). Storefront grouping changes from 44 to 50 groups
to protect unlike semantic surfaces sharing the same original factors.

These checks establish integration and invariance, not a visual-quality or frame
rate certification. Use production-renderer before/after captures to judge the
new material response. Missing high-detail silhouettes, unimplemented optional
glTF material extensions and the original asset provenance gaps described in
[the asset audit](ASSET_AUDIT.md) remain separate limitations.
