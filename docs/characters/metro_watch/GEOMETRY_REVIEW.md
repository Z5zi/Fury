# Metro Watch geometry, fitting and motion review

## Production contract

`NpcExtraGuard`, displayed as **Metro Watch**, remains Security, seed
`0x3e2ad98f`, height **1.82 m**, original narrow-fit appearance parameters and
original skin/auburn-hair palette. Its role-specific shirt, trousers, cap,
carrier and equipment colors are preserved. The registry selects this profile
only for the exact identity and Security role. Near/Far share immutable atlas
textures and use the original individual-profile 12 m LOD switch.

Final mesh budgets are enforced, never waived:

- Near: **39,192 triangles**, below 40,000
- Far: **15,980 triangles**, below 16,000
- Both retain the complete **8,660-triangle Face** with exact source positions,
  ordered corners and canonical per-corner UVs
- Hands remain connected; source topology, wrist cycles and finite normalized
  skin weights are checked

## Fitting decisions supported by real pixels and geometry

The first draft exceeded both budgets. Clothing/cap sampling and invisible Far
trim were reduced instead of dropping Face triangles or disconnected fingers.
The cap now has an inner crown, joined lower edge, finite band and closed visor;
its silhouette retains side volume rather than narrowing into the old cone.
Far curvature rows are concentrated where they affect the silhouette.

Simple independently sampled carrier grids failed proper-intersection checks
between their samples. The final panels, shoulder strips and rib bands are cut
from the actual sewn shirt triangles, retaining their support weights. The
shoulder strips turn outside the collar and continue into the rear carrier.
The shirt has connected armholes/sleeves; the trousers use one joined waist,
two legs and a shared crotch seam, replacing the old overlapping seat shell.
Boot uppers/soles are closed, the ankle is fitted beneath the hem, and the
unneeded open tongue overlay was removed. Laces sit on the exposed instep.

Attention closeups revealed chin shear from a neck transition ending too high.
The transition was moved below the jaw; no Face position, triangle or UV was
changed. Scalp coverage is independently ray-tested against actual cap/hair
triangles. At 1.82 m, 8,019 Near and 7,996 Far source probes have no uncovered
sample or strict scalp crossing. Minimum sampled radial gaps are approximately
0.00067036h Near and 0.00092027h Far. Socket rims and inward spokes have actual
visible eye geometry; measured rim depth/h is approximately -0.002035..0.001322
Near and -0.002545..0.001125 Far, within the unchanged -0.003..0.002 gate.

## Explicit ordinary and strict stress scopes

The additional `character_metro_watch_ordinary` gate covers 1,440 posed samples:
Near/Far at 1.35, 1.82 and 2.10 m, 24 phases per state, bind continuity,
idle/walk/run at actual and stage speeds, talking, attention, static crouch and
combined attention states. It checks finite geometry/unit normals, immutable
indices and storage, exact palette retention, feet above ground, sewn seams,
shirt/carrier/neck/hand contacts and boot/cuff contacts. Four deliberately
injected crossing classes must be rejected. Closed-layer topology is checked
independently for carrier, metal, rubber, leather and the cap.

The **default `character_metro_watch` test remains strict and fails** on the
known full-crouch-walk stress case. The exact final witness is Near, height
1.82 m, mode 4, phase 3/24, pants triangle **14834** against boot triangle
**16873**, groups 5/6. Its assertions and intersection tolerances are unchanged.
The ordinary switch excludes only full crouch-walk modes 4 and 9; it does not
convert this stress failure into a pass.

A broader foot influence temporarily cleared that mathematical crossing but
visibly kinked the calf. It was rejected during pixel review and reverted to
an ankle-only transition. The plausible ordinary anatomy is retained, together
with explicit failing stress evidence. No shared animation/controller change
is made. The all-side stage includes unhidden stress poses, so the limitation
can be inspected rather than cropped away.

This is a sampled prototype review, not a continuous-time collision proof or
an AAA/GTA VI parity claim. Facial expression/blinking, per-finger animation,
cloth simulation and the existing guard navigation-through-solids issue are
not implemented by this individual art pass.
