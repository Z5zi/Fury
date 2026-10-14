# Tessa Quill: reference study and original design criteria

Study date: 6 October 2026. This brief records viewed reference pixels and
proposed art decisions. It does not certify the finished model, deformation,
performance, or parity with a commercial game's character art.

## Identity and scope

Preserve `NpcCivC`, `Commuter`, 1.65 m, seed `e78507f3`, the existing route and
gameplay behavior. The [roster plan](../ROSTER_PLAN.md) identifies gray close-crown
hair, an ochre-brown jacket, brown trousers and a commuter satchel. Its original
shoulder/waist/face ratios are 1.05/0.94/1.03. Tessa should read as a compact,
slightly broader figure rather than a uniformly scaled-down Mira.

The [original appearance function](../../../engine/src/character_model.cpp)
supplies linear RGB skin `(0.790, 0.585, 0.415)`, hair `(0.420, 0.440, 0.420)`,
jacket `(0.480, 0.420, 0.280)`, trousers `(0.250, 0.210, 0.170)` and accent
`(0.680, 0.490, 0.200)`. Keep these identity anchors. Mature feminine presentation
is the chosen visual direction, not a newly established age, gender identity,
ethnicity or biography. Gray hair alone establishes none of those facts.

## Reference provenance and rights boundary

Only public presentation photographs and rendered images were downloaded.
Rockstar's own screenshot gallery, artists' own presentations and manufacturers'
own garment/product pages are the sources below. The local study includes source
pages, exact image URLs, dimensions, SHA-256 hashes and viewing status in
`out/character-study/tessa/references/source-ledger.json`. Those files are private,
ignored research material, not shipping assets.

No redistribution license for the Rockstar, Naughty Dog, Marmoset, Merchant &
Mills or Cambridge Satchel images is asserted. Their public availability does
not grant permission to import their meshes, textures, scans, logos, patterns
or character likenesses. Use the observations to construct Tessa's own forms.
Do not copy Maria's face, Lucia's outfit, Brian's hairline, or a branded bag.
The anatomy asset's separate license and provenance belong in Tessa's anatomy
report; these reference images provide no license for anatomy data.

## Viewed sources and specific observations

### Rockstar Games: GTA VI

Source: [official screenshot gallery](https://www.rockstargames.com/VI/media/screenshots).
Viewed full 3840 × 2160 downloads: Lucia Caminos **03, 04, 09** and Brian Heder
**01, 03**. Lighting and perspective differ, so these are construction and
surface-response references rather than calibrated color or size measurements.

- Lucia 03 separates a standing collar, turned lapels, front opening, pocket
  seam and sleeve compression. Her jacket's broad panels remain quiet between
  folds. Skin, leather, ribbed fabric and hardware respond differently to light.
- Lucia 04 exposes upper-lid overlap, a narrow lower wet edge, an embedded mouth
  seam, curved lip surface, alar depth and a chin turning beneath the jaw.
  Pores modify the reflected light without replacing those volumes.
- Lucia 09 shows a dense hair root mass underneath loose front strands and
  cloth draping from shoulders rather than a rigid chest plate. Strong backlight
  and bloom make it unsuitable for neutral skin-tone matching.
- Brian 01/03 show gray fibers over a darker root mass, regional skin hue,
  uneven cheek mottling and substantial lower-lid volume. Wrinkles follow local
  anatomy; they are not a uniform grid. His beard, recession, tattoos and facial
  proportions are outside Tessa's design.

### Soa Lee: Maria, The Last of Us Part II

Source: [artist's Maria presentation](https://soalee.artstation.com/projects/oAOrv4).
Viewed **maria-outfit**, **maria-shader**, and **maria-hex**. The images credit
Soa Lee and Naughty Dog. The page identifies the character and game but does
not enumerate every production contributor; treat it as a credited studio
presentation, not proof of sole authorship for every component.

The clay/color comparison is valuable because the lower lids, nose wings,
philtrum, mouth corners and chin already read in the clay half. Color adds warm
lid margins, lips and irregular pigmentation. The front and two three-quarter
heads preserve cheek/jaw depth; they are not a full rear/profile turnaround.
Hair grows out of a fine irregular boundary, with darker spaces beneath the
swept locks. The jacket's cuff gathers, pocket lips, collar roll and overlapping
front edges have physical thickness. Fine cloth detail stays subordinate to
the pockets and silhouette. Maria's longer hairstyle, freckles, jewelry and
particular face are reference observations, not Tessa requirements.

### William Paré-Jobin: short-hair construction

Source: [artist's Marmoset breakdown](https://marmoset.co/posts/real-time-character-design-creation-and-presentation-breakdown/).
Viewed **williamparejobin_06**, **07**, and **08**. The author identifies his
character-art role at Ubisoft Montreal in this article.

The crown/back diagram shows hair flow changing around a whorl and around the
ear. The four-step side comparison makes the roles of cap and cards explicit:
the fitted scalp layer provides continuous coverage; cards add depth, clumping
and an irregular silhouette. The isolated cards leave visible bands of bare
scalp. A shallow transverse bend helps a card catch light without becoming a
thick tube. This supports Fury's existing opaque `Hair` plus masked
`HairStrands` arrangement. It does not justify importing the example's textures,
copying its hairstyle, or disabling mipmaps globally. The tutorial's renderer
and viewport settings are not automatically appropriate for Fury.

### Merchant & Mills: Sanda coat and jacket

Source: [maker's Sanda photographs and construction drawing](https://merchantandmills.com/products/the-sanda).
Viewed all 20 gallery images as a contact sheet, plus full-size images **06,
07, 08, 11, 17, 18, 20** for collar, side, rear, cuff, pocket and seam inspection.
These are garment references; the photographed people are not likeness targets.

The side/rear coat views show continuous shoulder-to-back cloth, a yoke seam,
a collar that wraps the neck, and broad hanging planes. The short-jacket views
retain that construction at a higher hem. Close-ups expose an overlapping front,
a rolled edge, real cuff thickness and a pocket opening pulled outward by a
hand. The maker offers multiple fabrics, so the wool, twill and corduroy samples
have different surface and fold character. Tessa should use a restrained woven
coat interpretation with her existing warm palette and compact length, not copy
the oversized pattern or mix every sample's texture into one garment.

### Cambridge Satchel: bag construction

Source: [maker's 11 Inch Batchel, Bay](https://www.cambridgesatchel.com/products/the-11-inch-batchel-bay).
Viewed all six gallery images: front, rear, side, three-quarter with strap,
handle/edge close-up and open top. The earlier 11-inch Vintage page was no longer
downloadable; no conclusions depend on its stale availability or price.

The side view is the key: a gusset gives the bag depth, a continuous flap bends
over the top, and the shoulder strap attaches through a tab and ring on the
side. The leather's broad exterior reflection differs from the rough cut edge
and suede-like interior. Stitching follows actual joined edges. The rear is
quieter than the layered front. The branded shape, two-buckle arrangement,
handle and logo are not requirements for Tessa; retain her own simple satchel
while making its flap, depth, edge thickness and attachment credible.

## Original implementation priorities

These are authoring choices for Tessa, not copied measurements from the sources.
Geometry should establish the character in clay before fine atlas work.

1. **Compact silhouette.** Retain relatively broad shoulders and head width,
   with a connected neck widening into trapezius and chest. Use a modestly
   tapered torso rather than an exaggerated waist. Keep trousers and shoes
   proportioned to 1.65 m. The coat should have a continuous rear, softened
   shoulder corners and enough upper-back depth to avoid a front-only shell.

2. **Mature face without caricature.** Build distinct but soft cheek planes,
   a supported lower-lid shelf, slight separation of the cheek from the muzzle,
   a rounded chin and a jaw that turns toward the ear. Keep age cues concentrated
   around the orbit, mouth and neck rather than carving identical deep lines
   everywhere. Inspect nose projection and chin/throat separation in both
   profiles. Brows should taper and curve across the brow ridge. An ear needs
   a helix rim, inner bowl and attached lobe, visible between hair and jaw.

3. **Eyes seated in the face.** Fit the globe behind both lid rims, retaining
   upper-lid overlap over the iris and a small inner corner. Keep sclera subdued
   and warmer than white. Separate pupil, iris and sclera without creating a
   large front disk. A very narrow wet edge is enough; a bright rail beneath
   each eye will look mechanical. Test direct front, both profiles and the
   shaded three-quarter view for exposed globe, lid gaps and protruding irises.

4. **Short graying hair.** Make the coverage mass closely follow the skull,
   especially at temples, above ears and at the nape. Establish a small off-center
   crown flow with a few overlapping short locks. Use shallow thickness changes
   and tapered tips rather than equally thick parallel cords. A darker neutral
   root mass and selected gray fibers should read together as gray hair; a
   uniformly light cap becomes a helmet. Preserve small hairline irregularities
   without rows of isolated floating cards. The rear should follow the occipital
   curve and narrow naturally into the nape, with no rectangular hanging plates.

5. **Coat construction.** Form a collar with an inner opening, turned edge and
   rear continuation. The front overlap/placket must belong to the torso cloth.
   Give cuffs a turned lip and a slight shadow gap around the wrist. Put a small
   number of folds where cloth is constrained: inner elbows, sleeve ends,
   underarms and the satchel-bearing shoulder. Keep chest/back areas calmer.
   Add pocket rims and a hem with thickness. If a seam is visible, it should
   lead somewhere structurally plausible, not end in the middle of a panel.

6. **Satchel and strap.** Use a shallow rounded bag with a side gusset, bottom,
   overlapping flap and a quieter rear panel. Attach the strap to both ends
   of the bag, continue it over the shoulder and back, and fit it to the real
   coat surface. A tiny contact gap may prevent clipping, but it must not float
   visibly. Choose one simple fastening, retaining Tessa's own design. Check
   hand swing, hip/coat contact, walking turns and crouch clearance separately.

7. **Material detail at the correct scale.** Keep broad warm skin variation
   around cheeks, eyelids, nose, lips and ears; vary small pores softly. Use
   regional roughness so cheek, nose and lip responses differ without making
   the whole head glossy. Cloth weave belongs in a restrained normal/roughness
   signal; large folds belong in geometry. Bag leather should be smoother than
   coat fabric and rougher than eye highlights. Avoid uniform dirt, engraved
   pore noise, white hair stripes and painted highlights that survive in shadow.

## Fury-specific feasibility and review cautions

- The current shared atlas already separates Face, Skin, Jacket, Hair,
  HairStrands, Leather, Eyes and hardware. Material identity therefore needs
  correct surface assignment, UV flow and geometry as well as color changes.
  Fine texture frequency must be reviewed in actual CPU frames at gameplay
  distance; a magnified atlas alone does not demonstrate visible improvement.
- At the source inspection checkpoint, `character_surface.cpp::face_positions`
  derives Face pigment locations from Mira's anatomical positions. The source
  UV charts may be compatible, but a different facial shape is not evidence of
  correctly placed Tessa-specific pigment or wrinkles. Inspect the mapping on
  her model before changing the shared tile. Local anatomical vertex zones or
  a deliberately validated separate material can avoid unintended changes to
  already-reviewed actors.
- Preserve the [source-chart isolation](../FACE_TEXEL_AUDIT.md) and
  [surface contract](../../CHARACTER_SURFACES.md). Do not move facial marks by
  assuming atlas U is face left/right or V is head height. Do not use high
  contrast texture noise to hide weak underlying form.
- For hair, keep the opaque mass valid at far LOD and use selective masked
  cards near the silhouette. Short tuft lengths and widths should vary, while
  their direction remains coherent. Review side and rear cutout coverage in
  both bright and shaded light, including transitions across the LOD distance.
- Tessa's baseline face/body sheets were inspected. They show a faceted head,
  thin cylindrical neck, small detached-looking features, broad simple hair
  cap and rectangular rear hair pieces, front slab jacket, disconnected strap
  and box bag. These are baseline defects to repair, not observations about
  the later implementation under active development.

## Acceptance evidence to capture

Render body and close head from front, rear, both sides and both three-quarter
angles in matched neutral lighting. Include a clay pass, a lit material pass,
and a normal gameplay-distance view. Record the actual image paths and findings.

The front should distinguish Tessa's broader compact proportions and warm
coat/gray-hair arrangement from Mira and Jon. Profiles must show the back of
the skull, hairline, ear, nose, mouth, chin and throat as a coherent whole. The
rear must have complete scalp coverage, collar/yoke/hem construction and an
unbroken strap route. Front improvements do not compensate for a bald-looking
rear, flat bag or unconstructed coat back.

Review walk, turn, talk/gesture and crouch samples for coat/hand/strap/bag
contact, elbow and knee deformation, wrist continuity and exposed scalp gaps.
Check near/far silhouette and material stability in the renderer actually used
by the game. Numerical tests and triangle budgets do not certify art quality;
document remaining waxiness, coarse hair, cloth stiffness, visible intersections
or shading limits even when tests pass. The two-hour allocation is a minimum
working commitment, not evidence that these visual criteria have been met.
