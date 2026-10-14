# Owen rendered review notes

6 October 2026, UTC. The eight face and eight full-body views in the final
candidate were inspected from front, both sides, back and all four obliques.
This is an art review, separate from the geometry tests.

The face/neck now forms a continuous human-shaped volume with seated eyes,
readable lips, nose wings and ears. The narrower chin and longer profile differ
from the other completed characters. The sewn jacket removes the original
capped sleeve joints, and the side-facing satchel reads as an attached hanging
bag with a modest carrying stance. The collar remains seated through the
reviewed coarse walk sample after its support weights are corrected.

Visible limitations remain:

- Skin is pale/waxy under the neutral engine lighting, without the regional
  translucency, fine roughness and expressive soft tissue of the references
- The crop has a smooth cap-like mass and regular-looking boundary teeth;
  the whorl and side growth remain coarse despite the quieter material
- The broad jacket panels and sleeves are smooth and sculpted rather than
  convincingly draped; shoulder/collar edges remain angular in some views
- The strap has visibly angular direction changes across the shoulder and
  attachment transitions. Its measured fit/contact is improved, but it is
  not a simulated flexible leather strap
- The simplified trouser seat-to-thigh transition and knee creases remain
  readable construction boundaries rather than production cloth topology
- The rigid satchel back has roughly 18.5 mm neutral center clearance from
  the curved coat. Closer corners approach about 5–7 mm. This is much less
  than the rejected 73 mm floating-front version, but it is not full contact
  conforming deformation or a dynamic bag simulation

These are reasons to retain the images and continue future art iteration,
not defects made invisible by a passing automated suite. This is a first
individual checkpoint; no GTA VI parity or complete character-art finish is
claimed. Actual-game and backend review is recorded in PASS_01 after the final
capture batch finishes.

The forced Far portrait additionally exposed a tiny light scalp sliver at the
upper oblique crown. This was an actual coverage defect, not ordinary coarse LOD detail. The
Far-only conservative cap envelope now removes the sliver and passes the
expanded crossing/coverage regression; Near pixels remain byte-identical.
The close and 12 m before/after sheets were inspected before final sign-off.

The actual lobby CPU path image accumulated128 samples/pixel at 320square, two
bounces, with four 32 sppframes. It remains darker and visibly grainier than
raster/OpenGL; one such CPU accumulation frame cost about 16.43 seconds. The
cleaner32-sample diagnostic-stage portrait uses different authored lighting
and cannot replace the real interior result. OpenGL uses the engine's llvmpipe
mode, which disables directional shadows/reflections and uses FXAA. No equal
lighting, feature parity or physical-GPU speed is inferred from these captures.
