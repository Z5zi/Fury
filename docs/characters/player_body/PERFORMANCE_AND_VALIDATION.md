# PlayerBody measured cost and validation

All measurements use CPU affinity 8, one heavy process and one build/test job.
The GL renderer is SDL offscreen llvmpipe LLVM 19.1.7, 256 bits. The CMake
OpenGL development-package warning did not prevent actual GL creation or pixel
readback. These are cloud software-renderer measurements, not GPU performance.

The final iteration-26 microbenchmark is retained in
`final-checks/benchmark.log`.

| Mesh | Vertices | Triangles | Pose mean | Pose p95 |
|---|---:|---:|---:|---:|
| Original generic Near | 2,200 | 2,284 | 0.0649 ms | 0.0582 ms |
| Individual Near | 21,328 | 35,880 | 0.7689 ms | 1.0030 ms |
| Original generic Far | 1,026 | 1,076 | 0.0242 ms | 0.0219 ms |
| Individual Far | 9,373 | 15,994 | 0.3462 ms | 0.4728 ms |

Each pose measurement uses 16 warm-up plus 256 recorded samples of production
skinning, with the saved earlier production sprint state and varied phase.
This is a CPU deformation microbenchmark. It does not measure whole-game FPS.
Construction was 890.0 ms for the first individual Near profile and 75.2 ms
for Far after shared initialization. The generic profiles cost 0.231/0.035 ms.197/0.050 ms.
The first individual construction includes shared surface initialization;
these measurements must not be described as pure per-actor cold costs.
Shared texture storage including mips is 37,746,432 bytes, reused by this actor.
The actor adds no new texture allocation. Near/Far remain under 40k/16k.

The paired full-world 300-frame runs at 960×640 use the same input driver,
original spawn, scenery and fixed 30 Hz offline simulation. Baseline wall time
was 76.039 s and CPU time 75.942 s; candidate wall time was 78.577 s and CPU
time 78.459 s. The measured CPU increase is about 3.3% for this populated run,
including per-frame framebuffer readback and raw PPM writes. PNG conversion
was measured outside the world run. The world-run RSS field is a cumulative
child maximum including prior compilation, and is not isolated process RSS.

All 300 player input, camera/controller, animation and visibility records are
exactly identical across the model-only pair. Final NPC, crew and heist state
files are byte-identical. Profile/cadence counters differ: Player triangle and
LOD metadata change as intended, and aggregate pose updates are 3404 versus
3407 with deferred updates 1631 versus 1628. These counters are not part of
the claim of player controller or final simulation-state equality.

The preceding complete Release CTest on iteration 24 passed 62/63 in 433.32
seconds. Iterations 25 and 26 change the lowest cuff blend and restore the original
closure/waistband/buckle cues. The final source repeats
all eleven affected character, source, input and presentation checks; exact
counts and timings are in `final-checks/RESULTS.json`. The one
retained failing scope is the
explicit full crouch-walk strict contact sweep: 84 sampled failures across
three heights and both LODs, retaining ankle and shirt/raised-thigh witnesses.
The complete ordinary contact sweep, including static crouch and both head
attention directions, passes with the same geometric predicate and thirteen
injected actual-triangle controls. Full source and reproducibility tests,
1,188 production pose samples, generic fallbacks, skinning/storage, input
ordering, software rendering, CPU ray rendering, actual GL rendering and all
pre-existing regressions pass. No test timeout was raised and no strict test
was removed. This is not an all-contact or whole-suite green claim.

Initial sanitizer construction uses 43 read-only cached objects only after checking
compiler identity/version, normalized flags and every transitive dependency.
Recorded source hashes are compared against the private tree; physical system
headers are hashed and must predate the cached objects. Five mismatches are
compiled explicitly: current build fingerprint, Application, factory, original
accepted-eight Jon, and PlayerBody. New test units also compile privately.
The final cuff-only rebuild independently rechecks those dependencies and the
private objects, compiles the new fingerprint and PlayerBody unit, and links
fresh binaries. Application, factory and original accepted-eight Jon may reuse
the already compiled private objects only when every dependency byte matches.
The complete dependency records, object hashes, test source hashes, link maps
and binary hashes remain in the sanitizer manifest. No cache file is changed.
LeakSanitizer is unavailable under ptrace and is disabled; only ASan/UBSan
results are claimed. Final per-command results remain in the evidence manifest.

Historical cache limitation: the original 43 cached objects did not retain
contemporaneous hashes of all external system/SDL/compiler inputs. This pass
hashes the current physical dependencies, verifies recorded project hashes,
compiler version/flags and earlier dependency timestamps, but cannot reconstruct
those historical external bytes with certainty. The three private reused
objects and new test objects do have full dependency hashes from this study.
The observed ASan/UBSan results are therefore explicitly cache-assisted, not a
fully fresh sanitizer build. At 09:30 UTC the coordinator reported fresh cumulative Player ASan/UBSan
profile, ordinary/strict, raster/path, actual GL and input checks completed
without diagnostics; that independently fresh cumulative evidence closes the
historical uncertainty for integration. This private result remains correctly
labeled as cache-assisted. LeakSanitizer remains unclaimed.

Final iteration-26 affected CTest is 10/11 passing in 44.32 s; the same strict
full crouch-walk gate retains 84 geometric failures. All final ASan/UBSan
profile, ordinary contact, software/CPU-ray/GL and input runs exit 0 without
sanitizer findings; the retained strict gate exits 1 with geometry witnesses
and no sanitizer finding. There are no unfinished private checks.
