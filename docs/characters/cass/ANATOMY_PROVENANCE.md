# Cass Vesper: anatomical source and original fitting

## Reviewed graphical source

The independent Cass recipe uses only graphical data from
[MPFB2 commit d0a32e57a7f915cb2f2b95410e2117648c7bbb7e](https://github.com/makehumancommunity/mpfb2/tree/d0a32e57a7f915cb2f2b95410e2117648c7bbb7e).
The pinned upstream LICENSE.md distinguishes addon program logic (GPLv3)
from graphical assets, including the base mesh, targets and mesh metadata
(CC0-1.0). Both upstream license files remain in third_party/mpfb_assets.
No upstream addon program logic is imported, installed, copied or executed.

Cass's MANIFEST.json pins each input by full upstream path, commit URL, SHA-256,
Git blob SHA-1 and byte count. Existing pinned numeric graphical files are
referenced in place. No proprietary GTA mesh, texture, likeness, logo, garment
pattern or image is included as a game asset. Official screenshots remain
private reference material; the reference study records the observations.

## A distinct recipe, not a renamed existing face

The 17 weighted controls are explicitly recorded in cass_recipe.json. The
three macro target weights sum to one. Further controls separately address
oval breadth, chin width/projection, bilateral cheek prominence/volume, nose
depth/breadth, each ear's rotation and neck breadth/depth. The small left/right
cheek and ear differences are authored geometric choices.

Upstream control filenames describe numerical asset categories. They do not
establish Cass's age, ethnicity, gender identity, biography, affiliations or
wealth. The canonical Fence role, 1.78 m height, original linear skin/auburn/
teal/gray-green/mauve/dark-footwear palette and exact case-sensitive NpcFence
identity remain authoritative.

All deltas are applied before uniform normalization using the complete morphed
body's floor and crown. Source +X/+Y/+Z axes are retained. The normalized body
floor is -0.5, source crown is 0.493, and the remaining 0.007 is reserved for
original swept hair. Selected head/ear/neck geometry contains 4,378 source
positions, 8,696 triangles and 4,572 canonical UV coordinates. No selected
triangle is discarded. The source is one connected component with a single
58-edge attachment boundary, zero nonmanifold edges and consistent winding.

Both LODs retain this complete source selection. Original face/triangle/UV
indices and every source corner are recorded in the generated header and
checked against the actual runtime mesh. The shared Face atlas performs its
existing positive affine chart repack; Cass does not replace source UVs with
a cylindrical projection or modify the shared material/shader.

Eye centers and helper extents come from source graphical landmarks. Final
socket fitting is tested against the actual eye-opening boundary, independently
of the shipping dark lenses. Eyelid accents follow ordered source-boundary
arcs. They do not connect upper and lower corners by a mixed X-sort.

## Connected hands

The hand importer extracts one intact source hand, derives its local frame
from wrist/finger landmarks and applies Cass's explicit static rest fitting.
Length/width, lateral relaxation and depth/vertical curl are recorded in the
recipe. The minimum analytical vertical derivative is 0.76596875, retaining
an invertible rest warp. Both sides use the same source hand with a rigid
per-side fitting and mirrored winding. This is not a finger animation rig.

Each Near hand has 1,649 source positions and 3,276 triangles. The independently
authored Far endpoint-quadric reduction produces 560 triangles and retains the
20-edge wrist boundary. Link-condition, normal-direction, connected-component,
Euler and representative finger-section checks prevent branching, disconnected
fingers or an altered wrist. It does not claim a global Hausdorff-error bound.

Concave source thumb-nail folds retain source positions, UVs and triangles;
explicit rendering-normal splits avoid averaging through those sharp folds.
Each runtime forearm joins the exact anatomical wrist positions and weights.
The fingers, nails and thumb web are connected source anatomy, with a static
relaxed pose driven by the existing Hand joint.

## Original non-source construction

Hair, complete eyeglasses, jacket, rolled mauve facings, scarf, trousers and
ankle boots are original Fury shapes fitted to this source. Glasses remain
present in Near and Far. Their opaque dark lenses do not simulate refraction.
A clearly labeled --cass-no-glasses diagnostic is separate from the shipping
identity and may not substitute for shipping-glasses evidence.

The garment support fitter clips overlay triangles in XY against actual
support triangles to measure complete-facet clearance, rather than trusting
only sampled grid vertices. This is a static authoring operation, not cloth
physics, runtime collision response or a change to NPC controller behavior.

The forward exterior-jaw weights are fitted below the actual jaw envelope.
All 418 internal-mouth source positions share rigid Head motion, including all
484 UV corners. A separate 308-position exterior-chin regression rejects the
original weights. The scarf follows barycentric influences from actual source
neck triangles; the lower anatomical attachment remains unchanged. This is a
static attachment fit, not a new rig or facial-performance system.

The existing 17-joint shared animation system remains authoritative. The work
adds no expression blendshapes, facial performance, independent finger motion,
cloth/hair simulation or scanned skin detail. Validation covers finite sampled
poses; it is not a mathematical proof over every possible controller state.

## Reproduction

Run scripts/build_cass_anatomy.py --check and scripts/build_cass_hands.py --check
against the pinned inputs. Run the Cass model, renderer and independent runtime
dump/topology checks documented in GEOMETRY_REVIEW.md. The final validation
bundle identifies exact source and executable hashes; earlier fitting output
is not final-source proof.
