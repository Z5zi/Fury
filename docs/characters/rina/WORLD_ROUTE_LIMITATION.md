# Rina: preserved baseline world intersections

This individual art pass leaves gameplay and environment placement unchanged.
A complete 420-frame, 12 Hz actual-game patrol capture travelled 61.4791 m,
passing all four route corners and starting a second circuit. It exposed genuine
pre-existing building intersections, in addition to camera occlusion.

The exact baseline binary's `--world-audit` identifies these protected solids:

| Entity | Actual world collider bounds | Authored Rina waypoint |
| --- | --- | --- |
| Bldg21 | X 58–66, Y 0–18, Z −12–−4 | (62, 0, −8), at its center |
| Bldg11 | X 43.5–52.5, Y 0–8, Z 1–11 | (48, 0, 6), at its center |

`Scene::collect_solids` adds the local collider center to the entity position
and scales its half-extents. Both identified buildings have zero rotation and
unit scale. The current controller follows authored straight route segments;
NPCs do not acquire solid-world avoidance from a new visual mesh.

For these two buildings alone, **17.5 m of the 56 m route centerline lies within
solid footprints**. Including the unchanged 0.4 m agent radius gives 19.1 m of
footprint overlap. These are exact segment/AABB calculations from the production
world audit, not a reinterpretation of hidden camera pixels. The original route
is (48,−8) → (62,−8) → (62,6) → (48,6) → start, with speed 2.05 m/s and DayOnly
schedule. Height stays 1.68 m and root Y 0.84 m.

The issue was reported to the main project as a separate placement/gameplay
problem. No waypoint, spawn, home, prop, building, collider or AI behavior was
changed in this pass. Actual-game evidence retains the buildings and its visible
occluded intervals. Diagnostic-stage views show the whole model independently;
they are not substituted as proof of a collision-free patrol.

The full solid audit also finds BridgePillar at X 57.3–58.7, Z 5.3–6.7,
with its top at Y 0.75, on the westbound segment; and ParkedCar at X 45.9–50.1,
Z 3.05–4.95, Y 0–1.5, on the final southbound segment. The car's route overlap
is contained within Bldg11's overlap, so these values must not be blindly added
when calculating union length. These are also preserved and separately flagged.
