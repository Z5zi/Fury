# Owen Pike: reference study and preparation

Study: **6 October 2026, UTC**. Actor **`NpcBankCust`**, **1.74 m**,
`CharacterRole::Commuter`. This is preparation for Owen's individual pass,
not an implemented asset or quality sign-off. **It does not start or count
toward Owen's dedicated two-hour modeling allocation.** No character source,
model, material, recipe, gameplay definition, or existing document was edited.

## Identity and observed baseline

The [roster](../../../apps/vaultline/npc_roster.cpp) establishes Owen as the
Meridian Mutual **customer**, a civilian with `DayOnly` availability, speed
0.9 m/s, initial position `(1.2, 0.87, -5.2)`, and lobby waypoints
`(1.2, 0, -5.2)`, `(-1.0, 0, -4.8)`, `(0.4, 0, -5.6)`.
The [role resolver](../../../apps/vaultline/npc_presentation.cpp) selects
Commuter. Preserve these and the name, height, controller and interactions.
The [roster brief](../ROSTER_PLAN.md) calls for broad shoulders, a narrow face,
auburn close-crown hair, blue-gray jacket, olive-brown trousers and side satchel.

The actual [appearance function](../../../engine/src/character_model.cpp),
independently recomputed from stable FNV-1a seed **`0x485fc311`**, gives:

| Identity anchor | Source value, linear RGB unless stated |
| --- | --- |
| Skin | `(0.790, 0.585, 0.415)` |
| Hair | `(0.270, 0.110, 0.052)`; hair style **0**, close crown |
| Eyes | `(0.130, 0.080, 0.045)` |
| Jacket base | `(0.270, 0.380, 0.430)` |
| Trousers | `(0.200, 0.190, 0.150)` |
| Accent | `(0.530, 0.520, 0.390)` |
| Shoes | `(0.190, 0.125, 0.078)` |
| Satchel/strap | Role constant `(0.220, 0.145, 0.085)` |
| Shoulder / waist / face scales | **1.07 / 0.96 / 0.94**, face style **0** |

These scales describe the generic builder, not measured anatomy. A relatively
narrow head should remain plausible in profile; it is not permission to shrink
every head dimension. Adult masculine presentation below is proposed art
direction, not newly established age, ethnicity, gender identity or biography.

Four existing-profile stage views were captured and inspected at front, +X,
rear and three-quarter, with identical 480 × 640 software-raster settings.
The unmodified generic near model reports **2,242 vertices, 2,268 triangles**,
`individualized: false`, zero validation errors, no texture maps and roughness
0.72. This is a diagnostic stage, not a lobby or gameplay-performance review.

The actual images reveal faceted facial planes, a long cylindrical neck,
disconnected-looking shoulder/sleeve junctions, rigid flat jacket front panels,
mitten-like hands, little trouser seat/knee shaping, and a severe rear hair gap.
The diagonal strap stops at the shoulder with no visible back continuation;
the satchel reads as a small block. Prioritize those structural problems before
surface noise. Baseline captures and reports are private study material under
`out/character-study/owen/references/baseline-*`.

## Viewed references and provenance

The public source pages and actual downloaded image pixels were inspected.
Large images were displayed at the viewer's supported resolution. The ignored
`out/character-study/owen/references/source-ledger.json` records every retained
download's exact URL, dimensions, byte count, SHA-256, source and viewing status.
Some files were copied from prior private studies and **re-inspected for Owen**;
the ledger records that origin. Contact-sheet-only views are labeled separately.

**Rights boundary:** the following proprietary images are private reference
material, not shipping assets. No reuse/redistribution license is asserted.
Do not extract their geometry, maps, garment patterns, logos or likenesses.
The CC0 anatomy source in the next section has a separate, verified license.

1. **Rockstar Games, GTA VI:** [official screenshot gallery](https://www.rockstargames.com/VI/media/screenshots).
   Individually viewed Jason Duval **02, 04, 05, 07**; other downloaded Jason
   views and Raul 01 were surveyed only on contact sheets. Jason 02 shows lids
   wrapping the eye, nose-wing depth, embedded mouth corners and a jaw turning
   into the throat. Jason 04/05 show close hair hugging a continuous skull,
   short temple growth and small irregularities at the hairline. Jason 07
   confirms regional skin response, but injury, sweat, colored lights and a
   foreground fence make it unsuitable for Owen's neutral complexion.
   Selected originals: [02](https://www.rockstargames.com/VI/_next/static/media/Jason_Duval_02.1486~7_v40cn..jpg?akim=1&imdensity=1&imwidth=3840),
   [04](https://www.rockstargames.com/VI/_next/static/media/Jason_Duval_04.16d-z2wa2s~bk.jpg?akim=1&imdensity=1&imwidth=3840),
   [05](https://www.rockstargames.com/VI/_next/static/media/Jason_Duval_05.0kxp6enhvzqka.jpg?akim=1&imdensity=1&imwidth=3840),
   [07](https://www.rockstargames.com/VI/_next/static/media/Jason_Duval_07.0ca326xbl~oyh.jpg?akim=1&imdensity=1&imwidth=3840).

2. **Anton Kozlov / IO Interactive, Hitman Paris security jacket:**
   [artist's production presentation](https://antonkoz.artstation.com/projects/lKOWY).
   Viewed the [game-model turnaround](https://cdnb.artstation.com/p/assets/images/images/005/206/215/4k/anton-kozlov-securityjacket-turn-game.jpg?1489321860)
   and [gray sculpt turnaround](https://cdna.artstation.com/p/assets/images/images/005/206/214/4k/anton-kozlov-securityjacket-turn.jpg?1489321809),
   both 3200 × 1680. The artist credits his high/game models, textures and
   implementation. Rear shoulder seams, armholes, front edges, pockets and cuffs
   have volume before color. Folds concentrate around elbows and attachments;
   broad panels remain readable. Owen should borrow the completeness of
   construction, not the hood, elastic hem, insignia, uniform or security role.

3. **William Paré-Jobin, character/hair breakdown published by Marmoset:**
   [original article](https://marmoset.co/posts/real-time-character-design-creation-and-presentation-breakdown/).
   Viewed the artist's Witcher-inspired portrait and diagrams
   [06](https://marmoset.co/wp-content/uploads/2018/08/williamparejobin_06.jpg),
   [07](https://marmoset.co/wp-content/uploads/2018/08/williamparejobin_07.jpg),
   [08](https://marmoset.co/wp-content/uploads/2018/08/williamparejobin_08.jpg).
   The fitted base covers the skull continuously; separate short cards add
   depth and soften boundaries. Cards alone leave bald bands. Growth changes
   direction around the whorl and ear; a shallow bend catches light without
   turning each piece into a tube. This supports Fury's existing opaque Hair
   plus masked HairStrands approach. Its beard, long swept top, fantasy styling
   and renderer-specific settings are not Owen requirements.

4. **Merchant & Mills, Foreman jacket:**
   [maker's photographs and garment drawing](https://merchantandmills.com/products/the-foreman).
   Surveyed 19 gallery images; individually viewed **04, 05, 15, 19**:
   [front](https://merchantandmills.com/cdn/shop/files/P-FOREMAN.jpg?v=1772659842&width=800),
   [shoulder/collar](https://merchantandmills.com/cdn/shop/files/P-FOREMAN_036c8a45-6e92-4f22-8258-43ac3460ce26.jpg?v=1772659842&width=800),
   [profile](https://merchantandmills.com/cdn/shop/files/P-FOREMAN_31c08b1a-7d31-4e1a-9ad3-f58e0e223402.jpg?v=1772659847&width=800),
   [front/back drawing](https://merchantandmills.com/cdn/shop/files/P-FOREMAN_ef73f4a5-0557-4560-a7c1-88c12b5114bb.jpg?v=1773314698&width=800).
   The shoulder joins a shaped sleeve, collar rolls away from the neck, front
   edges overlap, and patch-pocket mouths pull away from the body. The profile
   supplies back depth and a hanging hem. A two-piece sleeve is identified by
   the maker. Study fabric behavior and assembly, not the models' identities
   or a reproduction of the commercial pattern.

5. **Cambridge Satchel, 11-inch Batchel in Bay:**
   [maker's product page](https://www.cambridgesatchel.com/products/the-11-inch-batchel-bay).
   Individually viewed front, rear and
   [side construction](https://www.cambridgesatchel.com/cdn/shop/files/BAT111318MPG20101-0034.jpg?v=1770895174&width=2025).
   The side shows a gusset between panels, a flap bending over the top, a
   stitched/riveted attachment tab and a ring carrying strap load. Cut leather
   edges differ from the smoother broad faces. Owen retains his own smaller,
   taller satchel silhouette, single flap and brown palette; the brand, handle,
   double buckles and commercial dimensions are not copied.

## Anatomy source options, not an authored recipe

Read the existing [Mira](../mira/ANATOMY_PROVENANCE.md),
[Jon](../jon/ANATOMY_PROVENANCE.md) and [Tessa](../tessa/ANATOMY_PROVENANCE.md)
reports. Their base/target inputs all matched their manifests in a read-only
SHA-256 check: 8, 14 and 30 entries respectively, including shared references.
The existing MakeHuman/MPFB authored human mesh is a suitable source of
connected head/neck/ear anatomy and canonical UV correspondence. It is not a
scan of a real person. Use the shared base and separate morph data, not an
unchanged Jon head or a scaled Mira/Tessa head.

The upstream [license boundary](https://github.com/makehumancommunity/mpfb2/blob/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/LICENSE.md)
and [CC0 asset license](https://github.com/makehumancommunity/mpfb2/blob/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e/LICENSE.ASSETS.md)
cover graphical asset data separately from GPL addon logic. No addon code,
package or new morph data was installed, copied into production or executed.

An untruncated read of the [official pinned tree](https://api.github.com/repos/makehumancommunity/mpfb2/git/trees/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e?recursive=1)
confirms these available options under `src/mpfb/data/targets/`:

| Purpose | Candidate files | Preparation status |
| --- | --- | --- |
| Adult masculine foundation | Three `macrodetails/*-male-young.target.gz` inputs | Already vendored for Jon; choose a separate recipe |
| Narrower/longer head | `head/head-scale-horiz-decr.target.gz`, `head/head-scale-vert-incr.target.gz`, `head/head-rectangular.target.gz` | Tree presence verified; not downloaded or tested |
| Tapered chin | `chin/chin-width-decr.target.gz` | Tree presence verified; not downloaded or tested |
| Restrained nose width | `nose/nose-scale-horiz-decr.target.gz` | Tree presence verified; not downloaded or tested |
| Ear individuality | Paired `ears/l-ear-rot-backward.target.gz`, `r-ear-rot-backward.target.gz` | Tree presence verified; not downloaded or tested |
| Neck-to-shoulder transition | `neck/neck-scale-horiz-incr.target.gz`, `neck/neck-scale-depth-incr.target.gz` | Already vendored for Jon |

Target labels do not establish Owen's ethnicity or literal age. Set weights
through measured shape and visual review during the dedicated pass. Any newly
selected target needs its own immutable URL, byte count, SHA-256 and Git blob
verification. Preserve source topology/UVs, complete triangles and attachment
boundary; compare corresponding vertices against prior characters. Recompute
Owen's actual eye/mouth seams and neck sections. Rig/helper markers are not
surface placement points, and Tessa/Jon's fitting numbers do not transfer.

## Original authoring criteria for the later pass

- **Head and body:** a longer, narrower face with softly tapered jaw and a
  modest chin, distinguished from Jon's squarer jaw and Tessa's compact rounder
  head. Give the bridge, alae, eyelids, mouth corners and ear folds connected
  volume. Preserve posterior skull depth, a clear jaw/throat break and a neck
  spreading into broad clavicle/shoulder forms. Keep his narrower waist and
  grounded 1.74 m proportions; check the clay silhouette before adding detail.
- **Hair:** continuous auburn close-crown coverage around crown, temples,
  ears and nape. Establish a restrained crown whorl and shorter sides. Sparse
  shallow tapered strands may soften the boundary, but must not create a
  swept quiff, bun, radial ribs or another hole in the rear scalp. Keep the
  existing exact total height when reserving room for hair.
- **Jacket:** a moderately structured blue-gray commuter shell, with a clear
  shoulder seam, fitted armhole and slightly forward-curving sleeve. Replace
  chest slabs with continuous front/side/back cloth. Use a small turned collar,
  central closure, shallow practical pockets, finite cuff/hem thickness and
  broad quiet panels. Put compression at elbows, underarm and strap contact;
  avoid evenly spaced horizontal ridges. The collar must clear the new neck
  throughout talk and slow turns.
- **Satchel:** preserve its small side location and brown identity. The
  existing main envelope is approximately 14.3 × 16.7 × 12.5 cm at 1.74 m;
  this is a builder-derived starting envelope, not a physical bag measurement.
  Round the corners, construct front/back/gusset/flap depth, and attach the
  strap to both sides. Trace the strap over the opposite shoulder and across
  the back, with garment clearance and modest contact compression. A strap
  should neither float far from the body nor disappear inside it.
- **Hands, trousers, shoes:** give wrists, thumb roots, staggered fingers and
  knuckles readable connected shapes. Tailor olive-brown trousers through
  waistband, seat, knee and ankle; maintain a soft break over distinct brown
  shoe uppers, toes, heels and soles. Avoid excessive surface lines that
  substitute for those forms.
- **Materials:** preserve the linear identity colors. Separate matte woven
  jacket/trouser response, smoother leather faces with visible edge thickness,
  restrained auburn hair highlights, and softer regional skin variation.
  Seams and grain remain subordinate to form. Do not transfer colored night
  lighting, wetness, scars or strong pore contrast from a reference image.

## Review gates once implementation starts

1. Capture matched full-body and face front, both sides, rear and three-quarter
   views, plus clay diagnostics; inspect nape, ears, collar, sleeve roots,
   wrists, bag depth, back strap and soles. Preserve camera/light/height parity.
2. Sample the walk, talk and slow left/right lobby turns. Inspect neck/collar,
   shoulder/strap, wrist/cuff, bag/thigh and hand/bag clearance through motion.
3. Capture an unobstructed review-stage body, then an honest real lobby view
   with its chairs/counter still present. Stage screenshots do not prove
   gameplay integration, animation completeness or world rendering cost.
4. Verify stable gameplay identity and height, actual near/far geometry and
   material costs, all relevant render backends, and prior-character isolation.
   Record remaining visible problems. Neither this research nor the later
   elapsed allocation alone establishes finished art or GTA VI parity.
