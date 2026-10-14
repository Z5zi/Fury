# Pax Wren — reference study and critique criteria

Pax is `NpcCivD`, a 1.72 m Dock civilian. The source establishes a work vest,
gray tied/rear-bun hair, a relatively broad waist and narrower shoulder/face
ratios. It does not establish an age, ethnicity, employer or new biography.
`PALETTE.json` records the exact independent linear RGB values derived from
`character_appearance(0xec850fd2)`, including the role's orange vest and pale strips.

Actual source image pixels were viewed during this pass, not just captions:

- [Rockstar's official GTA VI screenshots](https://www.rockstargames.com/VI/media/screenshots),
  Jason Duval 02 and Cal Hampton 01. Jason's image supplies eye seating, nose-wing
  and jaw/throat transitions; Cal's fuller body demonstrates how an open outer
  layer hangs independently of the shirt and how sleeves transition to exposed
  forearms. Neither likeness, tattoos, clothes pattern nor props are copied
- [Anton Kozlov's production Hitman Paris jacket presentation](https://antonkoz.artstation.com/projects/lKOWY),
  actual front/rear/oblique game-model turnaround. Study the complete back,
  continuous armholes, fold concentration and real closures. Do not inherit its
  security identity, hood, logos or equipment
- [William Paré-Jobin's Marmoset character breakdown](https://marmoset.co/posts/real-time-character-design-creation-and-presentation-breakdown/),
  actual hair-growth/coverage diagram 06. The opaque fitted base must cover the
  skull; separate strands soften the boundary and indicate growth. Hair cards
  are not evidence that the underlying scalp cap is safe
- [Portwest's official vest collection](https://www.portwest.com/products/clothing/vests/137/8),
  actual [C370 product image](https://d11ak7fd9ypfb7.cloudfront.net/styles400px/C370ORR.jpg).
  Study the bound V opening, roomy armholes, overlapping front, shoulder bands
  and slightly rounded hem. Pax retains his own two vertical front bands and
  original palette; no logo, safety certification or commercial pattern is copied
- [Red Wing's official 606 product page](https://www.redwingshoes.com/mens/work/soft-toe/supersole-2.0-00606.html),
  actual top and outsole images. Study the toe box, narrower heel, supported
  instep, tongue/eyelet layering and outsole depth. Pax's original dark shoes
  receive a separate unbranded shape; the manufacturer's branding and tread
  pattern are not assets

These proprietary images remain private visual references. They are not part
of source/recovery packages or shipped assets. The separate CC0 MakeHuman/MPFB
base and graphical targets have their own pinned provenance in
`third_party/pax_anatomy/MANIFEST.json`.

## Findings from the actual baseline and iterative pixels

The baseline's 2,328-triangle Near model has a long cylindrical neck, flat
vest slabs with no back, a severe rear hair gap, small faceted features,
mitten hands and block-like cargo pocket. The original world spawn is visibly
clear; scene scenery and all other actors were retained.

The first individual silhouette had a broader waist but an implausible
underarm pinch, clipped shoulder layers, a canister-like cargo pocket and
buried boot laces. Later iterations replaced the analytic vest fit with the
actual sewn shirt support, refitted the sleeve rest to the broad body, authored
a cargo pouch/flap over the thigh, and projected laces over the real instep.

Every-side images show that a new human mesh alone does not meet the visual
reference standard. Skin still reads too uniformly smooth; the gray tied hair
still has a cap-like base, and the simple cloth has too few broad compression
folds. Close head images exposed conspicuous strand-card root patches, leading
to narrower, tapered gathered strands and denser hairline coverage. Run/crouch
pixels exposed broken reflective strips, leading to actual supporting-triangle
fitting and additional Far torso support; all attempted corrections must pass
fresh rendered and numerical checks before final evidence is recorded.

At this checkpoint, no GTA VI parity or production AAA acceptance is claimed.
