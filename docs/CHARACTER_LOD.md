# Per-entity character LOD distance

An entity with both `mesh` and `lod_mesh` can set `lod_distance` to a positive,
finite distance in meters. `Application::draw_scene` then selects its proxy once
the camera's nearest distance to the transformed primary-mesh AABB is strictly
greater than that distance. At the threshold, the primary mesh is retained.

For example, setting a character's `lod_distance` to `12.f` lets its more detailed
close mesh switch at 12 m while ordinary entities keep the application's normal
threshold. The default application configuration uses half the 90 m cull distance,
so existing entities continue switching at 45 m. The value is a world-space
distance after the entity's translation, rotation and scale, rather than a
distance to its origin or gameplay collider.

## Compatibility rules

- `lod_distance` defaults to zero. Zero, negative, NaN and either infinity inherit
  the existing application rule: positive `AppConfig::lod_mid_distance`, otherwise
  half a positive `cull_distance`, otherwise no distance-based LOD.
- A positive finite override works even when global distance culling and global
  LOD are disabled. It affects only mesh selection; it cannot extend cull range.
- Without `lod_mesh`, the override has no effect. Detail-only clutter still uses
  the global hiding threshold; ordinary entities without proxies keep their mesh.
- Each entity gets its own visibility settings. An override never mutates
  `AppConfig`, shared mesh data, or the settings used by neighboring entities.
- LOD selection uses the primary bounds. Distance, sector and camera-plane
  visibility still use the conservative union of primary and proxy bounds, so a
  larger proxy cannot disappear too early or influence its own selection.
- CPU ray and DXR retain their existing behind-camera shadow-caster behavior.
  Distance and sector exclusions remain active. Raster backends retain their
  existing camera-plane rejection and safety margins.
- Gameplay colliders, collision collection and entity transforms are unchanged.

This adds a hard distance switch, matching existing world LOD behavior; it does
not add cross-fading or hysteresis. The primary and proxy should have compatible
silhouettes and poses. An animation updater that chooses which mesh to skin must
use the same resolved distance, transformed primary bounds and strict comparison.
When vertices change in place, call `Mesh::mark_dirty()` so the existing bounds
cache and rendering backends see the new geometry revision.

## Regression coverage

`character_lod` / `fury_character_lod_tests` drives the real default application
draw path with SDL's dummy video driver. CPU-ray submission statistics identify
the selected mesh even when it is offscreen. Software-raster and CPU-ray
framebuffers are also compared byte for byte with explicitly drawn near and far
reference meshes.

Coverage includes threshold equality and crossings in both directions; invalid
and default values; explicit overrides with global limits disabled; rotated,
mirrored and nonuniform transforms; local geometry offsets; same-count animation
revisions; primary/proxy union bounds; sector and distance culling; mixed ordinary
and overridden entities in both traversal orders; detail-only hiding; gameplay
collider invariance; and offscreen CPU-ray shadow-caster retention. Existing
`world_visibility` tests cover the shared geometry evaluator's raster camera-plane
rules and the backend-neutral shadow-caster exemption. This headless test does
not certify a native DXR device.

After configuring the normal build:

```sh
cmake --build build --target fury_character_lod_tests fury_world_visibility_tests --parallel 2
ctest --test-dir build -R '^(character_lod|world_visibility)$' --output-on-failure
```
