# Nell Ash, individual character-art checkpoint 01

Nell keeps `NpcCivAsh`, Market, 1.75 m, seed `d9d5f382` and the original
seven-color palette. The individual recipe gives her a longer/narrower face,
connected source-derived hands, a low rear bun, a sewn stone shirt, continuous
trousers and a teal wrap apron with a pocket, cross-back tapes and rear ties.
This is original Fury art over pinned CC0 graphical data. It does not claim
GTA VI fidelity or copy a proprietary likeness, garment pattern or asset.

Dedicated reference/model/source/pixel work began **6 October 2026, 20:16:29
UTC**. The genuine two-hour minimum was reached at **22:18:29**, excluding two
one-minute backup blocks. Geometry refinement continued through **23:04:36**;
that interval is 168 minutes 7 seconds, with four minutes of recovery packaging
conservatively excluded. Later validation, performance work and packaging are
recorded separately in WORK_LOG.md. No idle wait is used to satisfy the minimum.

## Geometry and evidence

| LOD | Vertices at 1.75 m | Triangles | Budget |
| --- | ---: | ---: | ---: |
| Near | 25,273 | 39,464 | 40,000 |
| Far | 10,119 | 15,872 | 16,000 |

The original generic Near model was 2,274 vertices / 2,328 triangles. Each new
Near hand retains 3,276 source triangles; Far retains 520, preserving the full
20-edge wrist and connected thumb/finger roots. The 8,800-triangle source
head/neck and original per-corner UV correspondence remain at both LODs.
Details and independent geometric checks are in GEOMETRY_REVIEW.md.

The review evidence includes front, back, both sides and all four obliques at
Near/Far; face and body framing; clay; hand closeups; 12 m forced-LOD views;
CPU raster/path and actual OpenGL llvmpipe; continuous idle, walk, run, talk,
crouch and crouch-walk; head-turn extremes; eight actual Ashcourt views; a real
70-second lap and live talk capture. Props, other NPCs and dynamic vehicles
remain in the recordings, including camera occlusions.

## Exact-source validation

The frozen art/audit source fingerprint is
`4ad563f4595b2f0ee8497d9e2c12e2716345f903d7e6071531e653fd67075d17`.
The delivery code fingerprint, including the separately approved world changes,
is `2b72f211854d0e7ec3ff29ee0f5bc07808e7e89c433897dc49838219233d3b2c`.
The only later rendering-input difference is conservative support-ray bounds
rejection during model construction. All six complete model dumps at three
heights/two LODs are byte-identical. Near/Far rear views, raster/path/llvmpipe
portraits and the actual Ashcourt control frame also have **zero changed pixels**.

- **38/38 selected raster suites passed**, including `npc_runtime` in 137.02 s
  under its unchanged 240 s limit. The earlier unculled construction timed out;
  the failed log is retained, not reported as a pass.
- **4/4 actual llvmpipe GL suites passed**, plus actual stage/world GL captures.
  This is software OpenGL compatibility, with shadows/reflections disabled by
  the engine's llvmpipe path; it is not hardware-GPU performance evidence.
- **ASan/UBSan passed** on the optimized source. Leak detection was disabled
  because LeakSanitizer cannot operate under this executor's ptrace environment.
- **Focused revised-route gate and all 11 controls passed**: 17 wrapped/spawn
  segments against 329 currently available solids, with minimum circular-body
  clearance 0.159999821 m. The stronger complete-scene and actual-posed-route
  certificate is supplied independently by the route audit.
- The older private `npc_gameplay` fingerprint check is intentionally excluded
  until the parent's separate route-test integration. It is not claimed as a
  passing full aggregate. INTEGRATION.md identifies the surgical handoff.

Construction CPU time fell from 7.018 / 1.140 s (Near/Far) to 2.278 / 0.409 s
without changing geometry. The final integrated paired CPU measurement is now
complete: Near stage raster 35.41 → 97.31 ms at 480×640; live-world raster
261.59 → 281.08 ms at 480×360. World ray variance overlaps, so no speedup is
claimed. [PERFORMANCE.md](PERFORMANCE.md) records all six conditions, process
memory, exact source/gameplay pairing and limits. The seven-character after
source includes Pax; Nell's six-character integration independently passed
42 raster/runtime/source suites and four actual llvmpipe suites. All prior
private evidence remains separately labelled.

The final continuous recordings retain all frames, including the
[70-second actual patrol](../../videos/characters/nell/revised-full-lap.mp4).
[Route repair scope](ROUTE_REPAIR.md) distinguishes the intentional longer
loop and prop changes from the character-art comparison.

## Remaining limitations

A separate independent full-deep-crouch probe finds same-leg calf/thigh cloth
self-contact at both LODs. This is distinct from ordinary patrol and the
passing apron/trouser and upper-cloth/shirt tests. The first witness's local
opposing-triangle plane depths are 4.02/0.89 mm Near and 12.93/8.62 mm Far;
these are not bounds on total penetration depth. Actual rear/side/oblique
closeups show compressed knee folds without a separate open hole; Far remains
angular. The frozen shape is retained with this explicit diagnostic limitation.

Hair remains cap-like with a coarse Far bun and one small grazing wrap-card
highlight, confirmed against actual cap/skin geometry and neutral-material
pixels. Skin is simplified and waxy; straps and long cloth remain angular/stiff
in some views. There is no cloth/hair physics, finger animation, facial
blendshape animation, blink or lip-sync. Static route clearance does not certify
raised dialogue hands or dynamic traffic avoidance. Existing paving/foot-height
mismatch is unchanged. Source images used for reference are excluded from all
source deliverables.

## Combined-source acceptance, 7 October 2026

The shared seven-character source now passes **47 registered suites** (43
raster/runtime/source and four actual llvmpipe), plus the diagnostic-stage
attention/crouch-walk checks and both Pax importers. No timeout was increased;
`npc_runtime` completed in 151.51 seconds under its existing 240-second limit.
Current runtime fingerprint:
`44e70ba992cdf7444e755e5b54b66706278d489cb85e3395db8fa5c519aa8e06`.

Fresh stage captures match the private accepted images pixel-for-pixel: four
Nell body/face/LOD samples and all six matched Pax samples. Eight integrated
Nell world angle pairs retain exactly equal gameplay-state sidecars, with the
same approved corrected route/world in both variants. Fresh Pax world front,
right and rear captures preserve real surroundings and are separately recorded.
Private and combined fingerprints are intentionally different; the actual
[combined capture manifest](../../validation/characters/nell/integrated-captures/MANIFEST.json)
records every command, source fingerprint and pixel check. These checks preserve
the visible limitations stated above and do not establish finished AAA art.
