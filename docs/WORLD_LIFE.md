# District vegetation and everyday props

`vaultline::upgrade_world_life(Scene&)` is a deterministic, once-per-assembled-scene visual pass. Call it after the six district builders. Repeating it leaves geometry, entity counts and mesh ownership unchanged. It does not alter source assets or gameplay systems.

## What actually existed

The baseline source audit distinguishes procedural objects from imported assets:

- `PlanterShrubA`, `PlanterShrubB`, and `LoftPlant` were three solid-looking **box meshes** representing vegetation. The first two have separate solid planter bases; the foliage entities themselves are non-solid
- The imported `BlkPlanterA`, `BlkPlanterB` and the planter inside `BlkStreetKit` contain **five sphere meshes each**, named `Planter_Foliage_0` through `Planter_Foliage_4`. Each sphere has 720 triangles: **15 sphere parts / 10,800 triangles**, representing three additional planted containers
- The shipped storefront GLB and its building-kit copy have two named planter containers, but those named primitives are planter frames, not another foliage system
- No tree, reed, vegetation billboard, sprite atlas or alpha-masked foliage system was found in the six district builders or shipped GLB material inventory. The five particle kinds (burst, rain, smoke, sparks and tire dust) are unrelated small geometry/FX entities and are intentionally untouched. HUD bitmap glyphs and authored sign/vehicle artwork are also untouched

The completed pass upgrades **six existing plant groups** and adds **nineteen boundary plant groups**. It does not claim to have replaced nonexistent vegetation sprites or rebuilt the particle system.

## District coverage

| District | Existing plants upgraded | Added plants | Resulting managed groups |
| --- | ---: | --- | ---: |
| Harbor Metro | 2 procedural shrubs + 3 authored sphere planters | 2 small street trees | 7 |
| Ridge Pier | 0 | 2 salt-tolerant-looking trees + 2 reed clumps | 4 |
| Ashcourt Market | 0 | 2 terracotta-potted trees + 2 low shrubs | 4 |
| Harbor Depot | 0 | 3 dry industrial-margin shrubs | 3 |
| Harbor Loft | 1 potted houseplant, in its existing footprint | 2 exterior trees | 3 |
| North Quay | 0 | 2 dry shrubs + 2 waterfront reed clumps | 4 |
| **Total** | **6** | **19** | **25** |

New vegetation is clustered in **13 spatial batches**, using 16-metre XZ cells so distant plants do not force a nearby plant into the wrong LOD. Every stem, twig, leaf and container in one cell is already one combined mesh, not hundreds of draw entities. There are no new solid colliders or interaction tags.

Trees have tapered, bent trunks, primary branches, secondary twigs, and folded lanceolate leaves attached to those twigs. Shrubs use the same smaller branching hierarchy. The existing loft plant gains a soil-filled pot and branching foliage. Reeds have thin three-sided stems, tapered leaves and occasional seed heads. Added tree pots use a tapered shell, rim and inset soil. All geometry is original code-generated, opaque and renderer-independent; there are no downloaded images, alpha masks, transparent foliage quads or new texture dependencies.

### Placement and routes

- Harbor trees: `(-28,-56)`, `(-57,43)`
- Ridge trees: `(74,-9)`, `(117,24)`; reeds: `(79,25.4)`, `(121,25.4)`
- Ashcourt trees: `(-107,24)`, `(-108,58)`; adjacent low shrubs: `(-106.4,25.2)`, `(-107.3,56.7)`
- Depot shrubs: `(75,-57)`, `(49,-60)`, `(75.3,-55.5)`
- Loft exterior trees: `(48.2,58.7)`, `(34.7,59.5)`
- North Quay shrubs: `(34,114.8)`, `(-15,115.2)`; reeds: `(-11,115.4)`, `(35,115.4)`

Coordinates are `(x,z)` in metres. These positions were coordinated with the ground module's water/land boundaries: reed roots sit on dry shore margins; loft planting sits beside the clear approach; depot shrubs sit at rear/unused edges. New plants are never inserted into an interior. The existing interior loft plant is upgraded in place. Existing doorways, fast-travel hubs, road/bridge routes, heist targets, crate positions, vehicle routes and solid footprints are not changed.

## Repeated prop coverage

The normal six-district build contains **194 procedural mesh replacements**:

- **153 utility entities**: 28 hydrants, 28 mid-block bins, 5 other bins, 4 slatted benches, 4 cylindrical bollards, 2 dumpsters, 40 louvered rooftop air-conditioning housings, 40 fan assemblies, and 2 recessed-soil planter bases
- **30 North Quay containers**: edge frames, raised side corrugations, divided doors, locking bars and handles. Neutral metal vertex colors prevent the original paint material from being multiplied by the same paint color twice
- **4 crane parts**: two open, braced masts and two lattice booms, replacing the opaque mast/boom boxes. Their original broad colliders stay byte-for-byte unchanged
- **4 market parts**: two slatted stalls with inset produce baskets and two pitched, striped canopies with scalloped valances. Produce stays inside the original stall footprint; only the non-solid visible tabletop contents rise above it
- **3 procedural plants** described above

The three surgically upgraded authored planter-containing entities bring the full replacement count to **197**. Unrelated authored hero props, brand plates, wear meshes, characters, vehicles, signs, security devices and the sealed interactable container are excluded.

## Authored sphere replacement safety

The merged Harbor meshes no longer carry primitive labels. A generic green-color or height filter would risk modifying unrelated art, so the pass reimports the shipped source GLB and selects only the five exact `Planter_Foliage_N` names. It fingerprints every selected triangle using its baked positions and vertex colors, canonicalized to a 10-micrometre/0.00001-color grid. All **3,600 unique triangles per planter** must match one-to-one in the existing runtime mesh before anything changes.

The runtime copy retains the complete original vertex array, including UVs, normals, colors and opacity, and preserves every non-foliage triangle index in its original order. Only matched sphere indices are removed; original branched foliage is appended. The full authored planter shell, labels and other kit contents remain in the near mesh. Authored props that were already detail-culled retain that behavior; the pass does not accidentally keep the entire large street kit visible as a far proxy. The entity's original material and shared imported-material identity are unchanged. Real image-mapped versions, missing GLBs, different triangle topology, changed positions/colors or incomplete matches are skipped safely and reported in `skipped_authored_planters`.

This deliberately leaves unused sphere vertices in the copied vertex buffer, avoiding any remapping of protected authored triangles. Those vertices have no draw indices and therefore do not incur triangle rasterization or ray-intersection work.

## Geometry and CPU budget

Measured by `world_life_tests` against the source-exact procedural family counts and production-imported GLBs:

| Selection | Original triangles | Near triangles | Distant triangles |
| --- | ---: | ---: | ---: |
| Procedural replacements plus 13 new plant batches | 2,328 | 32,280 | 14,314 |
| Three authored planter-containing entities, including unchanged street-kit contents | 107,000 | 97,802 | 0 (original detail cull) |
| **Total affected scene geometry** | **109,328** | **130,082** | **14,314** |

The full-world near-detail increase is **20,754 triangles**. Even when the authored-asset optimization is unavailable, the procedural/boundary work adds **29,952 triangles**, under the 30,000-triangle target. The procedural component drops **55.7%** at the existing renderer's mid-distance threshold. The 96,200 unchanged authored non-foliage triangles keep their original mid-distance cull. No full-shell far proxy is added for the large street kit; the zero in that row describes existing culling, not a newly claimed simplification of the authored kit.

Generated repeated props share meshes by family and source bounds. Spatial vegetation batches have conservative render bounds containing **both** near and distant canopies. Near and distant trees retain the same deterministic trunk/branch/twig structure and use fewer, slightly larger leaves at distance. Existing source meshes are never mutated in place.

## Verification

`tests/world_life_tests.cpp` verifies:

- finite generated positions/UVs, unit normals, nondegenerate triangles, valid indices and normals agreeing with winding
- every original transform, collider, solid/visibility/detail flag, name, tag and every material property and map pointer retained
- unchanged collected world-space collision boxes; no added solids or gameplay tags
- lower-triangle LOD for every procedural replacement and vegetation batch; original distance culling retained for authored kits
- conservative generated render bounds, deterministic independent rebuilds and idempotent repeat calls
- precise six-district/family counts and the 30,000-triangle fallback budget
- all fifteen authored sphere primitives matched; every original authored vertex and every non-foliage triangle unchanged; authored distance culling retained
- safe skips for changed authored sphere geometry and image-mapped authored assets

Run the test with the **repository root as working directory** so it can inspect the shipped GLBs. Link it with `world_life.cpp` and `Fury::Engine`. The test is separate from the renderer/backend tests and does not replace integrated world captures or a gameplay traversal.

A focused actual CPU software-renderer capture was also inspected for the generated tree: branches, twigs, folded leaf silhouettes and the recessed planter render as opaque geometry. Integrated district day/night/rain views and frame-time measurements remain the application-level validation responsibility.

## Explicit limits

This is an intentionally restrained small-tree/shrub urban palette, not a photorealistic tree library. Leaves are geometric and static; there is no wind animation, seasonal system, dense forest coverage, billboard atlas, soft leaf translucency or new dynamic particle art. Storefront decorative planter frames are retained as authored. Signs, hero material maps, branded labels, heist indicators, NPCs and vehicles keep their original art. No collider shape is made more detailed, even where the visual crane becomes an open truss.
