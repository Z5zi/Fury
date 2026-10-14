# Lia combined integration review

This report supplements the frozen private first-pass report. The private
19:04:28–21:12:24 session remains 127 minutes 56 seconds. Later integration,
placement diagnosis, corrective review and packaging are recorded separately;
they are not retroactively counted as uninterrupted sculpting.

## Reviewed corrections

The rear Near collar had a real 3.710 mm notch. A strict ray-triangle edge test
missed the exact garment seam. Inclusive barycentric tolerance of 1e-6 reduces
the sampled dip to 0.065 mm. Seventeen Near collar vertices change; Far is
bit-identical. Triangle counts, topology, UVs, colors and effective weights
remain unchanged. The new mesh-level negative control rejects the old notch.
See `../../validation/characters/lia/collar-correction/`.

The former teller route at Z -7.4 passed through the actual shipped counter
base and worktop; it was not merely camera occlusion. The correction translates
spawn, home and all three baseline patrol anchors to Z -8.9. X/Y, route vectors,
0.35 m/s speed, DayOnly schedule, role, dialogue and ID remain unchanged. The
existing Alarm relocation and Escape behavior remain intact; PreHeist restores
the corrected baseline. Counter geometry, props and colliders are unchanged.

The persistent teller-placement regression loads all 72 production counter
parts and actual bank/mission geometry. It samples 1,801 production Near poses
over 60 seconds at 30 Hz, including ordinary talk from the legal lobby player
approach, checks generic Q target selection against the actual roster, verifies
exact translated movement/animation at 30/60 Hz, and invokes real mission state
transitions. A triangle-edge negative control deliberately restores old Z -7.4
and must intersect the actual worktop. This is finite sampled pose coverage,
not a guarantee about all possible future gestures or dynamic actors.

## Comparison design

The preserved original Owen checkpoint remains available for showing the
placement defect. For model-only comparisons, a separate clean build from exact
Owen tree 9e21e9a1aae79fb7abd7a9cf3e177349581ef14d applies only the same teller
translation. Its production source fingerprint is
b23ca745e3a49478e6b3218fb5eea370dc01932cdbd6563fc547182ef940dde9.
This gives generic and individually authored Lia the same absolute placement,
camera-relative pose, route and timing. Placement comparisons are explicitly
labeled and must not be described as unchanged absolute positions.

## Combined validation and actual rendered review

Final combined production fingerprint:
`a02f9be52e6e7b85599c22274a3c8646877ae6b673a1dd4294e0d84dc42c0189`.
All **42 distinct suites passed**: 38 non-GL suites in 196.85 seconds, plus
four actual offscreen OpenGL suites in 10.79 seconds. The corrected placement
target was rebuilt after its exact-old-literal refinement and passed in 3.72s.
The corresponding logs are retained in the validation directory. The importer
reproduced Lia's source header exactly. The shared material fixture now also
exercises every one of Lia's 8,704 source face triangles, extending the five-actor
chart/tangent/mip coverage to 43,996 triangles; its CPU and GL checks passed
in 17.33 seconds after this test-only addition. Production source was unchanged.

The exact corrected collar/actor/test source also passed the full three-height
Near/Far ASan/UBSan matrix in **254.49 seconds**, with no sanitizer diagnostics.
All six combinations cover 198 poses and 30 attention turns each. Corrected
actor/test objects were linked ahead of the unchanged fully instrumented private
engine archive, and the link map verifies its old Lia object was excluded.
Leak detection was disabled in this executor; this is not a leak certification.
This focused sanitizer run does not substitute for the combined world tests.

The actual 1,801-pose counter regression retains at least **0.117999 m** of
sampled AABB separation from 711 body-height bank/mission mesh bounds. The old
position negative control produces **1,976** worktop edge/triangle crossings.
Generic Q selection, daylight schedule, exact translated timing and actual
Alarm/Escape/PreHeist mutations pass. Pose coverage is finite; no broader
navigation or dynamic avoidance claim is made.

Inspected fresh images include eight full-body and eight facial directions,
five clay views, top/low, six animation poses, both LODs, material diagnostic,
CPU raster/path and llvmpipe, actual lobby closeups and wider populated views.
The rear collar notch is corrected; no new detached garment, gross seam hole
or joint collapse was observed. Hair still reads as a cap, skin is smooth/waxy,
facial expression is rigid, cloth has broad simple folds and hands remain generic.
The low-sample path portrait is dark/noisy; even 128 accumulated samples do not
prove presentation quality equivalent to raster. No physical GPU or Windows
DXR testing is claimed.

## Motion evidence and shared visibility defect

The final route camera is 2.6 m, orbit 0 degrees, elevation 25 degrees. All 192
before/after frames are retained at 12 fps simulation sampling, totaling 16s.
This encoded cadence is not measured live performance. The earlier 1.7 m camera
crops the head and remains preserved as a failed framing experiment.

Both generic and individualized Lia **vanish for frames 86–97**, a full second,
and reappear at frame 98. Every before/after pair in that interval is pixel
identical. Guards obscure Lia around frame 123; vehicles cover much of the body
later. The complete footage is retained without omitting these frames. It is
not evidence of uninterrupted visible movement. The final serialized NPC,
crew and mission state matches exactly at every paired audit endpoint; this
is not a per-frame state log or a new full manual heist playthrough.

A private console proof using the real roster, presentation, Near/Far mesh
bounds and production visibility function confirms that the security-desk
lighting sub-zone is reused as the whole room's visibility bound. Its expanded
X boundary is +0.420000; Lia's route reaches +0.70. Both generic and individual
models fail visibility in exactly frames 86–97, and changing only sector_hide
to false restores all 12. At individual frame 86 the combined world-X minimum
is 0.524031; at 97 it is 0.447321; at 98 the turn widens it to 0.368010, restoring
overlap. The real final position/travel exactly match the recorded world audit.
Proof source/log are preserved in `visibility-defect/`. A bounded separate
connected-room visibility repair is being prepared. This defect remains in
this explicitly frozen art checkpoint and is not attributed to Lia's new mesh.

## Serialized observed CPU cost

All other active local compilation/render/test jobs were paused for the
22:08:32–22:10:36 UTC measurement window. Each case uses three alternating pairs
of fresh processes, four frames per process, and the median last CPU frame.
Two CPU workers were used. Other cloud contention remains outside this control.
The model-only baseline already has the same approved teller translation.

| Actual view | Generic Lia | Individual Lia |
|---|---:|---:|
| 640×640 isolated portrait raster | 55.7 ms | 230.0 ms |
| 640×360 populated lobby raster | 481.8 ms | 496.5 ms |
| 640×360 populated street raster | 273.6 ms | 275.1 ms |
| 320×240 populated lobby path, 1 spp/2 bounces | 277.4 ms | 253.6 ms |

The portrait is substantially more expensive. The lobby adds 37,548 submitted
triangles (549,343→586,891) with the same 1,302 instances. The street case has
identical submitted geometry, so its small difference is timing noise. The
path case is variable and its lower median is **not an optimization claim**.
These cloud results do not establish real-time gameplay or hardware guarantees.

## Checkpoint status

This is the fifth completed individual art allocation, with combined corrective
validation and explicit remaining gaps. The private 127m56s ledger is unchanged;
subsequent integration/diagnosis/packaging is not counted as continuous sculpting.
Nell, Pax and Rina are separate ongoing passes. No public source write occurred:
The last verified public commit is unchanged in this report, and publication is
paused. The cumulative recovery package restores source and optional evidence
directly from the pinned public base without requiring earlier character archives.


## Subsequent verified visibility correction

The bounded connected-room repair is now integrated and separately verified in
[VISIBILITY_CORRECTION.md](VISIBILITY_CORRECTION.md). All 43 suites pass; fresh
actual motion keeps Lia present during every formerly missing frame. Original
art/culling footage remains preserved with its original source hash. The
corrected footage reveals an existing guard/wall navigation intersection and
retains later guard/vehicle occlusion. Earlier sections describe the frozen
pre-repair checkpoint and its controlled art-only measurements.
