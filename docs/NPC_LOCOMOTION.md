# NPC and crew locomotion

## Preserved gameplay contracts

`NpcSystem` still follows the authored waypoint vectors and current waypoint index.
It does not construct a navigation mesh, choose another route, change missions,
change names/entity IDs, or alter dialogue, radii, heights, or authored speeds.
Only guards and enforcers can use the chase target. Chasing threats bypass the
non-chasing distance optimization; off-duty agents do not simulate.

Schedules retain their existing meaning:

- `Always`: present in both segments
- `DayOnly`: hidden and skipped at night, with chase cleared
- `NightTighten`: the captured day route is compressed toward its existing home
  by `0.52`, and its captured speed is multiplied by `1.45`

Repeated day/night application always uses the captured base route and speed,
so the compression and pace change do not compound. Invalid route indices are
repaired in either schedule segment and before movement. Going off duty also
clears stale movement signals so waking agents start from rest.

Crew activation, the caller's heist-phase `following` decision, the two-member
capacity/replacement behavior, and loot proximity rules remain unchanged. The
XZ radius boundary is inclusive, inactive crew do not contribute, and the
multipliers remain `1`, `1.175`, and `1.35` for zero, one, or two nearby members.

## Deliberate movement changes

### Bounded simulation and arrival

Both systems reject nonpositive or nonfinite `dt` without mutating motion state.
Finite positive updates simulate at most `0.25` seconds, split into steps no
larger than `1/60` second. Time beyond that hitch budget is discarded rather
than banked for a catch-up teleport. Tiny positive time steps stay finite.

All root translation follows the straight line from the current position to
the current target. Step distance is clamped to the distance remaining, so a
large frame or a chase target moved close to an agent cannot cause overshoot.
No inertia-driven curve is introduced through neighboring walls or doors.
This preserves the existing movement-only system; it does **not** add collision
avoidance or repair obstacles already on an authored route.

Patrols now reach within **1 mm** of each waypoint before advancing. The old
35 cm early switch cut corners, especially on narrow indoor routes. Reached
and duplicate waypoints are consumed within the current substep, bounded by
the route length; an entirely coincident route idles without an infinite loop.
Guard/enforcer approach arrival now uses **target radius + each agent radius**,
with 1 mm numerical tolerance. `NpcSystem::update` requires the target radius;
Vaultline supplies `app.config().player_radius` each frame. Its current on-foot
configuration is 0.45 m and the authored threat radii are 0.40 m, giving 0.85 m.
This deliberately replaces the former 0.20 m arrival: the final 0.65 m of approach
is removed so a threat does not advance its animated body into the standing eye.
Speeds, target updates, schedules, pending patrol target and braking are retained.
Invalid negative or nonfinite radii idle chasing threats; zero is a point target.

This is approach clearance for an initially separated on-foot player. It is not
collision response: player-driven intrusion, co-located spawn/teleport, crowd
intersections, obstacle avoidance and vehicle/body collision remain separate
limitations. An already overlapping target idles without push-out. Vehicle chase
targets still receive the configured on-foot radius, not a vehicle envelope.
Gameplay radii do not certify all animated mesh extents or watertight collision.
The 8.5 m guard heat/visibility range and existing failure/alert rules are unchanged.
Crew retain their **1 mm** follow settling tolerance.

### Starting, braking, and turning

| Motion | Acceleration | Braking | Maximum heading rate |
| --- | ---: | ---: | ---: |
| Patrol | 4 m/s² | 7 m/s² | 4.2 rad/s |
| Chase / crew follow | 8 m/s² | 12 m/s² | 5.5 rad/s |

Heading takes the shortest angular arc, including across `-pi`/`pi`, with
an 18 rad/s² turn-rate ramp and proportional easing near the desired angle;
final heading steps are clamped to avoid over-rotation.
Movement slows while facing away from a target, without bending its path.
Arrival speed decreases with remaining distance. The walk/idle weight approaches
the measured movement fraction at at most 4 units per second instead of jumping
to one at the first step.

Explicit gameplay pauses (off duty, distance optimization, inactive crew,
`following=false`, no route) stop translation immediately. The same hard stop
is necessary if an external target jumps inside the stopping distance; avoiding
overshoot takes precedence over maintaining a braking ramp. Normal target
approaches brake before the 1 mm numerical settling threshold. Target reversals
can include a short standing turn before movement resumes.

### Crew ground and coordinate corrections

Active crew are grounded at `height * 0.5` even while standing. The previous
hardcoded `0.9` center floated Rook (1.70 m) by 5 cm and Sparrow (1.72 m) by 4 cm.

`CrewSystem::update` takes **camera yaw**, whose zero faces +X:

- Forward: `(cos(yaw), 0, sin(yaw))`
- Right: `(-sin(yaw), 0, cos(yaw))`
- Target: `player + right * follow_offset.x + forward * follow_offset.z`

All authored offsets stay unchanged. Positive local X means right and negative
local Z means behind. This fixes the previous swapped-axis transform and its
contradictory behind-axis comment: Rook and Sparrow now trail the player on
opposite sides at yaw 0, +/-pi/2, and pi. NPC/body yaw remains the separate
+Z-facing model convention, `atan2(dx, dz)`.

## Presentation signals

Both `NpcAgent` and `CrewMember` expose the following runtime fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `travel_distance` | `double` | Cumulative actual XZ path distance, meters |
| `actual_speed` | `float` | Path distance in this update divided by simulated seconds |
| `velocity` | `Vec3` | Net XZ displacement in this update divided by simulated seconds; Y is zero |
| `turn_rate` | `float` | Accumulated signed heading change divided by simulated seconds, rad/s |
| `move_weight` | `float` | Smoothed measured movement/cruise-speed fraction, in [0,1] |
| `anim_phase` | `float` | Legacy cycle: actual XZ distance times 3.2 rad/m, wrapped modulo 2pi |
| `breathe_phase` | `float` | Active idle cadence: simulated seconds times 2.2 rad/s, wrapped modulo 2pi |

Use `travel_distance` **deltas**, rather than displacement between externally
assigned positions, to drive articulated stride/contact state. External mission
relocations therefore need not spin the gait cycle. `actual_speed` can differ
from the length of `velocity` when several route segments are traversed in one
update: the former measures path length and the latter net displacement.
The denominator is the bounded simulation time, not the unbounded hitch time.

`locomotion_speed` and `yaw_speed` are integrator state; they are not presentation
measurements. External relocations should clear them along with `velocity`,
`actual_speed`, and `turn_rate` while retaining cumulative distance and phase.
The shared internal `locomotion_detail::clear_motion` performs that reset.

Legacy phase only advances by representable position movement. It stays fixed
on arrival and during a pause, preventing stationary walk cycling. Active idle
breathing continues, including during the distance optimization; off-duty NPC
and inactive crew phases are frozen.

## Verification

`tests/npc_locomotion_tests.cpp` is a focused engine-level regression suite. It
covers:

- All four NPC roles, chase eligibility, far-threat chase, patrol resumption,
  and repeated chase/stand/follow/reactivation transitions
- 120, 60, 30, 10, and 4 Hz route traversal, strict straight-segment fidelity,
  reached/duplicate/empty routes, invalid indices, and arrival without jitter
- Guard/Enforcer summed-radius arrival across independently varied player/NPC
  radii and all authored threat speeds; stable idle, retreat/reapproach, moving
  targets and 100/250 ms hitches without overshoot; retained no-push-out overlap
- Zero, negative, NaN, +/-infinite, denormal, and huge finite time steps
- Measured displacement/speed/phase agreement, start/braking limits, shortest-arc
  bounded heading, idle blend settling, and continued active breathing
- Repeated day/night capture/restoration, off-duty skipping, and identity/dimension
  retention
- Camera-relative crew offsets at four cardinal yaws and four heights; ground
  contact; capacity, inclusive loot-radius, and active-member multiplier rules

A standalone build does not require SDL or the renderer:

```sh
g++ -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iengine/include \
  tests/npc_locomotion_tests.cpp engine/src/npc.cpp \
  -o /tmp/fury-npc-locomotion-tests
/tmp/fury-npc-locomotion-tests
```

The focused suite passed both the optimized build and AddressSanitizer plus
UndefinedBehaviorSanitizer. LeakSanitizer cannot run in this executor because
its process tracing is incompatible; the sanitizer run used
`ASAN_OPTIONS=detect_leaks=0`. Separate gameplay tests consume the extracted
production roster to validate real names, authored paths, and game integrations.
