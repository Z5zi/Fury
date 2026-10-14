# Metro Watch runtime contract and planned evidence

Source inspection: accepted tree `9d424277e99ba37a8c66e77430512d6dc188ab40`.
The art pass must preserve the current construction in `apps/vaultline/main.cpp`:

- name `ExtraGuard`, display name `Metro Watch`, stable ID `NpcExtraGuard`
- kind Guard, presentation Security, height 1.82 m
- selected current vault V determines spawn `(V.x+3.5, 0.91, V.z+2.5)`
- waypoints `(V.x+3.5,0,V.z+2.5)`, `(V.x-2,0,V.z+1)`, `(V.x+1,0,V.z-3)`
- patrol speed 1.8 m/s, chase 4.0 m/s, initially chasing the actual camera/player
- one live instance; repeated spawn requests return while the alive flag is set
- clear removes the agent, hides the scene entity and parks it at Y=-80
- presentation identity retains its registered near/far meshes across respawn,
  rejects a changed role/height and resets per-instance animation state

`--npc-view NpcExtraGuard` and `--npc-motion NpcExtraGuard` call the real spawn
lambda. Neither option alone proves despawn/reset/repeated spawn. Runtime
proof must invoke the existing construction/cleanup, retain world scenery,
and inspect actual allocation, identity, visibility and chase state.

Source nuance to test without speculative gameplay edits: Approach invokes
`reset_run()` before conditional clear, so alive flags are already false at
that point. Normal Escape, Success and Failed transitions clear while the flag
is still true. This is a baseline ordering observation, not a verified ordinary
mission failure or an art-pass change. Navigation/chase clipping through solids
is an existing limitation, explicitly outside this pass's requested scope.
