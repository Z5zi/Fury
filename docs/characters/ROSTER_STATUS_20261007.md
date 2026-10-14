# Individual character progress — 7 October 2026, 09:34 UTC

**All 18 individual art allocations are complete and all 18 models are integrated, with the limitations below.** The final cumulative Release suite has 99 passes, 12 retained geometric failure gates and zero skips. This is a development checkpoint, not a release or an all-green quality certification.

| Character | Two-hour individual allocation | Current result |
| --- | --- | --- |
| Mira Vale | 2h conservative | Integrated and reviewed |
| Jon Keel | 2h | Integrated; later lip/Far-hair correction reviewed |
| Tessa Quill | 3h02m (includes active QA/evidence) | Integrated and reviewed |
| Owen Pike | 2h conservative | Integrated and reviewed |
| Lia Merrow | 2h07m56s | Integrated; counter/visibility repair reviewed |
| Nell Ash | 2h48m07s | Integrated; approved static route detour disclosed |
| Pax Wren | 2h 00m 47s | Integrated and reviewed |
| Rina Holt | 2h 01m 54s | Integrated; inherited obstructed route disclosed |
| Sgt. Hale | 2h 01m 26s | Integrated; strict full-crouch contact failure retained |
| Ofc. Vale | 2h 00m 15s | Integrated and reviewed |
| Metro Watch | 2h conservative | Integrated; strict full-crouch contact failure retained |
| Cass Vesper | 2h 08m conservative | Integrated; strict limits retained |
| Ofc. Renn | 2h 00m 26s | Integrated; strict limits retained |
| Rook | 2h conservative | Integrated; run/crouch/ground limits retained |
| Sparrow | 2h01m | Integrated; hem corrected; run/crouch limits retained |
| Syndicate Enforcer | 2h00m22s | Integrated; actual spawn/pursuit proof and strict crouch failure retained |
| PlayerBody | 2h07m44s | Integrated; upright cuff fit and original wardrobe cues restored; live crouch limits retained |
| GhostLoop | 2h00m37s | Integrated; received UDP lifecycle/model comparison and strict crouch limits retained |

The dated ledgers exclude known outages, passive waits and shared coordination/packaging intervals. Tessa’s longer recorded interval explicitly includes active QA and evidence preparation; conservative entries claim only the verified two-hour minimum. Parallel worker hours are not added to another character’s allocation.

Fresh cumulative ASan/UBSan compiled every selected source and system-header dependency without reusing any object. Sixteen scopes pass; three strict contact scopes retain their geometric assertions, with no sanitizer diagnostics. LeakSanitizer is unclaimed. The normal Release build remains separate from the instrumented shared-engine diagnostic configuration.

Final Player evidence has 64 exact private-final/cumulative stage pixel pairs and 300 matched actual input/controller/animation/visibility states. Enforcer has real production spawn/pursuit proof; Ghost has received-UDP lifecycle/model proof. The final 18-actor preview uses newly captured native pixels from the exact cumulative runtime. NPC runtime passes in 205.76 seconds under the original 240-second limit.

Known limits include Rook/Sparrow actual-speed run contact, full-crouch/transition cloth and footwear intersections, a strict cached-seam threshold, small ground crossings, existing navigation clipping and simplified skin/hair/cloth. These designs use CC0 anatomical bases. They remain below the GTA VI reference target. Public writes remain paused.

[Final verification and evidence](../validation/characters/integrated-eighteen-20261007/README.md) · [Shared limits](SHARED_MOTION_LIMITS.md)
