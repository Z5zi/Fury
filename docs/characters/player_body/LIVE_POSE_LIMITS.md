# Exact live pose scope

The neutral-stage ordinary contact gate passes, but it is not equivalent to
all production crouch states. The final cloth correction also clears every
recorded upright production state in both LODs, including ordinary W at 9 m/s,
Shift at 19.8 m/s, acceleration and deceleration. Those are actual controller
speeds; the source animation speed is separately filtered and recorded.

The unchanged stateful sampler deliberately retains stepped foot spacing once
`gait_started` is true. It preserves phase and stride when travel stops. In
`sample_character_animation(CharacterAnimationState)`, spacing is 1 after a
gait has started; the sample-only diagnostic uses the current move weight.
Consequently, crouching after stopping can lower the pelvis between feet held
at their previous step offsets. The shin can become nearly horizontal while
the shoe remains level. A centered sample-only static crouch does not cover
that case. No change to this sampler or movement is included in the actor pass.

Iteration 24 initially retained W and sprint cuff contacts too. A bounded
isolated experiment increased only the bottom cuff/sock maximum Foot blend
from .70 to .90. It retained the calf and exact geometry, passed full ordinary
and profile tests, removed all upright live all-region contacts and looked
plausible in enlarged material pixels. That art correction is iteration 25.
The earlier ordinary failures are preserved and are not relabeled as an
unavoidable shared limit.

The remaining recorded live failure groups are stopped crouch after prior
travel, crouch-walk and the first release frame. They are preserved in
`final-live-replay/LIVE_CONTACTS.json`, with exact phase, filtered animation
speed, crouch value, region pair, source triangle IDs and shin/foot angles.
The associated actual world input/state trace remains separate from the
diagnostic stage replay. Both use the same recorded production state; the
stage's side/oblique views expose contacts hidden by the following camera.

The reported maximum segment-plane crossing is an intersection witness
metric, not a signed-distance penetration depth. Deep states can reach about
five centimeters by that metric, and are visibly strained. They are not called
subpixel, invisible or a clean live-fit pass. The stricter full crouch-walk
gate remains registered and failing with unchanged predicates and controls.
Foot/cloth contact and some shirt/raised-thigh contact are disclosed separately.

The full-state replay covers 259 visible frames per LOD, 518 states total.
All 199 visible upright frames per LOD pass every contact region. Of the
remaining 60 frames per LOD, failures occur in 18/20 stopped-crouch samples,
20/20 crouch-walk samples and 1/20 release samples. This is 78 failing states
across both LODs, with 69 containing footwear contacts; other witnesses include
the raised thigh/shirt hem. Selected Near witnesses are:

| State | Frame | Phase | Animation speed | Crouch | Shin / foot angle | Crossing metric |
|---|---:|---:|---:|---:|---:|---:|
| Stopped crouch after travel | 123 | .187804 | .00916 m/s | .932795 | 89.36° / .0029° | 49.5 mm |
| Crouch-walk | 142 | .690418 | 2.25918 m/s | .999775 | 96.83° / 1.83° | 54.9 mm |
| Early release | 155 | .698839 | 3.58052 m/s | .740814 | 86.54° / 3.88° | 48.8 mm |

Shin angle is measured from vertical; foot angle is measured from level. These
are the actual worst geometric witnesses, not a substitute for visual review.
The Near and Far native side/oblique replay images and unaltered world frames
are retained. A separate controlled eight-phase 19.8 m/s upright sample also
has zero footwear contacts in both LODs; static/crouch-walk full-state cases
retain their failures.

First-person and fly-hidden states are in the 300-frame world trace but are
excluded from the visible-mesh contact count. Far replay is diagnostic coverage:
the normal player-follow view selects Near. Grounded Space has no existing
jump presentation, and neither a jump nor a different movement speed is
invented for this review.
