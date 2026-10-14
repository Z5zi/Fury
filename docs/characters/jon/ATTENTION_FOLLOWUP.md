# Jon: attention and Far-hair follow-up

7 October 2026. Actual pixel review caught two visible defects after the original
individual pass: lower-lip shear during head attention and pale scalp patches
through the coarse Far hair shell. Both are corrected locally. The original
2-hour allocation remains in PASS_01.md; this follow-up is not counted again
against another actor's work.

## Changes and visual evidence

The 38 canonical lip-seam positions have 77 runtime UV corners. Their old Head /
Neck blend caused up to 8.10907 mm deviation from rigid Head motion and 3.07656 mm
change in a lip-pair distance at Jon's 1.70 m height. The blend now completes
below the mouth: its normalized span changes from .044 to .022, retaining the
.355 start. Source positions, source UVs, anatomy, rig and lower shirt attachment
are unchanged. [Matched attention pixels](../../images/characters/jon/attention-before-after.png)
show the corrected mouth/jaw shape.

All-side Far review then exposed a pre-existing shell-fit defect. The unchanged
original executable reproduces its pale scalp patches. The Far opaque crown
uses 19×7 cells instead of 16×6, with .0025 normalized radial clearance for its
coarser chords. This adds 80 triangles and 47 vertices, giving **15,992 triangles /
9,842 vertices**, within the original 16,000 cap. Near hair is unchanged. The
shared atlas, seeded palette, height, gathered knot and original hairstyle remain.
[Far before/after](../../images/characters/jon/far-hair-before-after.png),
[Near all sides](../../images/characters/jon/attention-near-all-side.png), and
[Far all sides](../../images/characters/jon/attention-far-all-side.png).

The Far silhouette is still coarse and cap-like. Skin remains smooth, cloth
rigid and fingers simplified. These corrections do not establish commercial
character-art parity.

## Verification and limits

- A new independent lip test derives the seam from canonical outer-head and
  mouth charts and checks all 77 corners in nine attention samples at three
  heights and both LODs. Measured corrected rigid-Head error is zero; injecting
  the original weights reproduces the error and is rejected
- An independent horizontal-ray test samples 527 actual source scalp vertices
  and triangle interiors above the authored hairline/ears. The old Far shell
  exposes 22 upper-crown samples in the first failing case. Both corrected LODs
  pass at all three heights; a shrunken-hair negative control is rejected. This
  is upper-crown coverage, not a claim that ears or the exposed forehead should
  be covered in hair
- The complete pre-existing Jon geometry and 1,188-pose checks still pass
- A separate 210-pose neck/shirt/apron comparison preserves all 11,340 ordered
  existing neck/shirt crossing events and introduces no apron crossing. Existing
  hidden lower-neck seam contacts are not claimed absent
- A 36-frame production walk/talk run at 12 Hz preserves the complete audited
  gameplay state exactly. Its cadence is an offline simulation setting, not
  measured real-time performance
- The final lip-and-hair source passes ASan/UBSan across all heights/LODs with
  unchanged source hashes. LeakSanitizer is unavailable under the traced executor
  and is not claimed clean. The first launcher did not execute because the
  optional timing utility was absent; the corrected Python launcher completed
  the actual sanitizer test successfully in 40.785 seconds

The earlier lip-only cumulative run passed 55/55 suites. The subsequent ten-actor
cumulative run, including this final hair fix, passes 59/60 suites with no skips;
its sole failure is Hale's separately documented strict full-crouch-walk contact
check. Neither that assertion nor Jon's regression predicates were weakened.

[Source, logs and sanitizer record](../../validation/characters/jon/followup-20261007/HAIR_FINAL_SANITIZER.json)
identify the final isolated follow-up fingerprint
`928472e041cb2a18102bbabade638d162254763ad52a02bbc472f4a9d34807c0`.
The cumulative roster build has its own fingerprint and fresh integrated evidence.
