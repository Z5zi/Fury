# Six-district ground and waterfront geometry

`vaultline::upgrade_world_ground(fury::Scene&)` is a deterministic post-process
for the existing Vaultline world. Call it after `build_harbor_metro` (which calls
the other district builders), before weather's asphalt-material cache, and before
the architecture/ecology passes. It is renderer-independent ordinary indexed
geometry and uses the existing concrete, asphalt, wood and metal material paths.

## The original problem

`StreetGrid` was an opaque 320 × 260 m plane at y = 0. All three authored animated
water planes are below it (y = −0.35/−0.4), so the base ground completely hid the
water wherever their bounds overlapped. Ten 14 m-wide, 140 m-long sidewalk planes
then overlapped most of the central streets. More surface texture did not change
those proportions or make the waterfront visible.

The pass replaces only the ground's render mesh with a subtraction-tessellated
set of land rectangles, clears the ten obsolete sidewalk render meshes, and
builds local frontage paving. Entity identity, tags, transforms, visibility and
collision data are retained. The five existing plaza/yard surfaces receive
matte masonry materials instead of metallic industrial-ground tinting.

## Authored shoreline and preserved land

These water apertures lie entirely within the original water planes:

| Area | X extent (m) | Z extent (m) |
|---|---:|---:|
| Harbor western basin | −24 … 8 | 55 … 74 |
| Harbor western inlet | −24 … −14 | 42 … 55 |
| Harbor eastern basin | 24 … 65 | 60 … 74 |
| Harbor eastern inlet | 49 … 65 | 38 … 60 |
| Water beneath western Harbor pier | −14 … 12.5 | 40.7 … 51 |
| Ridge Pier basin | 78 … 128 | 26 … 52 |
| North Quay basin | −14 … 42 | 116 … 130 |

Each aperture is clipped against original ground-level solid footprints plus
0.65 m foundation margins. The same scaled-collider convention as
`Scene::collect_solids` is used. Submerged bridge pillars are excluded from this
foundation operation, so they don't become floating dry islands. Composite
bank, jewelry store, depot and loft footprints also have complete protected
areas, including their interiors.

Explicit dry corridors preserve the North Quay approach and bridge (x =
12.5 … 23.5, z = 46 … 94), the loft promenade (x = 23.5 … 49, z = 56.5 … 60),
the headland behind the western pier, pier approaches and all six fast-travel
hubs. The original world-ground domain remains −160 … 160 by −130 … 130; no
global terrain support is deleted outside the aperture set.

Retaining-wall geometry follows the **union perimeter** of the openings. Shared
edges of the rectangle tessellation are removed, so there are no walls dividing
the water into a grid. Stone coping is on the dry side, with a dark tide band
below water level. The original water meshes, shaders, material values and UV
animation are untouched. `water_aperture_area` counts base-ground openings;
`exposed_water_area` subtracts the original two opaque pier-deck footprints.

## Coverage

- **Harbor Metro:** 28 existing building frontages plus bank/jewelry forecourts,
  2.1 m frontage walks, contextual road dashes, a bank pedestrian crossing with
  flush approaches, and the western timber pier on actual water
- **Ridge Pier:** five warehouse frontages, a contrasting street through the
  plaza, bridge lane/edge markings, waterfront apron, individual deck boards,
  submerged pilings and quay retaining walls
- **Ashcourt Market:** five shop frontages, warm-stone market spine and perimeter
  band, connector markings and pedestrian approach crossing, preserving the
  existing stalls and ATM access
- **Harbor Depot:** hall frontage, asphalt loading apron, selective loading-bay
  paint and a clear entry walk at x = 58; the hall, cage and garage stay intact
- **Harbor Loft:** 2.8 m frontage space, masonry peninsula and a continuous dry
  promenade to the North Quay approach; the floor/workbench/interior stay intact
- **North Quay:** four warehouse frontages, a cargo-yard spine, waterfront apron,
  sparse service markings and a retaining edge beyond the sealed-container job

Frontage curbs have 4.4 m flush gaps at their midpoints on every side. Slab joints
occur only within these local walks, at architectural rather than arbitrary
city-grid spacing. Markings and paving are clipped against building footprints
and water openings. Pier boards have real narrow joints and longitudinal wood
UVs; pilings are below/outboard of existing decks.

## Gameplay and performance contract

- No new solid entities, collision walls, entity movement or mission/tag edits
- Existing solid render meshes and materials are unchanged by this pass
- Ground simulation remains the existing flat-ground model; this is not a
  swimming mechanic or a navigation rewrite. Visual low curbs/quays intentionally
  do not introduce new gameplay barriers
- `WorldGround.Version1` makes a second invocation a strict no-op
- Geometry batches are separated by district, material and detail tier, with
  local-space vertices and a centered entity transform for useful culling
- Frontage slabs, curbs, quays and deck boards remain at distance; slab joints,
  paint, tide bands and piling collars use the existing near-detail cutoff
- No third-party assets or downloads, and no GPU-only geometry or material trick

The baseline-footprint fixture produces **29 entities, 6,782 added triangles,
60 replacement-terrain triangles, 46 frontage walks, 349 curb segments, 96 road
markings, 45 waterfront edges and 182 boards**. Open visible water is
approximately **3,614 m²** after pier occlusion. Runtime counts can differ when
additional original solid props intersect a basin; footprint protection is
deliberately dynamic. The tested triangle ceiling, including replacement
terrain, is 15,000.

## Tests and integration

Add `apps/vaultline/world_ground.cpp` to the Vaultline target and include
`world_ground.hpp` in the world-build call site. A focused test target consists
of `tests/world_ground_tests.cpp` and `apps/vaultline/world_ground.cpp`, linked
to `Fury::Engine`.

The tests use the original 42 generic building footprints and representative
hero walls, plus a new scaled/rotated test building inside a water basin. They
verify all original entity identities, tags, poses, visibility flags and
colliders; unchanged solid meshes and animated water; foundation center/corner/
edge coverage; all hub and approach land support; unoccluded samples in each
basin; actual water below the Harbor pier; triangle/entity budgets; finite mesh
data; and idempotence. They also verify that an empty or unsuitable scene is
left alone.

Standalone focused build using an existing engine build:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Iengine/include \
  tests/world_ground_tests.cpp apps/vaultline/world_ground.cpp \
  build/engine/libfury_engine.a -o /tmp/fury_world_ground_tests
/tmp/fury_world_ground_tests
```
