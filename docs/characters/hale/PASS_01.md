# Sgt. Hale: individual prototype-art pass

Status: frozen ordinary-fit checkpoint with final private validation and evidence curation complete. Cumulative integration validation is complete: 59/60 registered suites pass, with only this retained strict failure; all 64 Hale/Vale stage pairs and eight world-state pairs are verified. The strict registered full-crouch-walk test remains failed and is reported separately; this is not a globally passing test or release certificate.

## Result

Hale keeps the original `NpcGuard` / Security identity, seed `6ef263f5`, 1.85 m nominal height, uniform/skin/hair/eye/shoe palette, cap, badge, radio and hip pouch. A dedicated 17-bone bind fitting retains the original hierarchy and all animation/controller interfaces. The graphical import uses only the pinned CC0 MPFB data and Hale's original fourteen-target recipe. Original garment, equipment, hair and fitting methods are first-party work; no GTA or portfolio asset is included, and no AAA parity is claimed.

The final mesh has **39,792 Near triangles / 25,509 runtime vertices** and **15,986 Far triangles / 10,403 vertices**, under the 40,000/16,000 caps. Both LODs retain all 8,720 selected source head triangles and original corner UV correspondence. Near hands retain 3,276 source triangles per side; Far retains the documented 520-triangle endpoint reduction with the original wrist boundary locked.

The closed vest has supported shoulder bridges, distinct neck/hem/armholes, side webbing closures and complete back/lining/cut edges. The shirt is one sewn torso/yoke/sleeve pattern, with cuff returns and supported folded collar leaves. Trousers, ankle underlay and shaped footwear remain connected through the existing animator. The cap has a fitted crown, band, short layered peak and opaque scalp beneath it.

## Dedicated work accounting

**121 minutes 26 seconds are credited**, conservatively. This is 94m26s before the interruption plus 27m00s after resumption. See [the full ledger](WORK_LOG.md) for exact intervals.

Excluded: the 00:09–00:17:04 registry-reset gap; 43 seconds packaging; 10 seconds recovery packaging; 00:53–01:00 unverified time; all 01:04–02:10, including uncertain sanitizer/passive time and the entire later outage; 02:12–02:13 and 02:32–02:33 recovery/tooling blocks. Everything after 02:39 is uncredited, including later useful review, benchmark coordination, verification, waiting and packaging. Session UTC is used because filesystem/host timestamps run one hour ahead. The elapsed-time floor did not override failed art or numerical checks.

## Corrections caught by review

- A sleeve-compression variant passed numerical checks but had no clear benefit in matched Near/Far images. It was rejected and the previous sleeve source restored
- Extending the contact grid from eight phase samples per motion to all 33 found eight previously missed Near vest/shirt crossings at run phase 2/32. A first small allowance still failed. The accepted local rear-underarm ease removes them without changing topology, source anatomy, palette or the rig
- The original radio center was offset from the vest, but an edge of its axis-aligned case intersected the curved support. A first fitted frame still failed crouch-walk. The accepted radio uses the actual support's orthonormal frame and a closed narrow rear clip; one redundant Far case row funds that clip
- The extra 35 attention samples previously checked seams only. They now receive the same finite, normal, area, sole, source-UV/color, allocation and storage-identity gates as the 198 motion samples

All failed logs, rejected source variants and earlier captures remain in the study. No proper-intersection epsilon, source-retention predicate, topology rule or cap was relaxed to obtain the result.

## Evidence to inspect

The [pixel review](PIXEL_REVIEW.md) records actual observations and limitations. The [geometry review](GEOMETRY_REVIEW.md) explains construction and the exact scope of checks.

- [Original/current body comparison](../../images/characters/hale/body-comparison.png)
- [Original/current face comparison](../../images/characters/hale/face-comparison.png)
- [All eight Near body views](../../images/characters/hale/all-side-near-body.png) and [head views](../../images/characters/hale/all-side-near-face.png)
- [All eight Far bodies](../../images/characters/hale/all-side-far-body.png) and [heads](../../images/characters/hale/all-side-far-face.png)
- [Near clay bodies](../../images/characters/hale/all-side-clay-body.png) and [heads](../../images/characters/hale/all-side-clay-face.png)
- [Near/Far side-by-side](../../images/characters/hale/lods.png), [Near pose grid](../../images/characters/hale/poses-near.png) and [Far pose grid](../../images/characters/hale/poses-far.png)
- [Renderer comparison](../../images/characters/hale/backends.png), [attention/elevation](../../images/characters/hale/attention-and-elevation.png), [fitted radio](../../images/characters/hale/radio-mount-final.png)
- [Original-spawn day/finale-night](../../images/characters/hale/original-spawn-day-night.png), [body/real occlusion](../../images/characters/hale/world-body-and-occlusion.png), [world ray views](../../images/characters/hale/world-ray.png)
- [Complete Near rear-run](../../videos/characters/hale/near-rear-run.mp4), [Far rear-run](../../videos/characters/hale/far-rear-run.mp4), [failed Near crouch-walk stress](../../videos/characters/hale/near-crouch-walk-stress-failed.mp4), [failed Far crouch-walk stress](../../videos/characters/hale/far-crouch-walk-stress-failed.mp4)
- Matched complete real-world [original guard](../../videos/characters/hale/world-original-guard.mp4) and [individual Hale](../../videos/characters/hale/world-individual-hale.mp4) clips, including their later obstruction failures

Video frames were generated by the actual production engine at a 12 fps simulated cadence and encoded offline. No interpolation or synthetic frames are used. They do not claim 12 fps real-time rendering. Contact sheets are labeled resizes of actual images; full final stills are also saved as lossless PNG with decoded RGB equality checks and raw PPM hashes.

## Numerical validation

Sixteen nonempty contact pairs cover vest/shirt, collar/shirt, badge/vest, visible neck/shirt, hands/pouch, opposite knees, hands/shirt, hands/vest, hands/trousers, closure/vest, closure/shirt, hands/closure, hands/footwear, radio/vest, hands/radio and trousers/footwear. All sixteen pass **bind plus 167 idle/walk/run/talk/attention samples × 3 heights × 2 LODs**. The heights are 1.35, 1.85 and 2.10 m. Stationary crouch also passes in the separate full run.

**The registered `character_hale` test remains FAILED.** Full crouch-walk retains 122 Near / 42 Far proper trouser/footwear triangle-pair crossings at phase 6/32, at every stress height. Worst exact witnesses are 14702/16224 Near and 3010/3259 Far. Its assertions and registration remain strict. `character_hale_ordinary` / `--ordinary` is the separately passing scope, not a waiver or renamed full pass.

The source head/hands, original palette, exact sole/height bounds, finite unit normals, nondegenerate/winding checks, atlas boundaries, scalp coverage, eye seating, sewn corners, allocation-free poses and stable storage pass. These geometric and actual cuff/forearm coverage checks still exercise **all 233 samples** in each height/LOD combination. Cuff coverage uses actual welded boundary loops and rejects a shortened-forearm negative control; nominal overlap is 9.248/9.249 mm with 2.288/2.155 mm radial containment Near/Far. The complete shirt selector includes the actual cuffs, not the former lower-sleeve slice.

Independent negative controls reject a corrupted source position, corrupted UV, buried eyes, missing opaque scalp triangle, zero normal and injected vest penetration. The vest is a closed shell with Euler −4; shirt has four boundary loops; both source hands have one 20-edge cuff; collars and side closures are closed; trousers have three boundary loops. Fresh posed sewn corners retain 0.1 µm equality. The documented incremental pose-cache bound is checked separately; it is not a relaxed garment-contact tolerance.

The final ASan/UBSan command uses explicit `--ordinary` at both LODs and all three heights. It does not claim full-stress contact success. In the separately failing default run, each contact check stops at its strict trouser/footwear assertion before that function's final injected vest-penetration control; the ordinary run reaches and passes that injection. All source/UV/eye/scalp/normal negative controls and the cuff-shortening control are retained.

These are finite sampled tests, not a proof for continuous time, arbitrary future animations, full body self-collision, cloth simulation, navigation or world collision. Dedicated test logs, selected regressions, source generation checks, exact-source sanitizer results, image inputs and full source/binary hashes are delivered under `docs/validation/characters/hale`.

Final renderer/program source fingerprint: `81ccaa3e2cc3c614a5e8a6611681698b583e9942bbb7722ce380e9a363f07589`. Test files have their own hashes because the renderer fingerprint deliberately excludes them. The final source manifest binds both.

## Cost

The frozen source's five-process **shared-load observation** gives medians of **1.092 ms/pose Near and 0.449 ms Far**, with process-median ranges 1.062–1.265 / 0.420–0.460 ms. Total wall/user/system time is 7.301/7.199/0.097 seconds, pinned to CPU 1. Other workers were not paused, so this is not an isolated comparison. The earlier coordinated quiet result (1.078/0.453 ms) is retained only as historical candidate-16 evidence. The [full performance report](PERFORMANCE.md) separates those sources and gives construction costs, actual logical/reserved vector storage and exclusions. Pose cost remains substantially above the tiny original generic guard; neither result is whole-frame, GPU or real-time evidence.

## Preserved limitations and integration boundary

The existing chase ignores solids; other guards may overlap Hale. At original spawn the unchanged raised forecourt hides 7.6 cm of both old and new shoes. The real bench/post and wall occlusions are retained. Live diagnostic cameras trigger real investigation/chase and later enter scenery; those failures remain in the full clips. Five matched final-world end-state records are byte-identical to their baseline runs, which does not establish identical traffic/crowd pixels or sound navigation.

The ordinary-time night exploratory capture ends far from the bank and overlaps Renn. The final original-spawn night evidence uses the existing mission-4 finale cue in an isolated diagnostic save fixture; it is labeled accordingly, not presented as ordinary-time progression or a change to user saves.

Skin, cloth, hair and facial motion remain prototype-level. There is no subsurface scattering, commercial facial texture set, expression/lip-sync rig, finger animation, anisotropic groom or cloth simulation. Offscreen OpenGL is Mesa llvmpipe with its logged disabled directional shadows/reflections. Direct ray/path shading is visibly weaker on the face. No hardware-GPU, DXR or AAA parity is established.

This study never modifies main. [Integration instructions](INTEGRATION.md) separate Hale-owned files from the small shared dispatch/CMake/test changes and the independently reusable diagnostic controls. Preserve every later character and world correction on main. After surgical serial integration, reconfigure, rerun affected combined tests and recapture the actual combined-world guard; the isolated evidence is not a certificate of the combined roster. No GitHub write, publication, merge or release was performed.
