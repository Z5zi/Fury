# Nell route and fixture correction

This is a separately approved scene/gameplay correction, not character-art proof.
The original authored route crossed ten solid entities and additional non-solid
visual obstacles. Only the Ashcourt MidCrate/MidTrash pair moved by (-1,0,-2.2).
The nearby hydrant, all other props and building/collider geometry stay fixed.
Nell's home/spawn, ID, height, 0.4 m radius, 1.9 m/s speed, schedule, dialogue
and mission identity stay fixed. The route becomes a reviewed sixteen-point
static detour; four obstructed intermediate anchors move. At 60 Hz the steady
lap changes from 49.87 to 68.17 seconds, **+36.7%**, without increasing speed.

The current integrated world gate checks the full wrapped route/spawn against
329 early-world solids and retains the approved storage/player-access checks.
An independent frozen-model audit checks all 343 startup solids plus visual
bounds over 50,208 ordinary posed samples across Near/Far and update cadences.
The actual model remains byte-identical after construction optimization.
[Detailed posed result](../../validation/characters/nell/route/posed-clearance/RESULTS.md),
[route regressions](../../validation/characters/nell/route/route-regression.json)
and [timing](../../validation/characters/nell/route/timing.json) retain scope.
These bounded static checks do not add dynamic traffic/actor avoidance or fix
the inherited paving/foot-height mismatch. Deep-crouch trouser self-contact is
a distinct art limitation disclosed in PASS_01.md.

[Fixture-only world comparison](../../images/characters/nell/fixture-game-comparison.png)
and [overhead comparison](../../images/characters/nell/fixture-overhead-comparison.png)
show the scene correction independently. The current model-only CPU pair keeps
this same corrected world and route in both variants, with exact gameplay-state
sidecar equality. Later guard and Rina navigation defects remain unchanged.
