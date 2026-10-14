# Articulated character performance

`character_animation.hpp` samples the original procedural character rig without
changing gameplay state, actor height, paths, dialogue, schedules, colliders, or
mesh topology. The animator has no dependency on the NPC, crew, mission, camera,
or dialogue systems. All signals flow **into** presentation.

## Integration contract

Create one `CharacterAnimationState` per stable actor, set
`state.sample.seed = character_seed(actor_id)`, and retain it while the actor
exists. Create a posed mesh once by copying the model's bind mesh. Each simulation
update supplies:

- `delta_time`: elapsed simulated seconds, normally the same bounded dt used by
  locomotion
- `distance_delta`: actual nonnegative XZ travel since the previous animation
  update, after movement, collisions and path following
- `travel_speed`: actual travel divided by simulated dt, in meters/second
- `move_weight`: the locomotion start/stop blend in [0, 1]
- `yaw` and `turn_rate`: actor facing and signed turn speed, in radians; a NaN
  turn rate requests derivation from shortest-arc yaw differences
- Optional `attention_yaw`, `attention_pitch`, `attention_weight`: a relative look
  direction and blend; angles are radians, not world-space coordinates
- Optional `talk_weight`: the dialogue system's presentation activity in [0, 1]
- Optional `crouch_weight`: player posture in [0, 1]

Call `advance_character_animation(state, input, model.rig.height)` on the normal
simulation clock, even if an offscreen/distant skin update is skipped. When a new
mesh pose is needed, call `sample_character_animation(model.rig, state)`, then
`apply_character_pose(model, pose, posed_mesh)`. Models and meshes must already
have been allocated; none of these animation operations allocates or changes
indices. Unchanged posed geometry does not get another dirty revision.

**Keep the entity at its standing height / 2, including during crouch.** Crouch
lowers the pelvis and bends the knees, hips, spine and neck inside the original
rig. Shrinking the entity or rebuilding a shorter model as well would apply the
height reduction twice and destroy the intended proportions.

The stateless `CharacterAnimationSample` overload supports test scenes and
captures. `phase_cycles` uses full cycles (two steps), rather than radians. A
stateless sample's moving weight also blends its foot spacing; the stateful
integration path is preferred for gameplay because it retains support positions
on stopping. `stride_length <= 0` selects the speed-dependent stride automatically.

## Gait and ground support

The bind convention is Y up and +Z forward, with the entity centered at half its
original height. Ground is `-height / 2`. Both legs are solved from their actual
rig lengths rather than scaled limb meshes:

1. A full gait cycle advances by measured travel / cycle stride length. The idle
   clock cannot advance the walk phase. Walk stride grows with actual speed and
   transitions smoothly toward a run for chases.
2. Support is 62% of each cycle when walking, reducing toward 47% at a run. Left
   and right feet are half a cycle apart, giving walk double-support and brief
   run flight rather than moving both legs together.
3. During support, local ankle Z decreases one meter for every meter of steady
   actor travel. The object-space ankle and global-facing, level sole compensate
   hip and knee motion. This exactly cancels translation in the steady straight
   support test.
4. Swing uses a quintic longitudinal recovery with the same endpoint velocity and
   acceleration as stance, a smooth squared-sine clearance arc, and restrained
   toe-up ankle flexion. Actual generated shoe vertices stay above ground.
5. Analytical two-bone IK places the knee in its anatomical forward bend plane.
   Hip, knee and ankle joints remain connected; matrices contain rotation and
   translation only. A softly bounded pelvis drop prevents hyperextension across
   the stride and provides the small natural change in body height.
6. Startup begins with the support leg underneath the body. On stopping, the
   stateful pose retains both feet's longitudinal placements while any lifted
   foot settles vertically. It does not slide both feet toward a neutral pose.

Pelvis yaw is opposed by the chest; arms counter-swing with bent elbows. Faster
travel increases clearance and elbow flexion, without scaling the anatomy.

Support is solved against flat local ground. This is not terrain collision IK,
world-space foot locking through sharp heading changes, or a root-motion
controller. Speed/stride transitions use smooth filters; the exact no-slip
invariant applies to steady straight support. Locomotion still owns actor motion
and facing, and existing collisions remain authoritative.

## Idle, looking, speaking and crouching

A stable ID seed varies the timing of gentle breathing, lateral weight shifts,
small head turns and gesture handedness. These are continuous low-amplitude
motions, rather than synchronized population-wide bobbing. Idle weight shifts
still solve both feet against ground.

Exponential filters soften turn, attention, talking, moving and crouch inputs.
Crouch posture additionally uses a smooth endpoint easing to avoid a sudden
near-straight-knee bend at onset. Attention is shared between chest and head, with
bounded neck compensation. Conversation adds asymmetric open arm/elbow/wrist
motion in front of the torso. It does not start or finish a dialogue or change
facing/gameplay targets on its own.

## Bounds and robustness

- Nonpositive or nonfinite dt leaves state untouched; valid dt is capped at .25 s
- Actual distance is nonnegative and bounded against extreme teleport inputs
- Phase stays in [0, 1); trigonometric arguments use double precision
- Input angles, weights, speed and turn rate are finite and bounded before use
- NaN/Inf inputs and corrupted sampled clocks cannot propagate into skin matrices
- Invalid or degenerate rigs return the identity pose
- The same seed, rig and input history produce bit-identical output

A caller that teleports a character should reset its presentation state. Original
finite actor heights in the supported physical range remain unchanged. Normal
NPC and player heights require no special casing.

## Verification

`tests/character_animation_tests.cpp` covers:

- 14,460 complete-cycle samples: four heights, five speeds, three crouch levels
- Actual shoe vertices above ground, exact support ankle height, level planted
  soles, forward-only knee bending, rigid transforms, and connected bone lengths
- World travel cancellation at steady support and distance-clock independence
- Toe-off, touchdown, phase-wrap, idle, turn, attention, talking and crouch
  continuity
- 6,000 additional stopping samples spanning walk/run phases, with no horizontal
  foot drift while settling
- Original anatomy during crouch, deterministic histories, and per-ID idle variety
- Zero-allocation skin updates, fixed indices/GPU handles/geometry identity, and
  normalized animated normals
- Nonfinite/extreme inputs, invalid dt, and invalid rigs

The suite can be built in isolation without touching a shared build directory:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iengine/include \
  tests/character_animation_tests.cpp engine/src/character_animation.cpp \
  engine/src/character_model.cpp engine/src/math.cpp \
  -o /tmp/fury-character-animation-tests
/tmp/fury-character-animation-tests
```

Repeat with `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer` for
sanitizer coverage. Rendering and gameplay integration captures are separate
checks; numerical tests alone do not establish their visual quality.
