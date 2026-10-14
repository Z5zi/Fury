# Shared motion and world limitations

Status: 7 October 2026, completed individual checkpoints with retained failures. These are bounded,
observed limitations, not claims that all motion is contact-free. Refer to each
character's final report for the precise source revision and failed/passed checks.

## Full crouch-walk foot and shin constraint

Hale, Renn, Metro Watch, Enforcer and Cass inspections independently exposed an extreme
pose in the existing animation: the trailing shin approaches horizontal while
its foot remains approximately level. Plausible calf/boot geometry can intersect
in that pose. Attempts to clear it with excessive ankle influence distorted the
calf and were rejected. Cass also observed jacket/thigh contact in the same
stress motion. Nell's earlier pass records a related deep-crouch limitation.

The current scope is to improve ordinary fitting and visible character quality,
keep plausible anatomy, and preserve strict failing stress assertions and exact
witnesses. A failed stress check remains failed. Ordinary-pose success must be
reported separately and cannot be described as complete contact-free animation.
Shared controller/IK redesign is deferred for a separate scope decision.

## Actual-speed upright run

Sparrow and Rook's later crew coverage uses the actual 6.5 m/s follower speed,
which is faster than the earlier diagnostic stage's height-scaled run. At phase
0.4375, the shared pelvis/leg solve can place the planted shin nearly horizontal
over a level shoe even without crouch. This is a separate ordinary-run defect,
not the extreme-crouch limitation above. The trial toe-pivot correction removes the measured contact but leaves the shin
nearly horizontal and creates up to 5.7 mm shoe float. It was rejected for
integration. The original sampler and original failure proofs remain. The rejected
experiment's separate shared archive is now unavailable after the documented
local filesystem incident; it is not claimed retained. A shared-gait redesign is deferred for a
separate scope decision. Player's actual 19.8 m/s sprint is included in later
visual review; gameplay speed is unchanged. Grounded Player has no jump input;
Space is an ascent control only while the body is hidden in fly mode.

## Existing navigation and floor placement

Guard patrol/chase movement does not avoid scene solids. Some authored anchors,
spawns and inspection cameras intersect bank walls or street furniture. Rina's
unchanged route contains obstructed anchors. Original-world images retain these
conditions; clear stage views do not certify world navigation. The separately
approved Owen, Lia and Nell narrow placement/route corrections are documented
in their reports. No general navigation rewrite is included in these art passes.

## Sub-micron cached-vertex disagreement

Cass's strict seam diagnostic measured duplicate-corner position disagreements
of approximately 1.27e-7 m Near and 1.81e-7 m Far in static crouch, due to the
existing per-corner 1e-6 update threshold and differing corner normals. This is
not a visible anatomy gap. Metro's independent reset diagnostic uses the existing
2e-6 m coordinate tolerance, while gameplay state remains exact where required.
The reproducer and strict failure are retained. No shared renderer rewrite or
legacy arithmetic contract change is being made for this low-priority artifact.

These limits are separate from art weaknesses such as cap-like hair, smooth
skin, rigid cloth and simplified hands. Passing tests does not certify AAA
quality or parity with the commercial visual references.

Player live-state review also identified crouch/transition contact after travel,
when the controller retains spread foot spacing. Upright W/Shift contacts were
corrected by a bounded cuff-only fit and verified in the final Player pass.
Of 518 live height/LOD state samples, 78 crouch/transition states retain contacts;
all 199 upright visible frames per LOD are clear under the selected contact
predicates. Ghost retains full-crouch shirt/thigh and vest/thigh limitations. A neutral stateless stage matrix alone does not establish every
live controller state. No shared sampler or gameplay speed change is included.
