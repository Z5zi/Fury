# Resumed direct pixel review

Review resumed 2026-10-07 04:20:32 UTC. This file records observations from
actual existing final-source images, not new render claims. Production source
fingerprint is d87b46e676d2152715d3cc83987fd5d8d9f7b15d6f3311aeaa67cd35acdc44ef.

## Body and closeups, 04:21–04:23 UTC

Opened Near material/clay eight-side body sheets, both LODs' material/clay
head sheets, Near/Far pose sheets, both attention closeups, high-resolution
three-quarter/side body images, cap top and under-jaw views. The lean uniform
silhouette, straight rear carrier, connected shoulder straps, turned collar,
continuous trousers, blank hardware and source-derived hands read coherently.
No carrier tab floats away from the shirt; no scalp hole or cut neck rim is
visible in the inspected top/bottom and turn views. Near eye rims and cheek/jaw
volume remain continuous through the two 0.75-radian attention turns.

The coarser Far cap silhouette and absent tiny cap badge are visible in a
closeup comparison, expected for its 16,000 triangle cap and 12 m switch. Face
geometry remains complete. Far elbow/knee curvature has fewer broad facets.
The Near cap seam tubes alias at this pixel size; their dotted appearance is
not an opening in the finite crown. This renderer/material stage does not
establish pore-level skin or strand-level hair parity with references.

Static crouch retains plausible calf volume. Full crouch-walk shows the shared
planted-foot/shin distortion/contact limitation that caused rejection of the
broader foot-weight workaround. The stress views remain in the evidence set;
no attempt is made to pose or crop that limitation away.

## Actual bank and motion pixels, 04:23–04:24 UTC

Opened before/after side, after front/rear, and actual motion samples 000,
006 and 011. The same fixtures, checkered floor and occluding room remain.
Metro's fitted vest/head/hands are clearer than the old slab/primitive mesh.
The motion frames demonstrate visible turning and advancing stride in the
real scene. They do not prove all mission lifecycle states; use the separate
12-cycle selected-job report for that.

A separate, pre-existing world-ground mismatch is visible: the authored
BankFloor plane is at Y=0.08 m (apps/vaultline/main.cpp), while the unchanged
NpcSystem ground_and_breathe sets NPC center Y=height/2 (engine/include/fury/
npc.hpp). Both the original and new meshes put their lowest sole at local
Y=-height/2, hence Y=0 in the world. The new lower toe box is mostly hidden by
that elevated plane in the bank standing views. The diagnostic flat stage
shows complete soles. No actor height, gameplay transform, floor or navigation
code was changed to conceal this; fixing world ground sampling is outside
this individual pass. Existing navigation-through-solids and scenery occlusion
also remain visible in the actual scene.

## Renewed reference comparison, 04:25–04:26 UTC

Opened all four pinned private reference images at the viewer's 2048-pixel
width. Jason Duval 02 supports the cap band/head relationship and gradual
jaw/neck volume; 05 and 09 support connected palm/thumb webs and neck/shoulder
continuity. Dramatic lighting, facial likeness, props, text and exact clothing
are not transferred. Kozlov's jacket turnaround shows broad quiet cloth panels
with folds localized near elbows and seams. Metro uses its own fitted shirt
under a carrier, so its broad vest panels remain quiet; sleeve folds are much
subtler than the loose commercial jacket. Hair remains an opaque original
short underlay rather than a strand groom. No copyrighted reference asset is
part of the production or handoff bundle. Corrected the stale private Jason 09
usage flag to match the documented and directly repeated study.

## Height extremes, renderer coverage and preserved stage behavior

The additional sixteen exact-source OpenGL captures show run, static crouch,
and both attention limits at 1.35 m and 2.10 m, in Near and Far. Their controls
are unchanged production animation samples. All sixteen were individually
opened. No scalp slit, rigid chin shear, missing wrist or floating carrier end
was found. Far static crouch has a visibly more angular knee than Near at this
close diagnostic distance; this is a coarse-LOD silhouette limitation, not a
claim of identical pixels or anatomy detail at both LODs. The same complete
Face is retained in both.

Opened both LODs in the saved CPU ray and software raster reports/images. All
four report the exact frozen source, correct triangle counts and zero renderer
validation errors. CPU-ray directional shadows and raster appearance differ;
this is expected backend behavior, not changed actor palettes. The actual GL
log identifies llvmpipe with shadows/reflections disabled, so these captures
make no native-GPU, DXR or game-performance claim.

The existing attention regression checked only omitted versus explicit-zero
controls; its old_default_compared fields were false. Closed that evidence gap
using the preserved pre-change characterlab binary: bind, idle, walk, talk and
crouch now compare byte-identical pixels and exact reported pose/camera/bounds
against the prior binary as well as zero controls. The new report has all five
old_default_compared fields true. The diagnostic's default actor is the
unchanged NpcCivA, isolating stage behavior from Metro's intended art change.

## Fully characterized retained crouch-walk limitation

The separate stress_extent companion includes the unchanged original test
predicates and gathers failures without changing the strict default gate.
Across 288 full-crouch-walk samples (two existing speed modes, 24 phases, three
heights, two LODs), 90 have a strict boot/lower-calf crossing and none report a
different contact class. This is broader than one isolated witness. Near at
1.82 m, mode 4, phase 3/24 remains the first default-test failure, triangles
14834/16873. The triangle-plane signed distances for that witness are stored
separately; they are millimeter-scale descriptors, not a claimed penetration
depth or continuous-time bound. The submicron renderer reset cache issue is
independent and is not an explanation for this visible stress limitation.

The complete twelve existing world motion frames were individually opened.
The carrier and shirt follow turning/stride together; the same fixture and
lintel occlusions remain. Standing/lowered toes can disappear behind the
0.08 m BankFloor while raised toes become visible, consistent with the verified
shared ground mismatch. Camera and all agent/crew state match exactly between
the static before/after side captures; profile differences are confined to
NpcExtraGuard's individualization, LOD setting and intended triangle counts.

## Native-size Far silhouette at 12 m

The Far static-crouch knee is visibly angular in magnified review, so six new
source-identical diagnostic images forced Near/Far at the existing 12 m switch
distance for bind, walk and static crouch. The 960x720 images were opened at
native size. The coarse knee remains a small visible silhouette difference;
cap/body/boot placement remains coherent, with no detached piece or missing
limb. This is accepted as a coarse-LOD prototype limitation, not identical
perceptual output. No sampling or anatomy was distorted to hide it.

A first global background mask was rejected because it counted distant
background/HUD variation; the rejected values remain explicitly labeled. The
corrected descriptive mask uses the union of the source-reported posed AABBs
projected through the actual 35-degree stage camera, plus two pixels of margin,
and a local background threshold. Near/Far silhouette-mask IoU is about 0.987
for bind, 0.982 for walk, and 0.952 for crouch at this fixed resolution/distance.
These numbers describe only these stills and are not perceptual quality gates
or proof of an automatic gameplay transition. The largest reported posed
AABB difference in the pair is about 2.91 mm (Far cap front in bind).
