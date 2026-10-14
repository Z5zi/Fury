# Owen Pike geometry and deformation review

Geometry review completed on 6 October 2026 (UTC) for the actual
`NpcBankCust` / Commuter profile and dedicated `character_owen` regression
target. The full optimized and ASan/UBSan matrices pass on the source hashes
below. Rendered stage, lobby, gameplay and renderer performance evidence is
reviewed separately. The final Far scalp correction is included in this result.

## Contract and independent checks

The stable identity is seed `0x485fc311`, height 1.74 m, and the existing linear
skin, auburn hair, eye, blue-gray jacket, olive-brown trousers, shoes and accent
palette. The test checks the exact reviewed palette constants as well as
bit-deterministic repeated construction. Both LODs preserve the same rig,
appearance, shared immutable material and individual switch distance. Wrong
roles, altered/case-changed/embedded-NUL identities and other unregistered IDs
must retain the generic model bit-for-bit. Mira, Jon and Tessa's existing
fallback fixtures replace only the now-registered commuter ID with
`unregistered_commuter`; their exact-model assertions remain intact.

The anatomical test independently maps every emitted Face vertex back to the
canonical source. It compares the entire ordered triangle stream, winding,
original per-corner UVs after atlas mapping, source position retention, seam
normals and skin weights in both LODs at 1.35, 1.74 and 2.10 m. Source anatomy is
4,368 positions and 8,680 triangles. Owen is compared with Mira, Jon and Tessa
for distinct anatomical bounds, rig and appearance, and with his seeded generic
model for distinct body geometry.

All five connected source UV charts are recovered independently: main head
6,232 triangles, mouth 796, neck 480, and two sockets with 586 each. Mapping
checks preserve signed UV area, isotropic chart scale and tangent orientation.
60,760 actual triangle corner/edge/interior probes cover the base level, every
retained mip, half-level trilinear filtering and clamping beyond the last mip
for base color, tangent normals and ORM. Each entire retained chart rectangle
is compared with a chart-only mip reference, and every bilinear support texel
must stay inside its own chart. The reference uses Fury's mip builder; this is
an independent chart-isolation check, not a separate proof of its filtering
arithmetic.

Actual eye apertures are recovered from socket/external-head topology. Every
rim point and four inward samples per spoke must have real globe/iris coverage;
inward samples must be visible in front of socket skin. The measured rim depth
is bounded to -0.003h behind / +0.002h ahead of the rim. Approximate rig helpers
are only a neighborhood check, never a substitute for actual lid seating.
Hair-card winding is compared with the nearest actual external source
triangle's normal and signed distance, rather than assuming a spherical skull.
Owen uses the separate short-card cutout material `HairShortStrands`; the
underlying cap remains `HairCrop`. The original actor material tiles remain
unchanged and are covered by the shared atlas regression.

## Opaque scalp coverage and the Far correction

Final all-side renders exposed a genuine Far scalp sliver that the original
strand-card checks did not cover. The new regression checks all actual
Face/HairCrop triangle pairs after AABB pruning, recovers the cap's open root
from welded edges, and ray-tests source-triangle vertices, edges and interiors
above that boundary. It also checks the cap's manifold edges and rigid Head
attachment. The relevant source scalp already has that same rigid attachment,
so their relative coverage persists through animation and head turns.

The exact pre-correction source is retained as a negative control: the new
C++ check reports 185 strict crossing witnesses and a minimum radial gap of
-0.00256954h. An independent inclusive triangle audit reports 200 witnesses
across 65 Face and 22 cap triangles, including lower rear cap faces. Its deepest
sampled source-vertex penetration is 0.002576137h (4.482 mm at 1.74 m).

The bounded fix adds 0.003h radial clearance to Far HairCrop through its
existing crown taper. It moves 160 positions by at most 5.220 mm at 1.74 m;
Near geometry, other surfaces, topology and exact crown height are unchanged.
The expanded persistent test now finds zero crossings and uncovered samples
at all three heights, with minimum gap 0.00038577h Far and 0.00058193h Near.
It probes 21,615 Far / 21,656 Near source points per height. The separate denser
review has zero failures among 774,040 horizontal rays, 21,633 source-triangle
samples and 16,380 exact/adjacent seam rays. Its axis/tolerances differ slightly
from the permanent C++ check; neither repeats the production cap formula.

The [cap audit, source hashes and unchanged-source negative control](../../validation/characters/owen/cap-correction/README.md)
record the evidence. Matched [face comparisons](../../../out/character-study/owen/cap-review/face-comparison.png)
show the removed Far sliver and byte-identical Near pixels. At the real
[12 m transition](../../../out/character-study/owen/cap-review/transition-comparison.png),
the Far silhouette change is small and no new visible hole was found.
The opaque cap intentionally ends above the strand transition; the test uses
its actual root rather than demanding opaque coverage below it.

## Garment topology and contact definitions

The principal Jacket component is recovered by welding exact physical
positions, independent of authoring order or private pattern IDs. Both sleeves
and the torso must form one connected manifold. Every internal geometric edge
must have two opposite triangle directions; no internal armhole cap or
nonmanifold edge is permitted. The only accepted free edges are the turned
hem, turned cuffs and open collar/neckline returns. All coincident chart corners
must preserve identical weights, and the actual armhole/shoulder seams must
preserve smooth normals. Pocket/collar fold corners may intentionally have
hard normals while retaining identical positions and weights.

The canonical 54-edge Face cutoff must pair with 54 oppositely wound Skin
continuation edges at exact positions, weights and smooth normals. The
continuation must be one connected manifold with only one lower 54-vertex
boundary. Every lower vertex must remain at least 0.003h below the actual
collar opening and inside the garment according to four transverse-plane rays
against actual jacket triangles. The plane follows the posed neck direction,
so breathing/crouching cannot turn a world-up assumption into a false exposure
report. These finite enclosure probes do not claim visibility from every
possible camera inside the open neckline. This checks a physical continuation
rather than accepting an exposed source cutoff or forcing the collar into the
neck.

Every duplicated sewn corner is checked after deformation. Actual triangle
crossings are audited at both knees, neck/collar, the complete sewn shoulders,
and between satchel/strap and arms/hands/thighs. Owen has no inherited
buried-sleeve-cap or upper-arm overlap exclusion.

A proper crossing requires an edge to pass strictly through the interior of
another triangle. Shared geometric vertices/edges, barycentric boundary
contact and coplanar contact are excluded; they are not claimed to be
penetration-free clearance. Strap-to-jacket and strap-to-bag contact is an
intentional carrier/attachment join, and bag components can meet one another.
Torso carrier panels inside |x| <= 0.10h remain torso for this contact audit even
when their underarm blend gives them substantial upper-arm influence; actual
lateral sleeve/hands remain the limb groups. Bag-to-torso crossings are still
checked. Pants pelvis/upper-thigh joins are outside the knee test. The knee test
covers leg triangles centered between -0.40h and -0.065h. These tests do not claim a
boolean-unioned whole character or exhaustive collision coverage of every
intentional layered surface.

The classifier has a permanent negative control: an actual central carrier
triangle with over 60% upper-arm influence is intersected by a synthetic strap
edge. Its intended carrier join is accepted; the former weight-only arm label
would incorrectly reject it. Ten further penetrations covering bag-to-coat and
bag/strap-to-both-arms/thighs must be rejected. Thus the intentional carrier
exception cannot silently erase the requested limb/contact audit.

The satchel flap has a separate layer test: two open nonperiodic Leather
patches are recovered from indexed topology, and mirrored-UV correspondence
pairs every outer/underside vertex. The actual offset must be 0.001h, both
sides must have opposing winding/normals with positive normal thickness, and
no layer triangles may cross. Both sheets must use the same uniform DQ
transform, preserving their relative geometry in animation. A deliberately
recreated historical bag-local-Z underside offset must fail the same thickness
gate on the rolled top. Its axis is recovered from the straight lower flap, so
this negative control remains valid after an authored bag rotation. Unlike
ordinary coplanar attachment contact, collapsed flap thickness is explicitly rejected.

## Side satchel and carrying bind

The accepted satchel remains on Owen's right side, centered at
(0.115, 0.100, 0.025)h and yawed 90 degrees around Y, with unchanged half extents
(0.036, 0.047, 0.020)h. Every bag/flap/hardware vertex uses the same uniform
72.1213% Spine / 27.8787% Pelvis influence. Dual-quaternion skinning therefore
keeps the bag rigid. Strap endpoints use the matching bag attachment and
rotated bag-local normal; the remaining strap follows fitted coat weights.

The right shoulder is fixed. The elbow shifts outward by 0.032h and wrist by
0.025h. Bind points follow segment-aligned transforms: radial offsets rotate,
longitudinal distances scale to the new segment endpoints, and unchanged skin
influences blend the displacement. Unaffected vertices receive no identity
transform arithmetic. Bind normals are rebuilt after this authored fit; the
controller and animation clips retain their existing semantics.

The upper segment turns 9.2895 degrees and scales longitudinally by 1.031025;
the forearm turns 2.3996 degrees with scale 0.994536. These small segment changes
do not bound the local strain of a blended mesh. Final-source Near bind edge
changes range from -18.484% to +27.368%; Far ranges from -11.012% to +26.904%.
Sampled dynamic Near edge ratios relative to the remapped bind range from
0.4986 to 2.1934, compared with 0.4414 to 2.0117 in the unretargeted baseline.
Far ratios range from 0.5350 to 2.0816, against baseline 0.5149 to 1.9580.
These measurements cover affected Jacket/Skin edges; the uniform DQ bag remains
rigid. Full final geometry, winding, seam and crossing gates remain in force.

The retarget exposed a real coarse-LOD inner yoke/collar crossing. The final
yoke fades arm influence through its outer half and leaves the inner yoke and
collar rigidly supported by Chest. Exact root seams remain matched. This fixes
the nonlinear bind-warp disagreement between the fitted collar and its
supporting triangle, without adding geometry or a collar-overlap exception.

The accepted neutral back-panel center gap is about 18.5 mm Near / 19.0 mm Far
at 1.74 m. Minimum corner/surface clearance is smaller and must not be called
the back-center fit. The earlier 3–8 mm target was a fit-study preference; the
reviewed hanging side satchel was accepted after front, side and rear renders.
The rejected forward-floating construction and closer variant with invalid
Far strap winding are not part of the final asset.

Final-source surface distances at 1.74 m, measured across bind plus all 198
ordinary poses per LOD:

| Clearance | Near | Far |
|---|---:|---:|
| Neutral back-panel center to coat | 18.493 mm | 18.976 mm |
| Neutral minimum bag/coat gap | 7.341 mm | 5.322 mm |
| Minimum posed bag/coat gap | 5.048 mm | 2.226 mm |
| Minimum bag/right-arm gap | 5.217 mm | 7.799 mm |
| Minimum bag/right-thigh gap | 4.973 mm | 7.469 mm |
| Minimum strap/right-arm gap | 14.834 mm | 15.659 mm |

Left-side and strap/thigh distances are larger. These are sampled distances
between actual triangle surfaces, including edge-to-edge distances, not a
continuous-time collision certificate. Exact limiting poses, triangle IDs,
source hashes, baseline-relative strain and reproducible diagnostic helpers
are in the [final carrying-fit evidence](../../validation/characters/owen/carrying-fit/README.md).

## Animation and resource contract

The final required sweep is six actual runtime clips (idle, walk, run,
talk/attention, crouch and crouch-walk), 33 phases including both endpoints,
both LODs and all three heights: 1,188 ordinary poses. Another 180 sampled head
turns cover yaw -0.8/0/+0.8 and pitch -0.25/+0.25. Pose sampling and skinning
must allocate no per-frame storage, preserve index/vertex buffers and capacities,
renderer handles and geometry identity, avoid redundant dirtying for identical
poses, retain exact colors/UVs/opacity, and keep the immutable bind mesh intact.
Every deformed vertex must be finite with a unit normal, actual sole geometry
must stay above ground, and no triangle may collapse. Adjacent sampled frames
must remain bounded without vertex jumps.

The provisional whole-asset triangle ceilings remain **40,000 near / 16,000
far**. Budgets are not enlarged to make a candidate pass. Source hashes are
captured for each build/run; a result applies only to that source snapshot.

## Verified result and exact source

| Measurement | Near | Far |
|---|---:|---:|
| Whole-asset vertices | 24,622 | 9,813 |
| Whole-asset triangles | 39,620 | 15,996 |
| Principal sewn garment triangles | 8,716 | 2,492 |
| Oppositely paired garment edges | 12,956 | 3,678 |
| Intentional garment opening edges | 236 | 120 |
| Tested exact seam copies, including neck continuation | 335 | 204 |
| Opaque cap triangles | 2,368 | 264 |

Both LODs have exact normalized height 1.0h and soles at -0.5h. Neither
40,000/16,000 ceiling was raised. At 1.74 m the actual eye-rim depth interval is
[-0.00223308, +0.00187058]h Near and [-0.00266665, +0.00159662]h Far. Actual flap
normal thickness minima are 0.000463444h Near / 0.000252964h Far; the full
vertex-to-vertex layer offset remains 0.001h. The recreated old Z-only flap
fails the positive thickness control, including a Near coplanar limit.

Final SHA256 values:

- `engine/src/character_owen.cpp`: `1ae91ff9558309c3a4930978b8acdfed3ad7cad296fb12c6d9d35872ff656027`
- `engine/src/character_owen_jacket.hpp`: `800652894e683f1882152f2a3b781acf2abfba717c44abba3117e40b7e4fb09d`
- `tests/character_owen_tests.cpp`: `f70d6e5504bc1cbb41c98e1ffa6590c7c38f2ad38cd9f8524c70fc5ac801eaac`
- `engine/include/fury/generated/owen_anatomy.hpp`: `cab56470673bf4db27b68bc44dd5dd3fefe107ec13a82f7b56db60518d8b0bb8`

The expanded optimized pass took 27.53 seconds, with 77,284 KB peak RSS. The
ASan/UBSan pass took 194.96 seconds, with 556,076 KB peak RSS. These are test
runner resource measurements, not rendering benchmarks. Both checked the
complete 1,188 ordinary poses plus 180 head turns, all chart/mip checks, generic
fallback, bind geometry and contact negative controls. Input hashes were
unchanged before/after each run, and link maps confirmed the intended profile
object was used instead of an older archive member.

Evidence: [optimized log](../../validation/characters/owen/profile-optimized.txt),
[optimized resource/source manifest](../../validation/characters/owen/profile-optimized-resource.json),
[ASan/UBSan log](../../validation/characters/owen/profile-asan-ubsan.txt), and
[ASan/UBSan resource/source manifest](../../validation/characters/owen/profile-asan-ubsan-resource.json).
The wider [CPU suite](../../validation/characters/owen/tests-cpu.txt) and
[offscreen OpenGL suite](../../validation/characters/owen/tests-opengl.txt)
are recorded by the integrated application review. A log applies to its
recorded source fingerprint; an earlier pass is not evidence for later edits.

Mira and Jon production geometry sources remain unchanged. Tessa retains her
anatomy, rig and topology; a separately documented correction backports the
same confirmed finite-normal flap-layer fix. The existing exact generic-model
fixtures remain strict after replacing the newly registered commuter identity.
The shared material regression preserves all old 12 tiles byte-for-byte across
three maps and seven retained levels; Owen's short-card tile is appended.

These tests establish the stated sampled contracts. They do not certify every
continuous animation time, every possible camera, Boolean-unioned wardrobe,
all intentional layered contacts, or physically simulated cloth. Renderer and
populated-scene performance evidence remains separate from this geometry test.

## Reproduction

After configuring the repository's ordinary dependencies:

```sh
cmake --build build --target fury_character_owen_tests -j2
ctest --test-dir build -R '^character_owen$' --output-on-failure
```

The executable's optional `--quick` mode checks both LODs at 1.74 m without
animation; `--static` checks static geometry at all three heights. Neither is
reported as a full deformation pass. The default invocation runs the complete
matrix. During iteration, independent contact categories retain their first
failure while the other categories and numerical animation checks continue;
a successful run has checked every category at every required pose.

Sanitizer verification uses a separate build with address and undefined-
behavior instrumentation and halt-on-error. LeakSanitizer is excluded only
because the runner's ptrace incompatibility was already established during
Tessa's review; no leak-checking pass is claimed. Allocation-mismatch checking
remains enabled, including matching ordinary and nothrow allocation-probe
overloads.
