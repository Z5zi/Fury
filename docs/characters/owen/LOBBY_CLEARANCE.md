# Owen: actual-game lobby visibility and route clearance

## Findings

The six-second close-up failures have two independent causes. Neither requires
changing Owen's model or authored route to diagnose.

1. The live NPC capture camera is also the player position used by security and
   guard AI. Security cones trigger investigation, then guards approach that
   camera until their heads or torsos fill the view. `main.cpp` positions the
   camera at approximately lines 5030 and 7955, passes it to guard chase targets
   around 6358 and security around 7143, and keeps the live camera following at
   7573. `WishlistController::update_security_gameplay` responds to cone hits in
   `meridian_wishlist.cpp` around 1014. Frozen portraits and early live frames
   establish that the actor itself is visible. No security or capture behavior
   was changed for this review.
2. Owen's real lobby route crosses two real solid chairs. `NpcBankCust` starts
   at `(1.2, 0.87, -5.2)` and follows XZ waypoints `(1.2,-5.2)`, `(-1,-4.8)`,
   `(0.4,-5.6)` from `npc_roster.cpp`. `MMChair1` and `MMChair2` were placed at
   `(-1,0,-4.9)` and `(1,0,-4.9)` in `harbor_assets.cpp`. The second waypoint is
   inside MMChair1; the first segment also crosses MMChair2. `NpcSystem::update`
   follows these segments without a scene-collision query, so the overlap occurs
   during ordinary gameplay as well as a capture.

The frozen executable reached `(-0.99045044,0.87,-4.80173683)` at frame 36 with
12 fps simulation. MMChair1's real GLB seat occupies X `[-1.26,-0.74]`, Y
`[0.39,0.45]`, Z `[-5.15,-4.65]`; its collider occupies X `[-1.35,-0.65]`, Y
`[0,0.85]`, Z `[-5.25,-4.55]`. The first route segment crosses MMChair2's seat
footprint over X `[0.74,0.925]`, Z `[-5.15,-5.11636]`. These are route/geometry
intersections, separate from a foreground object merely blocking the lens.

## Minimal correction

Only the two inner chairs move forward by 0.95 m, to `MMChair1=(-1,0,-3.95)` and
`MMChair2=(1,0,-3.95)`. The outer chairs remain where they were. Both moved
chairs retain their name, mesh, yaw, scale, material, visibility, solid flag and
local `(0.7,0.85,0.7)` collider. `place_merged` applies the same entity transform
to the visual and its collider; there is no detached collision proxy to leave
behind. No NPC waypoint, schedule, navigation, interaction or mission state
changes.

The new inner-chair collider Z range is `[-4.30,-3.60]`. The nearest point of
Owen's unchanged route is 0.50 m away, leaving 0.10 m beyond his authored 0.40 m
radius. The gap between the inner-chair colliders remains 1.30 m, preserving the
central entrance-to-lobby aisle. Nearby original solids were checked from the
frozen whole-world audit:

- Entrance stanchions remain at X ±1.8, Z -3.4 with 0.30 m collider width
- Door posts remain at X ±2.35, Z -3.0 with 0.55 m collider width
- Outer chairs remain at X ±2.5, Z -4.9
- The non-solid threshold ends at Z -3.35; the moved seats/colliders remain behind it
- Queue poles/ropes stay at Z -5.8 and the teller counter at Z -7.6

The proposed new colliders overlap none of the audited world solids. No chair
has a gameplay tag, mission switch or narrative interaction; chair IDs are
retained. The correction changes furniture placement only.

A supplemental check of every original NPC route introduces no new swept
route/chair segment pair for another actor. `NpcGuard` already crossed MMChair2
on its authored segments 3 and 4 with its 0.40 m radius and still does. That
broader existing patrol/navigation issue is outside this Owen correction; the
result certifies Owen's path and player aisle, not a collision-free whole roster.
See `after-clearance/other-roster-route-check.json`.

## Runtime regression

`tests/world_runtime_tests.py` now runs a one-frame frozen `--npc-view` audit to
read the actual production roster route and radius. It checks all complete route
segments, including the closing segment and spawn-to-first-waypoint segment,
against all four actual chair colliders expanded by Owen's radius. The test also
checks the original `RouteEntrance`–`RouteLobby` aisle and each chair against all
other original solid colliders. It uses runtime JSON, not copied placement
constants or source-text matching.

The per-agent `*.state.json` audit gains an additive `radius` field from
`NpcAgent::radius`. This does not alter simulation. Before/after comparisons
must compare all pre-existing fields and validate radius separately; the older
frozen binary did not emit that field, so raw state JSON is not byte-identical.

Validation status: the focused actual-runtime gate passes on the frozen build03
candidate at `camera-review/after-clearance/vaultline`. It uses the audited
radius `0.40000000596`, all three closed-loop segments plus startup travel, all
four chair colliders, the entrance aisle and all other original solids. The
before/after world entity diff contains exactly the two chair Z positions; every
other original entity field remains identical. Every pre-existing frozen NPC
state field also matches exactly after removing the additive radius field.
Python syntax, continuous-segment helper checks and diff checks pass.

The negative replay uses the new actual audited radius with the original world
audit, after verifying that all older state fields are identical: the original
layout fails MMChair1, and fixing only MMChair1 still fails MMChair2. Both real
corrected placements pass. This is recorded in
`after-clearance/before-after-clearance.json`; the one-frame actual executable
gate is recorded in `after-clearance/focused-clearance.json`.

Four after stills were rendered and visually inspected: face, front body, side
body and frame-12 walking body. The front body now has clear legs, while the
same-camera live body frame retains the normal route and stride. The contact
sheet labels the model difference separately: the left column is the older
generic Owen from the frozen Tessa build and the right is the build03 Owen model
plus the two chair positions. Whole-world/model integration validation remains
owned by the main upgrade task.

## Honest actual-game capture settings

All baseline images were produced by the frozen, validated Tessa-era
`out/character-study/owen/baseline-bin/vaultline`, using the full live game scene
and unchanged assets, NPC routes and security logic. Captures run in fresh
temporary working directories with an assets symlink. The common settings are
`--soft --no-hud --width 640 --height 640 --capture-fps 12` and environment
`SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy FURY_AUDIO_BACKEND=null
FURY_CPU_THREADS=2 FURY_NET_PORT=18431`.

| Evidence | Additional settings | Scope |
| --- | --- | --- |
| Frozen face | `--npc-view NpcBankCust --npc-distance 1.1 --npc-orbit 0 --frames 1` | Clear front face at authored spawn; simulation frozen |
| Frozen side body | `--npc-view NpcBankCust --npc-distance 3.2 --npc-orbit 90 --frames 1` | Full standing silhouette, lobby scenery visible |
| Live full body still | `--npc-motion NpcBankCust --npc-distance 2.8 --npc-orbit 45 --frames 12` | Clear full body after one second of ordinary locomotion |
| Live body sequence | Same body settings, `--frames 18 --capture-sequence DIR` | Frames 9–18 unobstructed; earlier frames honestly show the original MMChair2 overlap |
| Live face sequence | `--npc-motion NpcBankCust --npc-distance 1.1 --npc-orbit 0 --frames 12 --capture-sequence DIR` | All twelve face frames visible; security still runs |

The existing six-second shots must not be used as evidence that Owen's model is
invisible or replaced. The frozen body orbit 180 points through the teller area
and is also unsuitable. Early live frames are a bounded comparison view, not
proof of an unobstructed full route or unchanged security state.

Raw PPMs, PNGs, logs, presentation/state/renderer audits, original frame sequences,
12 fps MP4s, commands and hashes are under
`out/character-study/owen/camera-review/`. In particular:

- `commands.json`: exact commands, environment and temporary working directory
- `MANIFEST.json`: executable, source fingerprint, asset and raw evidence hashes
- `geometry-review.json`: GLB seat bounds, route intersections and actual frame-36 position
- `world-before.json`: original real-world transforms and collider bounds
- `view-motion-contact.png`, `body-angles-contact.png`: reviewed camera alternatives
- `motion-body-contact.png`, `motion-face-contact.png`: every captured sequence frame
- `after-clearance/lobby-before-after-contact.png`: four matched camera pairs,
  explicitly labeled with model/build differences
- `after-clearance/capture-commands.json`: exact after capture commands

Frozen executable SHA-256:
`df5591436129e2539e21dcd1fbffe03495e7188b7bccbb300ceea68718828ed4`.
Renderer-reported source fingerprint:
`2a29d5197930562a1d41a39ccd6a7f2da62a2f57d5d108596b4de9047e60ebcf`.
After build03 source fingerprint:
`3159a469888f31a93e0b72090196d412f6b4d7a320a07dbeb48936a5b33461bd`.
`MANIFEST.json` separates baseline and after-build hashes and labels every file's
phase; the after stills are an intermediate model review, not a final character
quality sign-off.

No scenery or actors were hidden, no NPC was relocated, no gameplay trigger was
disabled, and no shared build was changed during the baseline camera audit.
