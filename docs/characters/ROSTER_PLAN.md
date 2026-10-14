# Individual character roster plan

Source audit: 6 October 2026, UTC, branch `feat/cpu-rendering-audio-upgrade`.
This document plans individual character work. It does not certify finished art.

**There are 18 stable humanoid actor IDs.** As of 7 October 2026, 09:34 UTC,
all 18 individual art/active-QA allocations have reached at least two hours,
and all 18 models are integrated and reviewed at the disclosed scope. The
[dated roster status](ROSTER_STATUS_20261007.md) separates the time allocations,
source integration and actual validation results.

The final cumulative Release suite has **99/111 passes, 12 retained geometric
failures and zero skips**. Fresh ASan/UBSan has 16 passing scopes and three
retained strict contact assertions without sanitizer diagnostics. The
[final evidence](../validation/characters/integrated-eighteen-20261007/README.md)
records exact source fingerprints, all-side comparisons, production input,
real spawn/pursuit and received-network checks. These are development
checkpoints with known visible limitations, not a release or AAA parity claim.

The separately authorized Enforcer spawn-order repair and Ghost absent-peer
hide/reset repair are documented separately from model-only comparisons.
Shared gait/navigation redesign remains deferred. [Shared limitations](SHARED_MOTION_LIMITS.md)
and the local raw-archive retention limits remain explicit.

An executor interruption paused work after 01:13 UTC; resumed passes explicitly
exclude the entire uncertain outage, recovery and other uncredited intervals.
Their original start times do not by themselves establish completed art time.

The dated reports and work logs establish dedicated time: [Mira](mira/PASS_01.md),
[Jon](jon/PASS_01.md), [Tessa](tessa/PASS_01.md), [Owen](owen/PASS_01.md),
[Lia](lia/PASS_01.md), [Nell](nell/PASS_01.md), [Pax](pax/PASS_01.md) and
[Rina](rina/PASS_01.md).
Mira's publication interruption, later recovery/packaging, shared tooling, and
uncertain reset gaps are excluded as recorded. Renn began 7 October at
00:21:22 UTC and Ofc. Vale at 00:24:42 UTC. Separate workers do not reuse one
worker's time across actors. Two hours is a minimum work allocation, not an art
quality verdict, and there is no GTA VI parity or finished-AAA claim.

Lia's approved counter placement and bank visibility correction are retained.
Nell's separately approved fixture/route repair changes the steady loop from
49.87 to 68.17 seconds (+36.7%); it is not an art-only change. Rina's inherited
obstructed route and the guards' scene-ignorant path/chase behavior remain
unchanged and disclosed. Broad navigation work is outside these art passes.

## Canonical roster and source of truth

The authoritative sources are the [regular/crew spawn factories](../../apps/vaultline/npc_roster.cpp),
[presentation role mapping](../../apps/vaultline/npc_presentation.cpp), and
[player, network, and complication construction](../../apps/vaultline/main.cpp).
The [runtime roster test](../../tests/npc_runtime_tests.py) independently lists
the same population. Preserve exact case-sensitive IDs, display names, gameplay
heights, transforms, routes, schedules, controller behavior, and interaction
roles when improving art.

| Stable actor ID | Existing name or presentation label | Height | Appearance role | Gameplay identity and availability |
| --- | --- | ---: | --- | --- |
| `NpcCivA` | Mira Vale | 1.75 m | `Commuter` | Civilian; central Harbor route; `Always`, including sparse night population |
| `NpcCivB` | Jon Keel | 1.70 m | `Market` | Civilian; eastern central-Harbor loop at X 18–34; `DayOnly` |
| `NpcCivC` | Tessa Quill | 1.65 m | `Commuter` | Civilian; Ridge Pier/east-bridge route at X 70–102; `DayOnly` |
| `NpcCivAsh` | Nell Ash | 1.75 m | `Market` | Ashcourt civilian, near the market/shop; `DayOnly` |
| `NpcCivD` | Pax Wren | 1.72 m | `Dock` | Civilian with work-vest wardrobe; central route at X −6–8, Z 14–28; `DayOnly` |
| `NpcCivE` | Rina Holt | 1.68 m | `Dock` | Civilian with work-vest wardrobe; eastern route at X 48–62, Z −8–6; `DayOnly` |
| `NpcTeller` | Lia Merrow | 1.68 m | `BankStaff` | Meridian Mutual teller; short counter route; civilian dialogue; `DayOnly` |
| `NpcBankCust` | Owen Pike | 1.74 m | `Commuter` | Meridian Mutual lobby customer; short lobby route; `DayOnly` |
| `NpcGuard` | Sgt. Hale | 1.85 m | `Security` | Bank guard; `NightTighten`; patrol 1.6, chase 3.5 m/s |
| `NpcDeskGuard` | Ofc. Renn | 1.82 m | `Security` | Meridian Mutual desk/interior guard; `Always`; patrol 1.15, chase 3.4 m/s |
| `NpcAlleyHmpd` | Ofc. Vale | 1.86 m | `Security` | Alley HMPD guard; `NightTighten`; patrol 1.4, chase 3.8 m/s |
| `NpcFence` | Cass Vesper | 1.78 m | `Fence` | Ashcourt fence broker by the shop; fence dialogue; `DayOnly` shop hours |
| `CrewRook` | Rook | 1.70 m | `CrewTech` | Heist follower; left/trailing offset (−1.8, 0, −1.4); network role `Muscle` |
| `CrewSparrow` | Sparrow | 1.72 m | `CrewScout` | Heist follower; right/trailing offset (1.8, 0, −1.2); network role `Lookout` |
| `NpcExtraGuard` | Metro Watch | 1.82 m | `Security` | Complication reinforcement near the selected vault; starts chasing; patrol 1.8, chase 4.0 m/s |
| `NpcEnforcer` | Syndicate Enforcer | 1.95 m | `Enforcer` | Rare complication adversary near the selected vault; starts chasing; patrol 2.4, chase 5.6 m/s |
| `PlayerBody` | Third-person player; no authored personal name | 1.75 m | `Player` | Existing optional player body; V toggle, hidden in first-person/fly-camera modes |
| `GhostLoop` | Network avatar; no authored personal name | 1.80 m | `Ghost` | Existing remote-player proxy, driven by network state; cyan identification vest |

The total is **12 regular NPCs + 2 crew + 2 player/network presentation actors +
2 complication actors = 18**. Normal startup registers 16 presentation actors;
that does not mean all 16 are simultaneously visible or on duty. `ReplayGhost`
trail boxes are markers and do not create additional humanoid passes. The two
complication IDs, optional player, and network proxy must not disappear from
the work queue because they are absent from an ordinary street view.

### What the story actually establishes

The [dialogue tables](../../engine/include/fury/dialogue.hpp) give civilians and
guards rotating role-level lines, not individual biographies. Cass's role speaks
about fencing named loot, shop upgrades, discretion, and keeping Metro Watch away.
The [crew phase banter](../../engine/include/fury/banter.hpp) establishes Rook's
lock/bag/comms work and Sparrow's lookout/heat-watching function. Rook's utility
wardrobe and `CrewTech` visual role coexist with the network `Muscle` assignment;
an art pass must not silently change that assignment or invent a technical class.

Individual ages, ethnicities, family relationships, gender identities, and
personal histories are not specified by these definitions. Gray hair does not
establish age. The two Vale surnames do not establish a relationship. `Dock` and
`Market` are existing wardrobe roles; they do not prove a particular employer
or trade. Proposed visual treatment below is art direction, not new story canon.

## Existing visual identity to carry forward

The [original character builder](../../engine/src/character_model.cpp) supplies
stable FNV-1a identity seeds, independent skin/hair/clothing palettes, facial and
body proportions, four hair treatments, and eleven wardrobe silhouettes. These
are useful starting constraints, rather than permission to assign random new
appearances. Color names below are descriptive; source linear RGB values are
authoritative. Security and Enforcer override shirt/trouser colors with their
uniform palette; BankStaff overrides the shirt with a pale service shirt.

The existing [18-actor game contact sheet](../images/npc-upgrade/population-after.png)
was inspected for this plan. It shows the recognizable bags, aprons, work vests,
uniforms, scarf/glasses, crew equipment, and cyan network vest. It also exposes a
shared narrow neck, faceted head, cylindrical limbs, small feature geometry,
rigid front-mounted clothing panels, and simple hair masses. It is historical
baseline evidence, not an all-side review of the upcoming individual assets.
Nell, Lia, Owen, and Hale are partly hidden by scenery; Tessa and Renn use live
AI samples. Their visibility/pose differences cannot be treated as quality proof.

The table records the original builder's deterministic proportions. `S/W/F`
means shoulder/waist/face scale relative to that builder, not real anatomical
measurements. Mira's current individual profile already supersedes its original
rig and geometry. Security caps suppress the generic rear-bun/side-hair options;
the seeded hair option is not evidence of a visible hairstyle under the cap.

| Actor | Stable seed | Original hair color / treatment | Original S / W / F |
| --- | --- | --- | --- |
| Mira | `e9850b19` | Near-black / swept crown | 1.00 / 0.90 / 1.04 |
| Jon | `e6850660` | Chestnut / rear bun volume | 1.00 / 0.90 / 0.99 |
| Tessa | `e78507f3` | Gray / close crown | 1.05 / 0.94 / 1.03 |
| Nell | `d9d5f382` | Near-black / rear bun volume | 0.95 / 0.95 / 0.95 |
| Pax | `ec850fd2` | Gray / rear bun volume | 0.92 / 1.10 / 0.95 |
| Rina | `ed851165` | Near-black / close crown | 0.99 / 0.95 / 0.99 |
| Lia | `327b8688` | Near-black / swept crown | 1.07 / 1.04 / 1.02 |
| Owen | `485fc311` | Auburn / close crown | 1.07 / 0.96 / 0.94 |
| Hale | `6ef263f5` | Dark brown / uniform cap | 0.96 / 0.94 / 0.96 |
| Renn | `b242d872` | Near-black / uniform cap | 1.07 / 0.96 / 1.03 |
| Ofc. Vale | `f5cd87d6` | Near-black / uniform cap | 1.03 / 0.95 / 1.02 |
| Cass | `513cdaa9` | Auburn / swept crown | 0.94 / 1.00 / 1.02 |
| Rook | `6a40f3fd` | Auburn / side hair masses | 1.07 / 0.93 / 1.03 |
| Sparrow | `301a7ca8` | Near-black / side hair masses | 0.91 / 1.03 / 0.94 |
| Metro Watch | `3e2ad98f` | Auburn / uniform cap | 0.93 / 0.91 / 0.97 |
| Enforcer | `bb361ac4` | Chestnut / rear bun volume | 1.09 / 1.02 / 0.96 |
| Player | `724a6d0c` | Chestnut / rear bun volume | 1.04 / 0.98 / 1.06 |
| Network avatar | `46b6a22e` | Chestnut / swept crown | 1.03 / 1.13 / 1.03 |

## Individual briefs and acceptance priorities

Each brief requires the full report checklist below in addition to its specific
checks. Anatomy, hair, and garments need actual form changes, not just denser
triangulation, new tint, or texture noise. Preserve the current stable skin/hair
palette unless a deliberate art decision is separately documented. Reference
subjects describe what to study during that character's pass; this planning
audit does not claim those future studies have happened.

### 1. Mira Vale — `NpcCivA` — integrated checkpoint; see dated reports

- Preserve the commuter's blue jacket, diagonal strap, side bag, dark swept hair,
  and 1.75 m height. Her always-on-duty route warrants both daylight and night
  scene review.
- Continue the integrated CC0 anatomical head/neck with fitted eyes, natural
  head/neck/shoulder transitions, connected hands, jacket construction, and hair
  coverage from the rear. Revisit remaining visible problems after the new
  anatomy integration rather than declaring the earlier checkpoint final.
- Report current geometry/material costs and all-side findings in the existing
  [Mira work log](mira/WORK_LOG.md), [review](mira/GEOMETRY_REVIEW.md), and
  [provenance](mira/ANATOMY_PROVENANCE.md). The elapsed window is a minimum;
  acceptance still depends on what the actual rendered images show.

### 2. Tessa Quill — `NpcCivC` — integrated checkpoint; see dated reports

- Preserve the shortest roster height, gray close-crown hair, ochre-brown jacket,
  brown trousers, and commuter satchel. The wider original shoulder/face ratios
  should distinguish this silhouette from a uniformly shrunken Mira.
- Study a compact adult head/neck and short gray hair in profile. Author distinct
  jaw, cheek, nose, brow, and ear forms without turning hair color into an age
  claim. Construct a quieter short-hair rear silhouette with visible hairline.
- Tailor the commuter jacket/strap/bag to the 1.65 m proportions, including cuff,
  collar, hem, and bag depth. Inspect strap clearance at the broader shoulders
  and bag-to-thigh clearance in the east-bridge route's turns.

### 3. Owen Pike — `NpcBankCust` — integrated checkpoint; see dated reports

- Preserve the 1.74 m bank customer, auburn close crown, blue-gray commuter
  jacket, olive-brown trousers, satchel, relatively broad shoulders and narrow
  original face. Do not rename Owen as a bank employee.
- Study close-cropped hair and narrow facial planes; distinguish nose/chin/ear
  geometry from Tessa and Mira, including the posterior skull and nape.
- Construct a more structured jacket shoulder and sleeve with the existing bag
  shape. Review collar/neck compression during the slow lobby turns and talk.
  Show a full unobstructed body on the review stage, then a real lobby view that
  honestly retains its chairs and counter occlusion.

### 4. Lia Merrow — `NpcTeller` — integrated checkpoint; see dated reports

- Preserve the 1.68 m teller, broad original shoulder/waist proportions, dark
  swept hair, pale service shirt, pointed collar, narrow tie, and name badge.
  The existing silhouette has no satchel or protective vest.
- Study service-shirt construction and swept short hair: collar stand, tie knot,
  placket, sleeve/cuff volume, hair part, temple, and nape. Give Lia a distinct
  head shape and face rather than reusing Mira with a shirt swap.
- Hands, wrists, elbows, and collar are especially exposed at the counter.
  Inspect the tie's depth and attachment while talking and turning; include
  neutral full-body and back images because the counter hides them in gameplay.

### 5. Jon Keel — `NpcCivB` — integrated checkpoint; see dated reports

- Preserve the 1.70 m market silhouette, chestnut rear bun, muted-rose shirt,
  plum-gray trousers, brick apron, and relatively narrow waist.
- Study tied hair from rear/side and apron attachment: believable hair growth
  into the tie, bun/skull separation, bib strap routing, waist ties, hem weight,
  patch-pocket thickness, and rolled cuffs.
- Author distinct facial planes and exposed forearm/hand anatomy. Replace the
  flat bib/waist slabs with connected garment construction that still reads as
  an apron. Review torso bends, hand/pocket clearance, and rear waist fastening.

### 6. Nell Ash — `NpcCivAsh` — integrated checkpoint; see dated reports

- Preserve the 1.75 m Ashcourt market civilian, narrower shoulder/face ratios,
  dark rear bun, stone shirt, blue-gray trousers, and teal apron. Nell must remain
  identifiable beside Cass rather than inheriting the broker's visual role.
- Study a narrower head/neck transition, dark gathered hair, and taller apron
  drape. Use distinct facial landmarks, tied-hair mass, collar fit, and apron
  pocket/hem proportions from Jon; avoid the same asset scaled up and recolored.
- Show the whole back, waist ties, and hem on the neutral stage. Find a usable
  actual-game camera around the shop fixtures without hiding those fixtures or
  moving Nell to manufacture an unobstructed result.

### 7. Pax Wren — `NpcCivD` — integrated checkpoint; see dated reports

- Preserve the 1.72 m civilian, narrow original shoulders with a fuller waist,
  gray rear bun, teal rolled-sleeve shirt, orange reflective work vest, cargo
  pocket, and olive-brown trousers. The wardrobe alone does not establish age
  or a specific dock job.
- Study how a high-visibility vest hangs over this torso, including the armhole,
  open front, side seam, rear panels, strip stitching, and hem. Give the body
  believable ribcage/pelvis transitions rather than enlarging the waist ring.
- Refine forearms, wrists, grip-ready hands, shoe construction, and tied gray
  hair. Inspect vest-to-arm clearance and cargo-pocket deformation during the
  2.2 m/s route; reflective strips must remain surface material, not fake light.

### 8. Rina Holt — `NpcCivE` — integrated checkpoint; see dated reports

- Preserve the 1.68 m civilian, more even shoulder/waist proportions, near-black
  close crown, plum shirt, orange reflective vest, brown trousers, and cargo
  pocket. Retain the shared safety-vest language with Pax.
- Study compact workwear proportions, short-hair contour, and a separate face
  with distinct cheek/chin/nose forms. Fit the vest and sleeves to Rina's own
  torso and arm lengths; do not uniformly scale Pax's entire model.
- Emphasize wrist/cuff anatomy, vest thickness at the shoulder, leg taper, and
  footwear. Review both side profiles, pocket/hand intersections, and turning
  around the eastern route corners at its authored 2.05 m/s speed.

### 9. Sgt. Hale — `NpcGuard` — integrated checkpoint; see dated reports

- Preserve the 1.85 m bank guard, narrower original shoulder/face scales, uniform
  cap and brim, dark protective vest, badge, radio, and belt pouch.
- Study cap construction, a face readable under the brim, vest side/back closure,
  radio attachment, trouser break, and service footwear. Give Hale a separate
  facial structure without inventing age, scars, rank insignia, or backstory.
- Build natural neck/trapezius transitions under the collar and cap-fit hairline.
  Inspect rear vest depth, belt/pouch attachment, and arm clearance during the
  3.5 m/s chase. Include nighttime readability because this patrol tightens at night.

### 10. Ofc. Renn — `NpcDeskGuard` — integrated checkpoint; see dated reports

- Preserve the 1.82 m interior guard, broader original shoulders/face, existing
  darker skin palette, cap, vest, badge, radio, and belt pouch. These proportions
  and facial identity should distinguish Renn from Hale in the same uniform.
- Study interior-lit facial planes, cap/ear/nape fit, and how the vest opens
  around broader shoulders. Ensure the original complexion remains independent
  of occupation and receives accurate material/lighting treatment.
- Review slow 1.15 m/s interior turns and the separate 3.4 m/s chase. Inspect cap
  shadow, eyes, neck, cuffs, belt, and full back in both neutral and bank lighting;
  a brighter skin tint is not a substitute for legible lighting.

### 11. Ofc. Vale — `NpcAlleyHmpd` — integrated checkpoint; see dated reports

- Preserve the 1.86 m alley officer, near-black underlying hair, cap, uniform
  equipment, and existing broader head/shoulder ratios relative to Hale.
  No family connection to Mira is established.
- Study side-view nose/jaw/ear silhouette and cap brim thickness because the
  alley baseline is dominated by profile. Construct a distinct exposed hairline
  and nape, shoulder/vest junction, and functional radio/belt attachments.
- Review both directions of alley lighting, nighttime tightened patrol, and
  3.8 m/s chase. Inspect cap/ear intersections, rear equipment, and shoulder/arm
  movement rather than accepting one favorable side portrait.

### 12. Metro Watch — `NpcExtraGuard` — integrated checkpoint; see dated reports

- Preserve the 1.82 m complication reinforcement and comparatively narrow
  shoulder/waist ratios. The display name is a unit label, not a new personal name.
  Keep the security cap, vest, badge, radio, and belt silhouette.
- Study a leaner uniform fit, separate face, and auburn hairline at the cap edge.
  Retain the same security equipment vocabulary with individually fitted jacket,
  vest, sleeves, and trousers. Do not clone Renn because the heights match.
- Inspect the initial chasing pose, 4.0 m/s pursuit, dynamic spawn/despawn, and
  repeated respawn. Confirm the same identity/material/mesh allocation returns
  and that the actor remains part of the complication flow.

### 13. Cass Vesper — `NpcFence` — integrated checkpoint; see dated reports

- Preserve the 1.78 m Ashcourt broker, narrow original shoulders, auburn swept
  hair, teal tailored jacket, gray-green trousers, mauve scarf/lapels, and dark
  glasses. Cass's trading/discretion dialogue is a useful wardrobe context, not
  a license to add unsupported affiliations or personality history.
- Study tailored lapel roll, jacket shoulder/hem, scarf folds and attachment,
  and actual eyeglass frame construction. Fit lenses, bridge, temples, and ears
  to a distinct anatomical face; keep the face believable with glasses removed
  in a clearly labeled diagnostic view if needed.
- Inspect glasses in both profiles, scarf/neck intersections during talk, jacket
  rear vents/hem, and shop-side lighting. Preserve a quiet recognizable silhouette
  without using unnecessary reflective ornaments to suggest wealth.

### 14. Rook — `CrewRook` — integrated checkpoint; see dated reports

- Preserve 1.70 m, broader shoulders with a narrow original waist, auburn side
  hair, stone shirt, gray-green trousers, diagonal utility harness, repair pouch,
  and visible tool handles. Preserve both `CrewTech` presentation and network
  `Muscle`; existing banter supports lock work and carrying bags.
- Study practical harness routing, reinforced pouch construction, tool retention,
  rolled-cuff detail, and hands/forearms. Author distinct strong head/neck forms
  and coherent side-to-rear hair; broad shoulders do not require a caricature.
- Inspect pouch/hip clearance and harness compression at full arm swing, crouch,
  and active-heist following. Capture the real left/trailing position as well as
  conversation poses. Idle-mission footage alone does not prove following works.

### 15. Sparrow — `CrewSparrow` — integrated checkpoint; see dated reports

- Preserve 1.72 m, narrow original shoulders, dark side hair, teal cropped jacket,
  plum-gray trousers, compact backpack, shoulder straps, and ochre accent.
  Scout presentation and network `Lookout` agree with the heat-watching banter.
- Study backpack volume/contact, strap routing, cropped-jacket construction,
  compact head/neck proportions, and an individually shaped short hairstyle.
  Distinguish the face and body from Rook before adding equipment details.
- Rear and rear-three-quarter views are primary acceptance views for the pack:
  inspect back clearance, shoulder attachment, zipper/seam construction, and
  silhouette while crouching and turning. Verify real right/trailing heist motion.

### 16. Syndicate Enforcer — `NpcEnforcer` — integrated checkpoint; see dated reports

- Preserve the tallest height, 1.95 m, broad original shoulders, existing skin
  palette, chestnut rear bun, broad protective vest, rounded shoulder pads, and
  front armor panels. This is a rare fast adversary, not an unnamed extra guard.
- Study substantial human mass under padded protection: ribcage/neck/shoulder
  anatomy, realistic pad thickness/straps, layered vest edges, and boot volume.
  Keep a distinct face and tied hairstyle; do not use facial distortion, scars,
  complexion, or an invented expression as a shortcut for antagonism.
- Inspect neck-to-pad clearance, head turns, arm elevation, rear vest, and actual
  5.6 m/s pursuit. Validate dynamic respawn and the largest silhouette at near/far
  LOD; raw size or extra polygons do not establish better anatomy.

### 17. Third-person player — `PlayerBody` — integrated checkpoint; see dated reports

- Preserve 1.75 m, the existing skin palette, chestnut rear bun, muted-rose shirt,
  gray-green trousers, diagonal strap, compact back pouch, and chest accent.
  This is the existing fixed player presentation, not a newly named protagonist
  or a character-customization feature.
- Study a coherent face, gathered hair, shoulder/back anatomy, compact pouch
  attachment, and flexible everyday clothing. The regular follow camera makes
  the back of the head, nape, shoulders, hands, and footwear high priorities.
- Inspect real V-toggle third-person behavior, walking/running, crouching,
  turn-in-place, and hide/show transitions. Keep standing rig height during
  crouch and inspect strap/pouch contact. The scripted `--npc-motion` route does
  not support this ID; actual player input evidence is required.

### 18. Network avatar — `GhostLoop` — integrated checkpoint; see dated reports

- Preserve 1.80 m, fuller original waist, chestnut swept crown, rust shirt,
  plum-gray trousers, and cyan vest with horizontal identification stripe.
  `Ghost` means the existing network proxy, not supernatural/translucent anatomy.
- Study a separately fitted torso, swept hair/nape, distinct face, and vest front,
  side, and back construction. Reuse player controller-compatible anatomy tools,
  not the player's face or clothing with a cyan tint applied to the whole body.
- Inspect actual remote heading, travel-driven gait, stationary updates, and
  visibility changes. Verify the existing heat/cash material modulation still
  works with the new material. A neutral-stage run or unsupported `--npc-motion`
  invocation cannot establish network animation correctness.

## Dependency order and honest time accounting

The initial accepted sequence is Mira → Jon → Tessa → Owen → Lia → Nell → Pax
→ Rina. Hale, Renn, Ofc. Vale, Metro Watch, Cass, Rook, Sparrow and Enforcer
have since been integrated. Player and Ghost are also integrated; the full roster checkpoint is complete
with the explicitly retained quality and motion limitations.
Each actor retains independently fitted anatomy, wardrobe, reference study and
review. Crew, rare adversaries, player and network actors keep their distinct
real-game evidence requirements.

Only start an actor's clock when dedicated reference/art/review work begins.
Record actual start/end timestamps and meaningful work intervals, interruptions,
shared-tool work, waiting, and unresolved issues separately. Do not backdate a
start, overlap one worker's time across several actors, or count this roster
audit and a common base import as two hours for each recipient. Render waits
should support active critique/revision where possible; time elapsed without
work is not a completed pass.

A practical two-hour session can allocate roughly 20 minutes to identity and
reference study, 30 to anatomy/hair, 35 to wardrobe/hands/footwear, 25 to rendered
critique and corrections, and 10 to evidence/reporting. These are planning
allocations, not automatic approvals. If all-side views still expose defects,
the actor remains in progress and gets further iteration. Pause for a dependency
blocker rather than marking the pass complete with inherited or missing evidence.

After each individual passes review, retain a fresh 18-ID coverage ledger.
At this checkpoint it reads **eight integrated individual art checkpoints;
Hale, Renn, Ofc. Vale, Metro Watch and Cass active; five not started**. Cumulative
recovery verification is open above. This is a pass-coverage ledger, not an
assertion that production art is finished. Change each status only from that
actor's actual evidence and credited effort.


## Shared foundations that preserve individual work

1. **Anatomical source and importer.** Reuse the pinned CC0 mesh-data provenance,
   original importer approach, source correspondence, UV seams, and landmark
   extraction from [Mira's provenance](mira/ANATOMY_PROVENANCE.md). Give each new
   face a reviewable recipe and verify any additional assets/licenses. Current
   vendored targets support Mira's recipe; they do not establish a complete
   roster library. Never describe a single unchanged head on 18 outfits as
   individual anatomy work. Source parameter names do not assign ethnicity.
2. **Rig/pose contract.** Reuse 17 joint semantics, deterministic skinning,
   animation/controller integration, height/origin/sole conventions, and mesh
   ownership. Fit pivots and shoulder/neck geometry to each anatomical asset;
   near/far representations of that actor must agree. Preserve gameplay height
   and stable ID even if authored visual proportions change.
3. **Surface system.** Reuse atlas sampling, correct linear/sRGB treatment,
   bounded mips, material categories, and shared immutable textures where they
   genuinely match. The current Face tile locates pigment from **Mira's actual
   anatomical positions and UVs**. A differently morphed or UV-mapped head must
   validate lips/eyes/nose placement and seam continuity; it must not inherit
   Mira-specific placement by assumption. Common shaders do not guarantee a
   suitable common facial texture. Existing generic meshes still need atlas UV
   authoring before the new material is applied.
4. **Garment methods.** Reuse construction primitives for seams, hems, cuffs,
   straps, soles, closures, and panel thickness, and common security/workwear
   vocabulary. Fit and inspect each actor's own collar, armhole, waist, hem, hair,
   face, hands, and accessories. Rear construction is mandatory even when the
   baseline only had a recognizable front panel.
5. **Review tools and tests.** Reuse the production renderer/diagnostic stage,
   camera/lighting presets, pose sampling, geometry checks, and source fingerprints.
   Evidence must name the actual actor and current source. An inherited test pass,
   another character's screenshot, or a contact-sheet thumbnail is not a substitute.
6. **CPU budget.** Share immutable data and reduce unimportant hidden detail where
   justified by images. Mira currently keeps an 8,824-triangle anatomical head
   in both LODs. Blindly replicating that decision across 18 actors carries a
   substantial CPU skinning/ray-update cost. Measure the populated scene as new
   actors land; author a feature-preserving lower-detail head only with actual
   image/deformation validation. Do not trade away eyelids, lips, ears, or identity
   merely to report a percentage reduction.

## Required report for every actor

Create a short report under `docs/characters/<actor>/` after each pass, with
these items present and each unresolved item clearly marked. Keep exact actor
IDs in filenames or a manifest even when the directory uses a readable name.

- **Identity and baseline:** exact ID/name/role/height, stable seed, source revision,
  canonical facts versus new art choices, original palette/silhouette to preserve,
  and matching before images. Record deliberate deviations and their reason.
- **Actual work record:** dated start/end and work intervals, breaks/waits/shared
  work, concrete iterations, and the dedicated time completed. Link intermediate
  evidence when a visible defect prompted a revision.
- **Reference study:** sources actually viewed, which front/profile/rear anatomical,
  hair, or garment features informed the work, and asset license/provenance where
  anything is imported. The existing [Mira reference list](mira/CHECKPOINT_01.md)
  is a starting point, not evidence of another actor's completed study. Do not
  redistribute proprietary reference art or claim to have imported it.
- **Substantial changes:** individually describe anatomy, face, hair, hands,
  clothing, footwear, accessories, and material work. Say which visible defects
  were fixed; triangle counts and material-map count are supporting data only.
- **All-side diagnostic evidence:** front, +45°, +90°, +135°, back, −135°, −90°,
  and −45° body views, then the same head views, at fixed camera/lighting. Add
  top/low views for scalp, shoulders, soles, or undercuts as needed. Include clay
  and material comparisons and near/far images. Use world axes to avoid ambiguous
  anatomical left/right labels. Record the actual renderer and capture settings.
- **Pose/contact critique:** bind, idle, walk, run, talk/head turn, crouch, and
  crouch-walk where supported; check both stride extremes and turning. Inspect
  eye seating, hair/skull, neck/collar, shoulder seams, wrist/cuff, fingers,
  strap/bag/pouch, hem/thigh, knee, shoe/ground, and accessory attachment from all
  relevant sides. Diagnostic stress poses do not imply the actor uses every pose
  in its normal gameplay role.
- **Actual-game evidence:** original world position and real lighting, close face,
  full-body/oblique views where the scene permits, and actual gameplay motion.
  Preserve scenery and disclose occlusion. Check day/night where appropriate.
  For crew, trigger real heist following; for dynamic actors, exercise the spawn
  flow; for player/network, use their real inputs/state. Distinguish presentation
  talk samples from Q/dialogue interaction.
- **Geometry/material integrity:** finite nondegenerate outward geometry,
  appropriate normals, valid skinning, UV/surface separation, sole/height bounds,
  source/UV/landmark correspondence for imported anatomy, deterministic identity,
  no material bleed at retained mips, and stable mesh/storage while posing.
- **Runtime/LOD cost:** actual vertices/triangles, retained texture memory, near/far
  identity and transition observations, pose/ray-update cost, and populated-scene
  performance under recorded resolution/backend/settings. Simulated video cadence
  is not live FPS; llvmpipe is not discrete-GPU or Windows DXR evidence.
- **Regression and verdict:** run applicable current profile/model/material,
  animation, renderer, presentation, and gameplay/runtime checks after edits;
  distinguish passed, failed, and not run. Include a short visual verdict with
  remaining defects, evidence links, and next correction. Final roster acceptance
  requires all 18 reports, not just a successful build or an elapsed-time total.

The [character review stage](../CHARACTER_REVIEW_STAGE.md),
[actual-game capture controls](../NPC_UPGRADE.md), and
[renderer/frame capture contract](../FRAME_CAPTURE.md) define the existing
inspection paths. This plan requests future evidence through those paths; it
does not claim that any of those new captures or tests have already run.
