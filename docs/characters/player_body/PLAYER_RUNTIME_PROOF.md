# PlayerBody production input proof

The opt-in `--player-input-audit` driver is separate from character construction.
It requires exactly 300 bounded frames and conflicts with NPC motion, photo,
smoke, heist and unbounded sequence capture modes. With no flag, its code never
runs. `Application::on_diagnostic_input` is null by default.

The hook executes after SDL polling and before the stored input, production
pre-update, V/F toggles, Camera::update, scene collision resolution, Vaultline
on_update and actual world draw. Thus movement is produced by the real camera
and collision/controller functions. Calling only the game's on_update would
not have moved the player. Tests explicitly verify this callback order and the
existing pre-update consumption of F. This is deterministic InputState testing,
not physical keyboard/gamepad device testing or an interactive UI playthrough.

The driver starts at the existing (0,1.7,12) camera/player spawn. It skips only
the introductory overlay/cutscene. It never assigns player or actor positions,
never hides scenery, and leaves collisions enabled. Ordinary downward mouse
look frames the body in the existing third-person following camera. Mouse look
turns the actor, analog and W input walk, Shift sprints, Ctrl crouches, release
returns to standing, V hides/shows first/third person, and F hides/shows the
body in fly/grounded modes. Grounded Space is exercised and has no jump behavior
in the current controller; no jump animation is invented. The existing collision
pass holds camera Y at 1.7 even when crouched, while the body keeps its 1.75 m
standing rig and animates into crouch.

Each frame records requested input, resolved position/velocity, camera/body
heading and visibility, source animation state, stance/support fraction,
travel, mesh storage and height invariants. A read-only presentation accessor
copies the current state and is regression tested against mutation, time or
pose-cadence changes. At 17 digits, the final JSON preserves numeric round trips.

The exact earlier production trace reached 19.8000011 m/s actual sprint speed
and 3.77999997 m/s crouch travel. Sample 99's smoothed animation speed was
18.1216507 m/s; sample 154's crouch weight was 0.999993861. Rendering/simulation
cadence is 30 Hz offline and must not be called measured live FPS. Final source
and model-only baseline pairing are recorded in the final evidence manifest.

A normal patrol crosses the player near the end of the untouched world run.
These real occlusions are retained. The character stage and frozen original-
spawn world portraits remain separately labeled; neither is substituted for
this production-input proof. The default player follow arrangement uses Near;
Far is independently reviewed on the stage and through renderer tests.
