# Tessa Quill geometry and deformation review

Review date: 2026-10-06. Final model source SHA-256
`69441a74a30b6e5addad511a11f522a45848d63511fc1f00c753fdf7e9db61f0` passes the
expanded optimized and ASan/UBSan regression with the frozen HairCrop materials.
All eleven recorded source/test hashes were verified unchanged after the runs.
Numerical validation does not itself certify art acceptance.

Stable profile: `NpcCivC` / `Commuter`, roster height **1.65 m**, FNV identity
**`e78507f3`**. The tests call the real `make_character_profile` entry point and
skin its actual indexed vertices, never a proxy mannequin.

## Final geometry and CPU budget

| LOD | Vertices | Triangles | Width / height | Depth / height | Signed shell volume / height³ |
| --- | ---: | ---: | ---: | ---: | ---: |
| Near | 25,126 | 39,988 | 0.362679 | 0.205999 | 0.0197793 |
| Far | 9,933 | 15,976 | 0.362679 | 0.205956 | 0.0181615 |

Counts are identical at **1.35, 1.65 and 2.10 m**. Far removes **60.05% of
triangles and 60.47% of vertices**. Both LODs retain all **4,486 source
anatomical positions and 8,912 source head triangles**, represented by 4,683
atlas-UV vertices after preserving the source seams.

The unchanged **40,000 near / 16,000 far** limits leave **12 / 24 triangles** of
headroom. No cap was raised to make the result pass. These are local asset
budgets, not a whole-scene frame-rate or geometry-budget guarantee. The signed
volume sums overlapping shells and open panels; it is not a watertight boolean
union or a physical body-volume measurement.

The rig keeps its requested height. Soles touch `-height/2`, and the hair crown
is at `0.499 × height`: ground-to-crown extent is 99.9% of the requested value.
At the 1.65 m roster height, the crown is 1.65 mm below the nominal upper plane.

## Source, material and mesh contract

`tests/character_tessa_tests.cpp` checks near and far profiles at all three
heights:

- Every source position and ordered triangle retains its original winding.
  Every source UV corner receives the documented atlas mapping; duplicated
  UV seams retain identical smooth normals and weights
- Source head/neck landmarks drive the rig. Both eyes use the source helper
  volumes and follow the skull. The actual 28-vertex external-head/socket
  seams are independently recovered from topology, rather than treating
  internal helper joints as eyelid surfaces
- Rays through both socket rims and inward aperture spokes find actual eye
  geometry, with the interior samples visible in front of the source socket
- Finite vertices and colors, unit normals, positive triangle area, consistent
  bind winding, valid indices, bounded material values and atlas UVs, and no
  triangle interpolating between separate material regions
- Normalized influences, including optional third indices/weights; all 17
  joints drive real vertices and form an acyclic parent-first hierarchy
- Near/far appearance and rigs agree exactly; generation is deterministic and
  reuses shared immutable textures. Tessa retains her stable seeded identity
  while differing anatomically from generic bodies, Mira and Jon
- Both LODs retain outward-facing HairStrands cutout geometry and an opaque
  HairCrop cap; Hair remains on eyebrows. The shared material uses alpha
  cutoff 0.4 and no blending. Required surfaces are explicit per actor, so
  unrelated new atlas regions do not become requirements for Mira or Jon
- Pants, skin, rubber and metal shell edges close after UV seam welding.
  Leather intentionally includes open rolled flap sheets and is not required
  to form one closed manifold. Periodic copies are checked within each
  authored surface, preserving tint/weights and smooth hair-cap normals
- **180 generic fallback cases** preserve exact geometry, rig, influences and
  appearance across unaffected identities, wrong roles, case changes, suffixes
  and embedded-null names. Invalid height, role and LOD inputs are rejected

## Actual poses and shoulder regression

Idle, walk, run, talk/attention, crouch and crouch-walk are sampled at 33 phases
per clip: **198 standard poses per mesh, 1,188 overall**. Every standard pose
checks finite skinned vertices and unit normals, nonzero triangle areas,
bounded movement, grounded shoe vertices, exact UV/color/opacity preservation,
immutable bind geometry, stable topology/storage/capacities/renderer handles,
and no duplicate dirty revision for an identical pose. Pose sampling and
skinning record **zero per-frame C++ heap allocations**.

Proper triangle-crossing tests find **zero crossings in the audited knee,
wardrobe and anatomical shoulder regions** in these poses and the bind pose.
A further **180 dedicated shoulder/head-turn checks** sample yaw −0.8, 0 and
+0.8 rad with pitch ±0.25 rad at five talk phases, both LODs and all heights.

The late shoulder review added checks that were absent from the earlier
checkpoint:

- Actual coat triangles cover the retained lower-head shoulder flanges.
  A procedural fit at sampled grid points is insufficient by itself
- The anatomical outer shoulder does not cross upper-coat triangles, including
  coarse triangles extending below the fitting region
- Independently recovered torso and yoke patches meet with coincident positions,
  smooth normals and identical weights. A double-applied partial fitting blend
  had previously opened a roughly 0.98 mm near-LOD gap despite matching normals

The accepted construction distributes each angular shoulder support offset
smoothly through the upper torso. The yoke obtains vertical support before
querying horizontal anatomical clearance at its final height. Far upper-coat
samples receive 1% radial clearance to contain the retained flange between
coarse angular samples. The source anatomy was retained rather than cropped.
These are one-time construction operations; no pose-time fitting was added.

## Scope and visible ear diagnosis

The predicate detects proper non-coplanar triangle-interior crossings. Shared
edges and boundary contact are excluded. It is not a complete coplanar-overlap
or continuous-time collision proof, nor a certification of every possible
animation-input combination.

The wardrobe audit includes sleeve triangles below bind-space Y=0.255 and
actual hands. Within that region, authored sleeve/coat overlap above Y=0.18 is
excluded only from coat-versus-arm checks; bag/strap-versus-arm checks include
it. The additional shoulder audit checks retained Face triangles with centroid
|X| > 0.055 and Y < 0.345 against upper Jacket geometry influenced by torso
bones. It also probes the lower flange vertices and the entire torso/yoke
attachment. The remaining neck/collar styling still receives visual review.

Pelvis-only crotch attachments are excluded from each leg's knee-fold audit.
Bag/strap attachments and strap/coat contact are excluded from the wardrobe
crossing test; the strap remains checked against arms and thighs. These are
named authored joins, not a general allowance for penetration.

The small bright ear marks in the earlier raster captures were independently
ray-picked. For example, pixel (245,299) in the 480px front-right portrait hit
intact Face triangle 25616: geometric normal/view cosine +0.226, interpolated
normal/view cosine −0.230. The old two-sided shader flipped the smooth normal,
turning its sun cosine from −0.797 to +0.797. The next surface was approximately
0.99 mm deeper. This established a facing/shading artifact, not a missing ear
triangle or ordinary z-fighting. The software-facing correction and its pixel
validation are separate from this geometry regression.

## Final validation and reproducibility

The isolated Linux Release build passed **4/4** focused suites:
`character_profile` (Mira), `character_jon`, `character_skinning`, and
`character_tessa`. Tessa's expanded run took **18.40 seconds**. The exact same
model/test sources passed ASan/UBSan with no address or undefined-behavior
findings, including all 1,188 standard poses and 180 additional head-turn
checks. The instrumented build used `-O1 -g -fsanitize=address,undefined
-fno-omit-frame-pointer` with halt-on-error.

**Leak checking was unavailable.** LeakSanitizer explicitly reported a ptrace
incompatibility under this runner during an earlier checkpoint. The verified
run used `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Address and undefined-behavior
checks remained enabled; no LeakSanitizer pass is claimed.

Final SHA-256 inputs, verified unchanged after both runs:

| Input | SHA-256 |
| --- | --- |
| `engine/src/character_tessa.cpp` | `69441a74a30b6e5addad511a11f522a45848d63511fc1f00c753fdf7e9db61f0` |
| `engine/include/fury/generated/tessa_anatomy.hpp` | `4774159205e04d918bc875db5c937982322d2cb07a8cb15c3f82dc524c1efdc4` |
| `engine/src/character_surface.cpp` | `77e79c7ff8921593202f2389af038b24e10b8c469569f22408f3f6e572b62576` |
| `engine/include/fury/character_surface.hpp` | `74722dcd5c3b91a4fca70221b8655dd8265c1b5f00f07b04fca1a106ca68bdcc` |
| `engine/src/soft_backend.cpp` | `326b8deaafe1386e7b5fa1d14826c59852ec02a6314048779b3e26c84aa87aaa` |
| `tests/character_tessa_tests.cpp` | `62e56d1ad15ee0a2f158d40e259a8f4aa640bdad2227f19141e3a448bb79ec57` |

The shared sculpt helper, skinning, animation, and adjusted Mira/Jon test hashes
were also recorded and confirmed unchanged. The Tessa test passes clang-format
dry-run; direct whitespace checks include the new, untracked source files.
Rejected fitting candidates and earlier validated checkpoints are retained
separately and are not used to claim coverage of this final source.

Ordinary reproduction after configuring the repository dependencies:

```sh
cmake --build build --target fury_character_tessa_tests -j2
ctest --test-dir build -R '^character_tessa$' --output-on-failure
```

These focused checks do not replace the full repository suite, renderer
screenshots or the separate visual review.
