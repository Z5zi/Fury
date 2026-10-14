# Geometry-aware world visibility

`Application::draw_scene` uses render geometry bounds for distance, indoor-sector,
camera-plane and detail/LOD decisions. Gameplay colliders remain unchanged.

## The regression

StreetGrid is a 320 × 260 m mesh centered at the world origin. A camera near
`(146, 1.7, 110)` is over its surface but more than 90 m from its origin. Origin
distance incorrectly removed the whole street in outer districts. The previous
camera-plane check also substituted the default 0.5 m gameplay collider for
render geometry, which could reject ground or batched facades facing the camera.

The fix applies at ordinary runtime quality distances; it does not depend on
larger cull distances used for deterministic overview captures.

## Bounds and visibility rules

- Local bounds include all mesh vertex positions, including geometry offset from
  the entity origin. Unreferenced vertices can make the bounds looser, never smaller.
- World bounds transform the local center and use the absolute affine matrix
  times local half-extents. Rotation, nonuniform scale, negative/mirrored scale,
  zero scale and shear are supported without inverting a transform. Intermediates
  use double precision and extents round outward.
- Distance is the nearest point on the world AABB to the camera. A surface under
  the player stays nearby even when its origin or most of its geometry is distant.
- Detail/LOD switches use this distance to the primary mesh. The optional proxy
  cannot change which distance selects it. Visibility checks use the union of
  primary/proxy bounds so a larger proxy is not rejected prematurely.
- Sector hiding rejects only bounds wholly outside the sector. A ground plane,
  wall or facade intersecting the sector remains available even with an exterior
  origin. It does not clip a retained mesh into indoor/outdoor parts.
- Raster backends reject a bound only when its entire projected extent is behind
  the camera. The existing 2 m near-camera exemption and 0.25 m plane tolerance
  remain. CPU ray and DXR still bypass camera-plane rejection for shadow casters;
  their existing distance and sector limits remain active.
- A nonpositive cull distance disables distance culling. A nonpositive explicit
  LOD distance uses half a positive cull distance; both disabled means no distance
  LOD. Empty/nonfinite geometry or invalid transforms yield unknown bounds and
  fail open rather than accidentally hiding potentially visible geometry.

## Cache lifecycle and mutation contract

`MeshBoundsCache` stores local bounds by the mesh's unique `geometry_identity`,
with `geometry_revision`, vertex count and index count validating each entry.
Repeated instances and transforms reuse the local scan. Changed revisions replace
the same entry rather than accumulating per-revision records. Mesh addresses are
never retained, so an allocator reusing an address does not reuse stale bounds.

Each scene draw starts and ends a cache pass. Entries not used in that pass are
removed at its end. Persistent cache size is bounded by unique visible-entity
primary/proxy mesh identities in the latest scene pass; transient peak storage
is the previous and current pass working sets. There is no arbitrary capacity
that would repeatedly evict large active scenes. Traversal/eviction uses ordered
identity keys, and clearing or replacing a scene releases old entries on the next
completed pass. Distance-culled visible entities retain entries for the next frame.

Code that changes mesh positions in place must call `Mesh::mark_dirty()` after
the change. The existing humanoid animation already follows this contract. Count
changes provide an additional guard, but do not replace marking same-size edits.
World bounds use the current entity transform each draw in constant work per
instance. The only repeated vertex scans are newly encountered or changed meshes.

These conservative AABBs can retain extra geometry when rotated, sparse, or widely
batched. This is intentional: false-positive draw submission is preferable to
visible holes. This pass does not add frustum/occlusion queries or change gameplay.

## Regression coverage

`world_visibility` / `fury_world_visibility_tests` covers the distant-origin
StreetGrid at ordinary 90 m culling, sector intersection/exclusion, rotated and
mirrored nonuniform geometry, zero scale/shear, changing animation revisions,
many shared instances, replacement identities, bounded cache retirement,
extent-based LOD/proxy behavior, fully distant/behind-camera rejection, ray shadow
preservation, near-plane margins, and fail-open invalid bounds.

Run after configuring the normal build:

```sh
cmake --build build --target fury_world_visibility_tests --parallel 2
ctest --test-dir build -R '^world_visibility$' --output-on-failure
```
