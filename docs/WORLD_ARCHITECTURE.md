# Runtime district architecture

`vaultline::upgrade_world_architecture(Scene&)` replaces the decorative box
facades across the playable world after all districts are constructed. It uses
original runtime geometry and existing material profiles, with no downloaded
assets, generated images, new authored textures, or gameplay changes.

## Coverage and visual direction

| Area | Upgraded shells | Character |
| --- | ---: | --- |
| Harbor Metro | 27 | Masonry/concrete street blocks, individual recessed bays, ground-floor fascias and canopies on smaller blocks, cornices, parapets, service crowns on taller blocks, rooftop plant |
| Ridge Pier | 5 | Brick industrial halls, broad multi-pane windows, loading shutters, standing-seam pitched roofs, raised ventilation monitors |
| Ashcourt | 5 | Warm brick and muted plaster-toned shop fronts, deep shop fascias, shallow canopies, pitched clay/slate-colored roofs and chimney stacks |
| North Quay | 4 | Restrained blue-gray/olive metal industrial halls, structural ribs, loading bays and three-tooth northlight roof profiles |
| Harbor Depot and Harbor Loft | 2 exteriors | Roof profiles, parapets, roof ventilation/chimneys and trim attached to the existing split front-wall sections |

The separately authored `Bldg3` storefront is deliberately retained. Mission
buildings, branding, doors, garage bays, interiors, interactive props, and all
unrelated entity names are excluded. Exact numeric suffixes are required for
`Bldg`, `RidgeBldg`, `AshShop`, and `NQWarehouse`; prefix lookalikes are not accepted.

## Facade construction

The old shell entity remains the single collider and gameplay identity. Its
visual mesh is replaced with masonry panels surrounding actual window cutouts.
Each bay has glazing recessed 16 cm, reveal surfaces, painted frame faces,
projecting stone sills, and small mullions. Window dimensions are authored in
metres and fitted to evenly spaced structural bays, rather than continuous
emissive bands. All four elevations are constructed, including building backs.

Opaque, dark blue-gray glass closes the decorative non-enterable shells. A
stable minority of inset warm panes carries the existing `window` tag for the
day/night lighting cycle; the rest of the glazing remains dark. Geometry and
palettes depend deterministically on district, dimensions and source name.
No random generator or frame-dependent state is used.

Only matching legacy `WinX<number>` / `WinZ<number>` entities are hidden. Matching
requires their original face position, height and `window` tag. Authored map
materials and unrelated window entities are retained. Rotated custom shells do
not trigger axis-aligned strip removal.

## Collision, portals and authored protection

The original entity's name, tag, visibility, transform, solid flag and local
collider are unchanged. Added entities are always non-solid. Source meshes are
never edited in place, so shared meshes remain safe. Authored maps, glass,
water, transmission, transparency, masked materials, emissive materials and
mission-tagged shells are protected. Empty tags plus the explicitly decorative
`scenery` and `building` tags are accepted.

Depot and Loft use a separate exterior-only routine. Their source wall and roof
meshes/materials are not replaced. Roof work begins above the existing roof;
front-wall trim stays within the left and right solid sections. The doorway gap
is never bridged below the existing roof. Their portals, doors, signs, furniture,
loot, security devices and path colliders are untouched.

## Batching and performance

Geometry is merged per source building and material purpose: roof, stone,
frames, glass, occupied panes, shop frontage, and fine detail. There is no
one-entity-per-window or one-entity-per-trim explosion. Small mullions, seam
strips, shutter grooves and vent detail are distance-culled using `detail`.
Stonework has a small cornice-only mid-distance proxy. Main facade openings,
window glass, roof silhouettes and structural frames survive at range.

The full-current-world fixture verifies 41 shells plus two landmark exteriors,
834 individual window bays, fewer than 48,000 new active triangles and 273 added
entities. This is below the 60,000-triangle architecture allocation. Triangle
statistics include replacement facades and every added active mesh, but exclude
alternate LOD meshes and unchanged source geometry. They conservatively count
replacement triangles without subtracting the old box triangles.

Each upgraded source has a stable `Architecture.<source>.Roof` batch marker.
Calling the function again creates no meshes or entities, changes no geometry,
and reports zero newly upgraded shells. Newly appended district shells can
still be processed on a later call.

## Integration

Include `world_architecture.hpp`, compile `world_architecture.cpp` in Vaultline,
and call `vaultline::upgrade_world_architecture(scene)` after
`build_harbor_metro(scene)` has constructed every district. The return value
contains total/per-district shell coverage, protected-shell count, landmark
coverage, window bays, removed strips, batch count and active triangle count.

`tests/world_architecture_tests.cpp` links with the engine and module. It uses
all production shell dimensions and checks:

- Full district and landmark coverage, budget and batching limits
- Bit-exact solid collider collection, original transforms and gameplay tags
- Genuine facade cutouts and 16 cm glazing recess, physically scaled windows
- Roof silhouette height on every upgraded building
- Finite positions/UVs, unit normals, valid indices, nondegenerate triangles,
  and matching triangle winding
- Open Depot/Loft portal volumes, protected authored/interactive entities,
  selective strip removal and source-pose propagation
- Per-building idempotence and incremental new-shell processing

The focused test can also be built independently with the module plus the
engine's `mesh.cpp`, `math.cpp`, and `scene.cpp`; it needs no windowing system,
GPU, asset downloads, or renderer.
