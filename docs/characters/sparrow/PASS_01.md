# Sparrow individual character pass

The individual model and evidence are ready for review. Motion validation remains red at the original shared sampler’s real 6.5 m/s planted-foot contact and deep crouch-walk contact. Those failures are retained, not waived. A separate toe-pivot experiment was rejected for integration; this source uses the original sampler byte for byte.

## Identity and source

- CrewSparrow / Sparrow; CrewScout; 1.72 m; seed 0x301a7ca8; network Lookout; right/trailing offset (1.8, 0, -1.2)
- Preserved seeded palette, role, height, collision/controller/follow/mission factories and original scene
- Isolated immutable base: b7bb006f1748fbe4f0e6ce46a19a7631a6166e0a
- Final program-source fingerprint after ankle-liner correction: 113c6d9eef2440adf67cdefa56a7a7a7f2144e774995735f0f6adeeb5301ab7c
- Exact owned files, hashes and narrow shared deltas: [SOURCE_FREEZE.json](SOURCE_FREEZE.json), [integration patches](integration/)
- Dedicated individual art/active QA: 121 minutes; real UTC intervals and excluded shared work/packaging are in [WORK_LOG.md](WORK_LOG.md)
- Publication remains paused. No main checkout, GitHub or user-computer edits were made by this worker

## Model changes

| | Original | Final | Budget |
|---|---:|---:|---:|
| Near triangles | 2,280 | 39,684 | 40,000 |
| Far triangles | 1,084 | 15,914 | 16,000 |
| Near vertices | — | 25,620 | — |
| Far vertices | — | 10,182 | — |

Sparrow has an independently shaped narrow oval face, pointed chin, compact nose and slimmer neck, source-fitted eyes, connected hands with a restrained static curl, a complete short crop, sewn sleeve roots and turned cuffs, a cropped teal jacket with a fitted zipper slider/pull, and a rounded compact pack with padded contact face, piping, pocket, zipper, carry handle and fitted straps. Ordinary sleeve/hand breakthrough, low shoe/ankle fitting and waistband protrusion were corrected on the individual asset. The calf and controller were not distorted to accommodate the sampler failure.

The source is pinned CC0 graphical data transformed by Fury-owned importers. No addon/GPL program code is imported or executed. [Anatomy provenance](ANATOMY_PROVENANCE.md) and [reference criteria](REFERENCE_CRITERIA.md) explain the source pin and original recipe. Proprietary reference pixels are not included.

## Independent hem review and correction

The coordinator's independent review found dark fabric triangles emerging through the bind-pose trouser hem. The 803484 source, original front/side pixels and newly exposed failed triangle pairs are preserved in [hem-correction](../../validation/characters/sparrow/hem-correction/). Near Shirt triangle 14260 crossed Pants 11809; Far 3261 crossed 2745. An initial center-only correction 310a0c cleared the pants but left a real heel/outer-upper crossing, also preserved.

The final correction fits only the hidden dark ankle fabric inside the actual shoe opening and trouser axis. All four directions were inspected in both LODs: final-feet-{near,far}-{0,90,180,-90}.png. The calf, shoe geometry, indices, rig and sampler remain unchanged. Exactly 294 Near  / 104 Far bind vertices belong to this changed fabric; every other bind and posed position and every bone matrix matches 803484. The new independent check tests actual ankle fabric against trousers and the visible outer shoe upper, with a real-triangle negative control. Internal flat closure caps under the leg/sole are not exterior fitting surfaces; warm laces are a separate sewn attachment, not ankle fabric. Existing contact thresholds and motion assertions remain unchanged.

Only engine/src/character_sparrow.cpp and tests/character_sparrow_contact_tests.cpp differ from the first integrated source. Final hashes and the small patch from the coordinator's 310a0c version are in final-correction-manifest.json and final-hem-fix-from-coordinator.patch.

## Visual evidence and comparison

[Complete pre-correction neutral stage](../../validation/characters/sparrow/final-stage/) was captured at 803484 and contains 64 matched all-side material/clay body/head views, eight directions in each LOD, plus 62 pose/attention/top/low/detail views. [Exact original stage](../../validation/characters/sparrow/baseline/stage/) retains the same cameras and original accepted executable. [Close details](../../validation/characters/sparrow/final-details/) include four attempted directions for each LOD hand (the -90° views are camera-obstructed and are not clear hand evidence), shoes from front/side/rear, and actual 16-sample, four-bounce CPU path-traced head frames. The stage, hand/head details, original motion witnesses and matched world pair retain 803484 provenance; final corrected foot views carry 113c6d9e provenance. The measured geometry delta proves the other geometry unchanged. These are engine pixels, not composited art.

The official GTA VI Lucia 02/05 and Jason 02 pixels were inspected against facial planes, eye/lid depth, finger separation, shoulder construction and garment tension. Sparrow’s face, ears, neck, fingers and garment construction improve substantially over the original primitive. The reference skin response, dense hair strands, cloth microstructure and pose-dependent folds remain substantially richer. Sparrow’s short crop still reads partly as a cap, the face is static, fingers have no independent animation and clothing is not simulated. There is no GTA VI parity or AAA claim.

Rook’s final front/profile clay was compared separately: Rook retains a broader lower face/neck and flatter brow-to-bridge transition; Sparrow’s narrower oval jaw, sharper chin and nose/jaw profile remain distinct under neutral material. Patagonia pack references informed generic load transfer and gusset construction without copying branding or patterns.

## Real production gameplay proof

[Matched world pair](../../validation/characters/sparrow/world-pair/) drives the actual production E-equivalent interaction, crouched breach input, ordinary timers, active crew following, four heading offsets, loot, escape, successful extraction and reset. It records 1,216 ticks and 13 world frames for each model. Every paired frame has exactly equal recorded production state and camera metadata. This uses explicitly positioned diagnostic player/review cameras; it is not a navigated UI playthrough and does not use scripted `--heist-capture` state.

The untouched accepted executables are preserved separately. The matched model-only baseline adds the identical opt-in crew review harness to the exact original character-profile dispatcher; it keeps the other accepted actors. Current parallel actor studies are not silently included.

The review refresh uses live production animation without advancing it. Idle arms return after reset. The original sampler deliberately retains a split stance once gait_started is true; this is distinct from the earlier stale whole-mesh capture. Original counter/prop occlusion is retained. The original actor ground is y=0 while sidewalk planes are y=.03/.04, so shoes can be partly hidden by that existing mismatch. No actor, scene or camera change was used to conceal a before/after fitting difference.

## Validation

[Final optimized checks](../../validation/characters/sparrow/checks-final-optimized/) passed 13 of 14 selected regressions: character profile, Jon, Tessa, Owen, Lia, original animation, NPC gameplay/presentation, Sparrow anatomy/profile, software/CPU-ray rendering, actual OpenGL and both source importers. The ordinary contact gate fails at real 6.5 m/s, phase 14/32 for heights 1.35 and 1.72 in both LODs; it passes 2.10. Its preceding idle/walk/normal-run/talk/static-crouch scopes pass. The separately executed default strict contact scope also retains the deep crouch-walk failure. Twelve injected real-triangle negative controls remain enabled.

[Final ASan/UBSan checks](../../validation/characters/sparrow/checks-asan-hem-final/) instrument the full engine and affected actor tests. The final 113c6d9e run completed; earlier 803484 and 310a0c results are retained separately. Profile, all three renderer paths, NPC presentation and gameplay pass. Both contact scopes reach their expected geometric assertion failures, with no sanitizer diagnostic. LSan was attempted separately and failed because this environment uses ptrace; leak coverage is not claimed. ASAN_OPTIONS=detect_leaks=0 applies only to the disclosed address/undefined-behavior runs.

[Motion failures](../../validation/characters/sparrow/final-motion-failures/) preserve actual-speed and deep-crouch images, full bind/posed vertex positions, triangle indices, bone matrices and sampler fields. The exact automatic-stride input record is kept separately from the stage’s equivalent computed stride, with equality checked. Existing strict assertions remain the default; no timeout or acceptance threshold was increased to hide a failure.

## Measured CPU cost

One pinned CPU, optimized build. [Final-source measurements](../../validation/characters/sparrow/performance-final/actor-costs.log): cold Near creation including the atlas 718 ms; warm-atlas creation median 256 ms Near / 25.8 ms Far; skinning median 0.848 ms Near / 0.346 ms Far across 360 changed poses. Sampler median is about 0.0027 ms.

The actual 160×128 direct CPU-ray fixture takes median 29.3 ms for a changed Near mesh and 17.0 ms for a held one; Far takes 20.4 / 15.8 ms. Each changed frame rebuilds one character BLAS after startup, while held frames reuse it. These are diagnostic readback measurements, not game FPS.

The 13 matched 640×480 populated software-rendered world snapshots at 803484, before the isolated ankle-fabric correction, contain 456–1,545 instances and 443,041–987,475 submitted triangles after the change. Recorded mean frame cost is 424.5 ms after versus 414.5 ms before; medians 407.7 / 404.2 ms. They are single frozen snapshots under shared-machine load, not a controlled steady-state performance guarantee.

## Evidence availability

All current source, exact accepted baseline binaries, original-sampler failures, 803484 hem failure, 310a0c heel failure and final 113c6d9e correction evidence remain in this private study. The separately rejected toe-pivot experiment's shared directory disappeared for an unknown reason; no separate copy of its original images/matrix archive survives here. Its reported trial result is historical, and that experiment archive is unavailable. The current original-sampler failure proofs are retained and independently reproducible.
