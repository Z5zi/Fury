# Sgt. Hale geometry and rendered review

Stable identity: `NpcGuard`, Sgt. Hale, Security, seed `6ef263f5`, nominal height 1.85 m. The individually fitted 17-bone bind rig keeps the existing hierarchy, animator and controller interfaces. Palette and gameplay heights are unchanged. Generated facial anatomy uses Hale's own fourteen-target recipe; no biography follows from target filenames.

## Construction

- Complete CC0 head/neck selection: 8,720 source triangles, 4,390 source positions, 4,584 original corner UVs, five retained face charts and a 58-edge attachment boundary. Both LODs retain the complete anatomical face
- A hidden skin continuation sews the head boundary into the shirt; fitted sockets, iris geometry, eyelid edge accents and source-queried eyebrows sit on the individual face
- A continuous opaque scalp, short masked edge cards and restrained clipper taper sit under the original blue security cap. The crown, woven outer band, short curved peak, closed peak edge and six Near panel seams are original construction. There is no simulated groom or fully modeled inner sweatband
- The shirt is one sewn pattern with continuous sleeves/yoke and four openings. Cuff returns, tucked waist and folded collar leaves have real thickness and support-derived weights. The collar's inner yoke follows the lower neck sufficiently to clear maximum attention turns
- The protective vest has outer/lining surfaces and closed cut edges, two connected shoulder bridges, separate neck/hem/armholes and two support-fitted side webbing tabs. Pockets/badge follow the actual garment support; radio and hip pouch retain the original placement language
- Complete connected source hands are independently fitted and mirrored, retaining UV corners and a static relaxed curl. The hand-to-wrist seam uses the original 20-edge source boundary. There is no finger animation
- Connected trousers have one waist and two ankle openings. The shaped leather footwear includes a separate rubber sole, fitted instep panel, Near laces and a weighted ankle underlay

## Final prototype costs

| LOD | Runtime vertices | Triangles | Source head triangles | Hand triangles per side |
| --- | ---: | ---: | ---: | ---: |
| Near | 25,509 | 39,792 | 8,720 | 3,276 |
| Far | 10,403 | 15,986 | 8,720 | 520 |

The Far reduction is in hands, cloth support, cap, folds and small equipment details. It is not a low-quality replacement face. The profile keeps the 12 m geometry-distance LOD threshold; this is nearest render-bound distance, not simply camera-to-root distance. The material atlas is shared and immutable, not duplicated per Hale instance. A separate measured metrics file records actual storage and pose cost; these counts alone do not establish performance.

## Numerical evidence and limits

The dedicated test verifies finite unit normals, nondegenerate outward bind geometry, triangle-local atlas regions, normalized weights, exact height/sole bounds, exact source head/hand position and UV correspondence, opaque scalp coverage and eye seating against the actual socket rim. It checks Near/Far at 1.35, 1.85 and 2.10 m.

The nominal-height physical-surface checks establish:

- Protective vest: one complete closed shell, Euler −4, preserving four independent garment openings through its thickness
- Shirt: one connected surface with exactly four boundary loops, Euler −2
- Each hand and wrist: one connected surface, exactly one 20-edge cuff boundary
- Collar: two complete closed leaf shells
- Trousers: one connected surface with exactly three boundary loops
- Vest side tabs: two complete closed shells, total Euler 4

Each of six height/LOD combinations receives 233 pose/attention samples. They preserve source UV/color, finite unit normals, mesh/storage identity and allocation-free pose updates. Freshly forced production poses retain sewn corners within 0.1 µm. The production engine intentionally caches changes of at most 1 µm per component; one hard-edge corner pair at height 1.35 m exhibited a 1.14683 µm Euclidean incremental gap. That is logged separately against the two-corner cache bound, rather than confused with a millimetre-scale garment tear. The 1.85 m and 2.10 m samples had zero measured cached gap.

Sixteen proper triangle-crossing pairs cover vest/shirt, collar/shirt, badge/vest, visible neck/shirt, hands/pouch, opposite knees, hands/shirt, hands/vest, hands/trousers, closure/vest, closure/shirt, hands/closure, hands/footwear, radio/vest and hands/radio and trousers/footwear. The ordinary contact subset passes bind plus 167 idle/walk/run/talk/attention samples per height/LOD. The full 233-sample suite retains a failed full-crouch-walk trouser/footwear assertion; see the bounded limitation below. These are finite sampled model contacts. They do not certify arbitrary future animations, full body self-collision, navigation, physical cloth or collisions with scenery.

Independent negative controls corrupt a retained source position and UV, bury the eyes, remove a scalp triangle, zero a normal and force a vest penetration. They must fail the same positive predicates. No failing cloth penetration was waived.

## Actual-pixel observations

The eight fixed Near body and eight head views show continuous anatomy, both ears, the nape, complete rear vest, supported shoulder bridges, side closures, sewn cuffs, separate fingers, trouser rise and footwear. The head set uses the same 1.40 m distance for every angle and the matched original, avoiding the earlier profile crop. World-axis angles, light, aspect and pose remain fixed.

The quarter/profile head views give Hale a separate chin, nose projection and cheek/ear structure. The skin is still smooth and the neutral face is visibly rigid. The hairline is softer than the early solid triangular cutoff, although the small crop still reads as a simple mass under the cap. Cloth uses broad shaped folds and the shared procedural atlas; it does not have commercial garment sculpting, cloth simulation or photographic texture variation.

Far renders preserve the face and uniform identity but expose coarser cap/shoulder silhouettes and reduced hand detail at close diagnostic magnification. Those pictures intentionally show the real Far mesh at the same camera as Near, rather than hiding its losses with a distant screenshot.

Actual direct-ray images show the cap shadow, raised collar leaves, radio depth and pocket edges. Facial shape reads less clearly under the prototype renderer's brim shadow than under raster lighting. A separately labeled 32-sample, four-bounce path stage retains grain. OpenGL evidence is offscreen Mesa/llvmpipe with its logged feature limits; none is hardware-GPU, Windows DXR, real-time or GTA VI parity evidence.

## World-placement limits preserved

The real bank bench, bollards, posts and walls remain present. The entrance view makes the guard visible from the side; the straight rear view is blocked by a wall. At the original spawn, the new and old shoes both meet the y=0 animation ground beneath the existing visual forecourt at y=0.076 m, hiding their bottoms. Hale was not lifted to disguise this existing mismatch.

The live capture camera is the actual player camera. It can trigger normal guard investigation/chase; the original following orbit enters scenery. The retained street-side comparison uses the same new camera settings for old and new models and retains the real responses. End states agree, but that is not a navigation fix. The existing chase follows straight lines through solids, guards can overlap one another, and prolonged following capture can lead them far outside the authored bank route. Those are unchanged shared-world limitations.

Ordinary-time night captures and a separately labeled isolated finale-night save fixture are both retained. The latter uses the existing mission-4 lighting cue at the original spawn, without an edited light, actor relocation or a change to real user saves. Crouch/crouch-walk are production-animator stress poses on the diagnostic stage; this report does not claim Hale's AI naturally crouches or that a presentation talk sample is a Q/dialogue interaction.

The Far all-side review also shows a slightly angular visible shirt/waist strip under the vest and simpler footwear without Near lace geometry. This is preserved in the close diagnostic evidence, not described as visual identity loss. Near/Far head topology remains identical while the cap surface and secondary eye geometry differ in detail.


## Dense phase correction after recovery

The initial candidate-12 contact pass sampled eight phases per motion and 35 attention combinations (83 total). A resumed complete 33-phase motion sweep found eight Near vest/shirt crossings at run phase 2/32. A first 0.0015-height-unit local ease reduced but did not remove them and was rejected. The final Near rear-underarm ease is a bounded 0.004-height-unit field, centered at |x|=0.073, y=0.207 and fading out toward the front; it moves the actual supported vest/lining and attached details. Far, shirt, cap, anatomy, rig, source palette and topology remain unchanged. Both failed runs are preserved.

The full contact grid now matches the 233-sample geometric grid at every height/LOD: 198 motion samples including the period endpoint and 35 attention combinations. All sixteen pairs remain nonempty. Ordinary poses pass the original proper-intersection predicate with unchanged epsilon; the full crouch-walk failure remains strict and explicitly reported. Attention samples now also receive the full allocation, finite-normal/position, source UV/color, storage identity, sole, seam and triangle-area checks rather than seams alone. These denser tests still do not prove collision freedom for continuous time or other future animations.


The resumed equipment review separately found a rigid radio-case edge buried in the curved vest in both LODs. Merely fitting its center depth was insufficient. Final construction fits an orthonormal mount frame to actual support triangles and provides a closed narrow rear clip. A first shallower fitted frame still crossed in crouch-walk and was rejected. That intermediate fifteen-pair matrix added radio/vest and hands/radio. One redundant Far radio row funds the clip; no source anatomy or uniform identity is removed to meet the caps.


## Final cuff/ankle scope and retained strict failure

The complete shirt selector now includes its actual cuffs rather than stopping at a lower-sleeve height slice. Actual welded shirt and forearm boundary loops are checked through all 233 posed samples. The forearm terminus must continue inside the posed cuff plane and remain contained within its contour. A shortened-forearm negative control is rejected. Nominal minimum overlap is 9.248 mm Near / 9.249 mm Far, with minimum radial containment 2.288 / 2.155 mm. No production wrist anatomy was changed.

The final ordinary ankle fit uses a modest cuff/upper fit and a shared local ankle weight field for trouser end, footwear upper and underlay. Laces and instep panel inherit their actual supporting upper triangle's weight. Far redistributes its existing longitudinal rows through the bend rather than adding triangles or narrowing the body. Source anatomy, body height, palette, joint hierarchy, animator and controller remain unchanged.

Bind, idle, walk, run, talk, attention and stationary crouch are clear in both LODs at all three heights. The full crouch-walk stress still has 122 Near / 42 Far trouser/footwear crossings at phase 6/32. Worst witnesses are triangles 14702/16224 Near and 3010/3259 Far, consistently at each stress height. The registered `character_hale` test remains failed; `character_hale_ordinary` is the separate ordinary-scope gate. No threshold, assertion, test registration or anatomy was altered to call that stress pose passing. This remains a bounded model/animator limitation; the ordinary guard AI does not acquire crouch-walking from the diagnostic fixture.
