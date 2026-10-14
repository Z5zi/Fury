# GhostLoop anatomical source and original fitted asset

GhostLoop is the existing unnamed 1.80 m network proxy, Ghost role, seed
`0x46b6a22e`. Its original skin, eye, hair, shirt, trouser, shoe and accent
appearance fields are preserved. The cyan vest uses the actual original
rendered role tint `(0.11, 0.49, 0.53)` and identification stripe
`(0.57, 0.79, 0.75)`, independently of those appearance fields.

Only pinned CC0 MPFB graphical data is imported. The independent
`third_party/ghost_anatomy/MANIFEST.json` retains exact upstream URLs, SHA-256,
Git blob IDs and lengths for 21 inputs at
`d0a32e57a7f915cb2f2b95410e2117648c7bbb7e`. The input mesh, UVs, metadata,
numerical morph targets and license statements are already present in the
accepted eight-character tree. No addon program logic is copied, imported or
executed. Reference photographs and professional renders are viewed privately;
none is shipped as an asset or texture.

## Face and source correspondence

The separately authored recipe chooses a broad rectangular face, a defined
chin with a soft jaw turn, forward brow and moderate nose projection. Morph
control filenames do not assign an age, ethnicity, gender identity or personal
history. One uniform source normalization maps the body foot floor to −0.5 and
source scalp crown to +0.486, reserving +0.014 for the swept hair mass. The final
sole-to-hair height remains the exact authored height.

Selection retains complete original body faces above normalized Y=0.330. It
starts below the whole exterior-head/neck chart boundary. The lower selection
is an attachment choice, not a jaw weighting cutoff. The output retains 4,357
source positions and 8,656 source triangles, original winding, per-corner UVs
and source vertex/UV/face/triangle IDs. Both LODs retain this face anatomy. The
complete 56-edge neck attachment cycle is joined to an exact Skin underlay;
shared source positions receive identical weights at the attachment.

The complete canonical exterior-head chart, mouth cavity and eye sockets are
rigidly attached to Head. In particular, all 418 mouth positions and 3,185
exterior-head positions, including the chin and descending exterior jaw, are
covered. Neck influence falls off along actual source edges from that boundary.
This avoids cutting the mouth or jaw with a horizontal neck-blend threshold.
The separate `JAW_MOUTH_SOURCE_WITNESS.json` retains the complete source IDs.
Production-model tests compare every emitted corresponding corner against the
actual Head matrix in combined attention/talk poses. Negative controls alter
real mouth and exterior-head weights and must be rejected.

The eye globes and irises are fitted to GhostLoop's actual morphed opening
rings. Lip/lid pigment centers are emitted from GhostLoop's own canonical
surface seams, not copied from another actor or taken from an internal mouth
joint. The independent source test reconstructs coordinates and UV corners from
raw OBJ and numeric target bytes without importing the producer. Regeneration
must reproduce both generated headers and reports exactly.

## Hands, clothing and hair

Each hand keeps 3,276 original Near triangles, source corners and the complete
20-edge wrist cycle. The original endpoint-quadric Far reduction locks that
cycle and retains 520 triangles per hand, with manifold, winding and finger-gap
checks. The restrained rest curl is static. Runtime fitting rotates the source
hand 51 degrees, mirrors the other hand with corrected winding and joins the
forearm to every exact wrist point. There are no new finger bones.

The original sewn-shirt method is independently fitted to GhostLoop's broader
shoulders and fuller waist. Its front neckline is lower than the initial
prototype and the raised collar lip is fitted at its actual height. The upper
yoke/collar follows the measured source neck surface influences. Shirt, yoke
and sleeves share physical nodes and seam weights; only the neck, hem and two
cuffs remain open.

The cyan vest is an actual lined shell with turned cut edges, a fitted front V,
complete rear/side panels and larger armholes. The horizontal identification
band follows two actual sewing rows and complete vest facets. That construction
corrects the prototype's Far strip drift caused by clipping and independently
interpolating skin weights. The band wraps the torso and remains ordinary
surface material rather than a light.

The trousers have a connected seat and a higher, properly fitted trouser break.
The low boots have an open ankle with turned cuff, a shaped vamp, separate sole,
welt, projected tongue and fitted laces. Cloth cuff, boot shaft and underlay use
one local ankle bend field; the shared rig and animation solver are unchanged.
This corrects the visible and strictly measured ordinary-run cuff/instep clash.
The retained full-crouch stress results are reported separately.

The chestnut swept crown is fitted to the actual source scalp. An opaque
underlying mass provides complete coverage, with an asymmetric forward sweep
and directionally fitted root/crown cards. Near and Far cards are projected onto
the actual rendered cap. The material remains a bounded isotropic hair
approximation, not strand simulation or a photorealistic hair shader.
