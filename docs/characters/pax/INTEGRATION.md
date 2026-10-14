# Surgical integration of Pax

Apply the Pax source patch against the exact fixed Owen base tree
`9e21e9a1aae79fb7abd7a9cf3e177349581ef14d`, or transfer these surgical
changes into the later combined tree without replacing other actors:

1. Add `engine/src/character_pax.cpp`, the Pax shirt/vest headers, generated
   head/hand headers, Pax importers/capture scripts, third-party provenance,
   dedicated test and this documentation directory. Update only the Pax entry
   in the shared roster plan after combined-tree acceptance; the private copy
   intentionally leaves that shared planning file unchanged.
2. Add `src/character_pax.cpp` to the engine target.
3. Add the `make_pax_character_profile` declaration and exact `NpcCivD` / Dock dispatch in
   `character_profile.cpp`. Preserve other actors' declarations and dispatch.
4. Add the `fury_character_pax_tests` target and CTest registration, timeout
   600 and processors 2, to `tests/CMakeLists.txt`.
5. Remove only the obsolete generic `NpcCivD` / Dock fallback entries from
   character_profile, character_jon, character_tessa and character_owen tests.
   Preserve any parallel additions/removals for other accepted actors.
6. Reconfigure forcibly before building; copied source mtimes must not leave
   a stale embedded fingerprint. Re-run relevant combined-tree checks and
   recapture the final population after integration.

Characterlab diagnostic controls are a separate patch in the recovery
bundle: `apps/characterlab/main.cpp`, `docs/CHARACTER_REVIEW_STAGE.md` and
`tests/characterlab_attention_tests.py`. It adds crouch-walk and bounded
attention yaw/pitch while preserving omitted/zero control pixels/state.
Do not include that patch implicitly with Pax model ownership.

The private study does not include later Lia/Nell/Rina/Hale changes. It must
not overwrite the main repository, index or build. No shared shader,
material atlas, animation, AI, roster, route, schedule, collider, dialogue or
mission changes are needed by Pax. The generated anatomical headers depend
on the existing pinned graphical input directories already in the base.

The recorded stage source fingerprint includes the separately listed
characterlab controls. Reproducing that fingerprint requires both patches on
the fixed base; a later combined roster will intentionally have a different
fingerprint and needs its own captures.
