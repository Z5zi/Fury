# Pax Wren — recoverable in-progress checkpoint

Snapshot requested at 2026-10-06 22:36:57 UTC. **This is not a finished pass.**
The dedicated work began 21:39:41; shared diagnostic tooling at
22:17:51–22:20:51 and this packaging interval are excluded from the minimum.
No two-hour completion is claimed.

Identity stays `NpcCivD` / Pax Wren / Dock / 1.72 m / seed `ec850fd2`.
The exact linear appearance is in `PALETTE.json`. Spawn, four-corner route,
DayOnly schedule, 2.2 m/s speed, dialogue, collider and missions are untouched.

Current source implements a separate pinned CC0 head recipe, connected source
hands with an invertible static rest curl and fitted 20-edge wrist loops,
a broad-waisted sewn rolled-sleeve shirt, full gray gathered/bun hair, shaped
trousers/boots, a fitted orange vest and the existing left cargo pocket.
All-source head corners/UVs and source-hand corners survive. No source program
logic or proprietary reference assets were imported.

The latest test run reports 39,776 Near / 15,912 Far triangles, inside 40k/16k.
Both LODs pass finite geometry, unit normals, anatomy/corner correspondence,
actual eye seating, 23k+ scalp samples with no cap penetration/uncovered skin,
233 animation/attention samples, and actual vest/shirt, hand/cargo and
opposite-knee contact checks. Coordinate-welded topology checks verify one
closed vest shell (Euler -2), four separate closed bands, two closed cargo
pieces, a single sewn shirt with four expected openings, and connected
hand/forearm surfaces with one 20-edge cuff boundary each.

**Pending correction:** the newly thickened reflective-look bands still cut
through their supporting vest in some poses: the test currently fails with
maximum 36 Near / 247 Far band/vest triangle crossings over 48 poses. This is
not waived. Earlier every-side images also show broken strips during gait.
The strips now follow actual support weights but still need geometry fit across
coarse support triangles. The current source-only recovery preserves this honest
failure and the independent tests that detect it.

Eight angles of body/head, clay/material, Near/Far; top/low views; and idle,
walk, run, crouch, crouch-walk and attention poses have been captured and
inspected in the private review. That review predates the latest shell-winding
and finite-band correction, so it is **iteration evidence**, not final proof.
Final recapture, live full-route motion, 12 m transition, CPU cost measurements,
full regressions, further critique and the actual two-hour minimum remain open.

Visual limits remain obvious: smooth wax-like skin, simple rigid cloth folds,
cap-like hair, simplified boots and a static hand rest. No finger animation,
cloth/hair simulation, blink/lip-sync/facial performance, GTA VI parity or
finished AAA art is claimed. GitHub and all external publication stay paused.

Diagnostic `characterlab` controls are a separate verified patch under the
parent's coordination. They change only the review stage and are excluded from
this Pax model/source patch.
