# NPC gameplay preservation and regression coverage

The character upgrade keeps the actual Vaultline roster and gameplay systems.
`fury_npc_gameplay_tests` exercises the production factories, movement systems,
nameplate/dialogue selector, heist state machine, and Meridian world/security
controllers. It is a deterministic C++ controller integration suite, not a
claim that an interactive player completed the game through the UI. The companion
`npc_runtime_tests.py` launches the actual full-world application for CPU-rendered
portraits, live patrol sequences, and presentation/gameplay separation checks.

## Content authority

`apps/vaultline/npc_roster.cpp` is the single production source for the twelve
regular NPC definitions and the two crew definitions. Both the application and
the tests consume those factories. The test does **not** create a substitute
civilian/guard/fence roster.

The twelve regular NPCs are Mira Vale, Jon Keel, Tessa Quill, Sgt. Hale, Lia
Merrow, Ofc. Renn, Owen Pike, Ofc. Vale, Nell Ash, Pax Wren, Rina Holt, and Cass
Vesper: eight civilians, three guards, and one fence. Crew are Rook and Sparrow.

Independent expected fingerprints were generated from the spawn blocks in
baseline commit `b2a695b7`, before extraction. They pin each regular NPC's internal
ID, display name, entity name, kind, schedule, initial position and yaw, home,
height, radius, walking/chasing pace, route index, every authored waypoint, and
color. Explicit crew assertions pin their corresponding identities, positions,
heights, follow offsets, pace, active state, and colors. Expected text
fingerprints independently preserve every original Q bark and phase-banter line.
No expected value is computed from the factory being tested at test time.

## Automated cases

- **Authored content:** exactly twelve regular NPCs and two crew, with original
  unique identities, role counts, routes, schedules, colors, and text
- **Actual patrols:** every production regular route runs for 240 simulated
  seconds at both 30 and 60 Hz; each completes at least two circuits in authored
  order, remains finite and grounded, and retains its original route and identity
- **Schedules:** three repeated day/night/day cycles per frame rate; seven
  daytime civilians plus Cass go off duty, while Mira and three guards remain;
  sleeping agents freeze and cannot chase; night watch preserves the original
  52% patrol compression and 1.45x pace, then restores the full day route
- **Chase and distance culling:** only guard/enforcer kinds pursue; far ordinary
  NPCs remain stationary, active threats bypass the distance cull, and the
  production bank guard resumes its pending authored waypoint when chase ends
- **Crew:** real Rook/Sparrow definitions follow at all four cardinal camera
  headings, remain grounded at their individual heights, stop outside active
  phases, and preserve the original 0/1/2-nearby-crew loot multipliers of
  1.0/1.175/1.35; inactive members contribute no bonus
- **Nameplate/Q selection:** every production regular NPC can be selected from
  eight camera headings with the original identity, speaker label, and role;
  facing away, excessive range, coincidence, and off-duty state are rejected;
  Rook/Sparrow retain individual speaker labels, and the dynamic enforcer kind
  uses the guard dialogue role
- **Dialogue content:** all 24 role barks remain reachable in one-to-three-line
  rotations and all 24 phase-banter lines remain reachable with named crew
  speakers; both sets match the original text exactly
- **Security integration:** the production-spawned extra Meridian camera's cone
  raises visibility/heat, starts the named guards' investigation without any
  teleport or route/home/index mutation, approaches through speed-bounded actual
  travel, escalates after 2.5 seconds, and locks the physical lobby gate; after
  chase cancellation the original pending patrol resumes; separate badge and
  terminal cases immediately release both controller and security-network
  lockdown; the registered badge breaker disables Meridian camera heat
- **Real Meridian controller flow:** entering approach range, an out-of-range
  breach attempt, leaving/re-entering approach, actual crew travel, an accepted
  breach interaction, the real breach timer, a lock-jam pause, accelerated loot,
  escape, extraction payout, and reset; alarm relocations, aftermath/shutters,
  escape blockers/response vehicle, traffic pace, and vault/gate release follow
  the actual controller transitions
- **After a job:** payout occurs once, carried bags clear, all five relocated
  NPCs restore their original spawn/home/day-route/pace, and subsequent schedule
  cycles preserve the restored patrols
- **Failure:** leaving during breach fails without payout; a second real
  advancing breach/loot run fails from camera detection plus guard heat, clears
  carried loot, and restores the five authored patrols after reset

The mission case calls `HeistController::update` and waits for real timers. It
never calls `apply_net_sync`, never writes an artificial heist phase, and never
calls `WishlistController::apply_world_state` directly. `sync_from_heist` drives
its production world-state changes. The security alarm boolean is supplied as
an integration input to verify lockdown release during the real escape phase.

## Gameplay defects caught during the character work

1. **Look direction for talking:** the old nameplate/Q loop used
   `(sin(yaw), cos(yaw))`, while `Camera::forward` uses `(cos(yaw), sin(yaw))`.
   A directly viewed NPC could be missed. The shared production
   `find_npc_talk_focus` uses the camera convention and preserves the previous
   5.5 m range, 0.55 look-dot cutoff, ranking, role mapping, and nameplate fill
2. **Lockdown state disagreement:** badge/terminal interactions cleared the
   Wishlist flag but left `SecurityNet::lockdown` set. The interaction now takes
   the security network explicitly and updates both; escape also clears the
   controller flag, with network state reconciled by the security tick
3. **Patrols after reset:** pre-heist restoration covered the desk guard,
   customer, and teller but omitted the bank guard and alley officer. All five
   mission-relocated agents now restore from the shared authored roster
4. **Relocation animation signals:** mission teleports clear measured movement
   and integration speed without adding traveled distance or resetting stride
   history, so the character animator does not turn relocation into a walk step
5. **Guard investigation teleport:** the first real follow-camera regression
   exposed the desk guard teleporting onto the sighting/player position and
   replacing its authored patrol. This could put the camera inside the guard.
   Cone hits now set chase/target only; normal bounded locomotion brings the
   guards to the sighting while preserving their patrols. Main honors that
   investigation before the movement update, and a post-job PreHeist reset
   clears stale investigation/escalation flags. The 2.5-second escalation and
   deliberate mission alarm/escape relocations are unchanged

These are targeted repairs. Mission phase rules, patrol authoring, original
names and dialogue, payout rules, and normal alarm/escape behavior remain intact.

## Reproduce

From a configured CPU build (adjust the build directory to your environment):

```sh
cmake --build build --target fury_npc_gameplay_tests --parallel 2
ctest --test-dir build -R '^npc_(gameplay|runtime)$' --output-on-failure
```

The test uses the null audio backend and does not require a display or GPU. Its
many per-frame predicates are checks within the cases above, not hundreds of
thousands of distinct test scenarios.

## Validation result (2026-10-05)

- Warning-enabled optimized standalone build: **passed, 780,461 predicate checks**
- AddressSanitizer + UndefinedBehaviorSanitizer build: **passed**, same checks,
  with leak detection disabled for the sandbox runtime
- Four deliberate temporary regression variants: **all rejected** at the
  intended checks: old camera-axis selection fails on Mira; unsynchronized
  lockdown fails at badge/terminal bypass; omitted bank/alley restoration fails
  the original bank-guard patrol check; restoring alert teleportation fails the
  immediate position/home/route/index preservation check
- `git diff --check`: passed for these changes
- Final real-application runtime: **passed**, 38 launches, 22 captured views,
  16 malformed-input rejections, and 38 distinct ephemeral localhost UDP ports
- Final runtime executable SHA-256:
  `60b796ba4d176d4fe8e7c1194093fcd81506f6922719d675e26b34f4a29c6560`
- Runtime test-script SHA-256:
  `b3f8e5e7c47877e4df149b15f6a70c550f60a66aab8042c2dc0a9b546ca7e5d0`
- Machine-readable evidence:
  `out/npc-upgrade/runtime-regression/npc-runtime-summary.json`

The independent standalone builds compile the current tests, roster,
interaction selector, Wishlist controller, and NPC movement implementation,
linking the configured engine archive for unchanged engine components. The
sanitizer result applies to those newly compiled translation units; it is not a
claim that the entire engine archive was instrumented. The coordinated full
application build and aggregate CTest results are reported separately.

## Actual application runtime regression

`tests/npc_runtime_tests.py` launches `vaultline`, builds the real city and actors,
and reads renderer-owned PPM files, detailed-character profiles, and the real
agent/crew/heist state sidecar. Every launch has a fresh private working
settings/save directory, links the production assets, and establishes its own
ephemeral localhost UDP host/client. The test checks the selected port in the
application log, keeping concurrent CTest/capture runs isolated. User saves in the
source/build directories are never loaded, changed, or deleted.

Runtime cases cover:

- Frozen legacy/detailed pairs for Mira, the bank guard, Cass, Rook, and the
  dynamically spawned Syndicate Enforcer, with exact actual-state equality and
  visible central-portrait pixel differences
- Identical repeated/default-on frozen results across fresh processes
- Valid orbit changing camera framing while preserving physical state/profile
- Valid extra-guard, player-body, and network-ghost portrait IDs, with exactly
  16 ordinary detailed profiles or 17 when a dynamic threat is explicitly spawned
- Eight-frame bank-guard and Mira live runs at a fixed offline 12 Hz cadence,
  with real positive travel, unchanged authored routes, and identical complete
  state sidecars between legacy and detailed rendering
- Exactly numbered renderer frame sequences, a final still matching the last
  sequence frame, temporal pixel changes, finite real renderer measurements,
  expected frame counts, and zero renderer validation errors
- A Rook talk-gesture capture changing pixels while keeping actual state
  identical and the idle crew physically stationary
- Clear rejection of malformed actor IDs, missing bounded-run arguments,
  unsupported player motion, conflicting photo/live modes, bad numeric capture
  settings, and invalid detail-toggle values

`--npc-talk` invokes presentation only. This case does not claim to synthesize
Q, open dialogue through SDL, or verify HUD text. The C++ test independently
exercises the real nameplate/Q selection helper and all authored bark strings.
Likewise, the initial crew's live capture is idle because no heist is active;
actual heist following and proximity bonuses are covered in the controller test.

Reproduce independently, using an **empty** evidence directory:

```sh
python3 tests/npc_runtime_tests.py --vaultline build/apps/vaultline/vaultline \
  --source-root . --output-dir out/npc-upgrade/runtime-regression
```

The CMake/CTest name is `npc_runtime`. Software captures use 320 x 180 pixels,
4 frozen or 8 live frames, dummy SDL video/audio drivers, null audio, at most
two CPU workers, and one application process at a time. Wall time includes
startup and shutdown and is not reported as FPS. The summary records the exact
executable and test-script SHA-256 values, every command/result/UDP port, frame
hashes, renderer-owned counters,
and numeric image differences.

The original failing investigation-camera frames/state/log are retained under
`out/npc-upgrade/investigation-regression-before`, explicitly labeled pre-repair
regression evidence. They are not a final character showcase.

## Scope and evidence limits

The C++ controller suite does not render characters, synthesize SDL keyboard input, traverse
city collisions/interior doors, verify actual HUD pixels, hear audio, exercise
multiplayer, drive getaway vehicles, or run the random reinforcement/enforcer
spawn/despawn lambdas in `main.cpp`. Synthetic guard/enforcer fixtures cover kind
eligibility; the real twelve-NPC and two-crew factories cover authored content.

The C++ mission fixture uses the real Wishlist-spawned props, actual extra-camera
anchor, registered security, and production controllers, plus a minimal bank
vault-door entity and a traffic vehicle for state assertions. It feeds player
positions and interaction inputs directly. Camera heat/guard investigation and
heist progression are tested in their own deterministic cases, rather than
claiming a fully navigated, heat-balanced user run.

`--smoke`, `--heist-capture`, and forced visual phase captures are presentation
checks. They cannot replace this timer-driven controller test, and this test
cannot replace actual runtime images/motion or an interactive playthrough.
See `NPC_LOCOMOTION.md` for movement signal semantics and numerical tests. The
separate character presentation report covers visual-quality review and larger
rendered evidence; pixel differences alone do not establish visual quality.
