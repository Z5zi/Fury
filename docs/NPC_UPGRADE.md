# Complete character population upgrade

This report records the original whole-population articulated upgrade. Its
generic models are original code-authored assets under the repository's MIT
license. The later [individual character study](characters/ROSTER_PLAN.md)
adds dedicated profiles for Mira, Jon, Tessa, Owen, Lia, Nell, Pax and Rina, using explicitly licensed
CC0 anatomical data with Fury-authored clothing, hair, materials and rig
integration. See the source provenance for [Mira](characters/mira/ANATOMY_PROVENANCE.md),
[Jon](characters/jon/ANATOMY_PROVENANCE.md), [Tessa](characters/tessa/ANATOMY_PROVENANCE.md),
and the linked individual reports in the roster plan for subsequent characters.
No runtime asset service or network request is required.

The result is a compact, articulated low-poly population. It is not photorealistic
AAA character art. The screenshots and videos in the validation report come from
the game renderer, at the characters' real world locations.

[Runtime comparisons, motion videos, tests and measured costs](NPC_VALIDATION.md)

## Population coverage

| Production identity | Character / wardrobe |
| --- | --- |
| NpcCivA | Mira Vale, commuter |
| NpcCivB | Jon Keel, market worker |
| NpcCivC | Tessa Quill, commuter |
| NpcCivAsh | Nell Ash, market worker |
| NpcCivD | Pax Wren, dock/work vest |
| NpcCivE | Rina Holt, dock/work vest |
| NpcTeller | Lia Merrow, bank staff |
| NpcBankCust | Owen Pike, commuter |
| NpcGuard | Sgt. Hale, security |
| NpcDeskGuard | Ofc. Renn, security |
| NpcAlleyHmpd | Ofc. Vale, security |
| NpcFence | Cass Vesper, tailored broker |
| CrewRook | Rook, utility harness / technical equipment |
| CrewSparrow | Sparrow, scout backpack |
| NpcExtraGuard | Metro Watch, security reinforcement |
| NpcEnforcer | Syndicate Enforcer, heavy protective equipment |
| PlayerBody | Existing optional third-person player |
| GhostLoop | Existing network avatar, cyan identification vest |

There are **18 possible stable actor IDs**, **16 at normal startup**, and
**11 authored wardrobe roles**. Reinforcement/enforcer actors still spawn only
through their original complication triggers, except when explicitly selected by
the bounded inspection CLI. Replay trail markers are intentionally still markers;
they are not additional humanoid NPCs. All twelve original regular definitions
and both crew definitions now live in shared production factories, pinned by
independent baseline fingerprints.

## Visible changes and runtime behavior

- Shaped heads/jaws, eyes, ears, noses, brows, hair, layered clothing, hands,
  flat-soled boots, belts and role-specific accessories replace the box figures
- Independent skin/hair/clothing palettes and body proportions derive from stable
  IDs; role does not determine complexion, and near/far LODs retain identity
- Seventeen-joint deformation runs on the CPU for every backend. Normalized
  normals, geometry revisions, stable mesh ownership and original material colors
  reach software raster, CPU ray/path, OpenGL and the unchanged DX12 mesh interface
- Real traveled distance drives gait. Acceleration, braking, heading, arm swing,
  articulated crouching, idle breathing and restrained conversation gestures are
  blended rather than snapping or uniformly scaling the body
- Near/mid/far pose updates are limited to 30/15/about 8 Hz; gameplay continues at
  its own cadence. Models are constructed once, with no per-frame skinning
  allocation or topology rebuild. Dynamic respawns reuse cached meshes/entities
- Exact waypoint arrival avoids overshoot and early corner cutting. Existing
  authored routes, speeds, schedules, identities, dialogue, collisions, mission
  and payout rules remain under the original gameplay systems
- Crew follow offsets now match the camera's actual coordinate convention;
  player/network heading does too. Player crouch keeps the standing rig height
  and bends the skeleton instead of shrinking the body twice
- Q targeting now uses the camera's forward axes. A real Q bark also starts the
  corresponding actor's head/upper-body attention gesture; the original dialogue
  text and speaker roles remain unchanged

See [model and asset contracts](CHARACTER_MODELS.md),
[animation and foot-support limits](CHARACTER_ANIMATION.md),
[locomotion](NPC_LOCOMOTION.md),
[gameplay/controller validation](NPC_GAMEPLAY_VALIDATION.md),
[presentation review](NPC_PRESENTATION_REVIEW.md), and
[renderer/cache validation](CHARACTER_RENDERING.md).

## Inspect the real game

```sh
# Normal game, all detailed characters enabled by default
./build/apps/vaultline/vaultline --soft

# Same game/controllers, previous geometry and animation presentation
FURY_NPC_DETAIL=0 ./build/apps/vaultline/vaultline --soft

# Frozen portrait at the original world position
./build/apps/vaultline/vaultline --cpu-ray --npc-view CrewRook \
  --width 640 --height 640 --frames 2 --spp 2 --bounces 2 \
  --no-hud --capture rook.ppm --npc-audit rook.json

# Real patrol simulation and camera following; 4 simulated seconds
./build/apps/vaultline/vaultline --soft --npc-motion NpcCivA \
  --width 480 --height 360 --frames 48 --capture-fps 12 \
  --no-hud --capture-sequence mira-frames --npc-audit mira.json
```

`--npc-distance 0.5..30`, `--npc-orbit -180..180` and `--npc-elevation -60..60`
adjust inspection framing without relocating the actor or changing collision.
Elevation adds degrees to the established slightly raised framing, rotating the
eye around the same target at the same actual radius; positive values raise the
eye. Omitted or explicit zero elevation preserves the original camera math and
pixels. Small distances target the face. Buildings, furniture, vehicles and other
actors can genuinely occlude a camera; adjust its orbit/elevation rather than
hiding scene geometry. A modest elevation can look over approaching guards, but
the camera can still intersect ceilings or beams. Static inspection
supports all 18 IDs; motion inspection supports NPCs/crew. Player/network motion
requires real player/network inputs, so those IDs explicitly reject the scripted
motion-camera option.

The live inspection camera is also the game's player position. Changing framing
can therefore change guards' real chase trajectories or other camera-dependent
gameplay responses; elevation does not suppress, freeze or override those
systems. For before/after art comparisons, use identical distance, orbit,
elevation, cadence and frame count. The render audit records final camera eye,
target, yaw, pitch and requested framing parameters for checking that match.

`--npc-talk` with `--npc-motion` invokes only the presentation gesture for
inspection; it does not manufacture a Q input or dialogue interaction. Crew motion
inspection in an idle mission demonstrates idle animation, not active-heist
following. The follow behavior is separately exercised by production-controller
tests.

`--npc-audit path.json` writes the character profile/work counters, a
`path.json.state.json` sidecar from the real NPC/crew/heist systems, and
`path.json.render.json` last-frame renderer metrics/source fingerprint. Parent
output directories must exist. Motion capture requires bounded `--frames` and
cannot combine photo or mission-smoke modes. [Frame capture](FRAME_CAPTURE.md)
documents sequence naming, output safety and FFmpeg encoding. Video cadence is
**simulated time**, not a claim that the game rendered at that live frame rate.

`FURY_NPC_DETAIL=0` is a presentation A/B switch, not a replay of old gameplay bugs:
corrected locomotion, targeting and mission behavior stay active on both sides.

## Remaining limits

- The generic assets are original low-poly models; dedicated heads additionally
  use documented CC0 source geometry. The work does not provide scanned humans, mocap,
  skin subsurface scattering, physically simulated hair/cloth, facial blendshapes,
  lip-sync, finger articulation or individualized voice acting
- Foot support assumes the existing flat ground. Exact straight steady travel is
  distance-matched; turning feet are not locked to arbitrary world terrain
- Navigation is still authored waypoint/chase movement without a navmesh or new
  dynamic crowd avoidance. This work does not invent interiors or repair every
  original route's relationship to decorative props
- Some mission world-state transitions intentionally relocate existing agents;
  animation does not turn those teleports into false walking distance
- Model skinning is shared CPU work, including on GPU renderers. A faster GPU does
  not remove it, and CPU ray tracing still rebuilds changed character BLAS data
- OpenGL is validated on Mesa llvmpipe here. Windows hardware DXR and actual
  discrete-GPU timings require those platforms; no hardware performance is inferred
