# PlayerBody integration boundaries

This study starts from immutable accepted-eight tree
`b7bb006f1748fbe4f0e6ce46a19a7631a6166e0a`. The original file manifest and
unmodified baseline executables are preserved in `player-body-evidence`.
The coordinator owns integration into the newer roster tree and publication.

The actor-specific production files are `character_player_body.cpp`, its
shirt/equipment helpers, two generated anatomy/hand headers, two original
data importers and `third_party/player_body_anatomy` recipes, pins and stats.
Their source dependencies already exist in the accepted tree and retain the
same pinned CC0 data bytes. No professional or Rockstar image is shipped.

The art's shared changes are one engine source-list addition and one exact
`PlayerBody` / `CharacterRole::Player` factory branch. Merge these two small
deltas into the coordinator's current files, preserving other roster passes.
Do not overwrite the coordinator's factory or CMake file from this older tree.

The separately reviewable input driver adds a null-by-default Application
InputState hook immediately after polling, a read-only presentation state
accessor, and the bounded `--player-input-audit` command. Its implementation is
in `apps/vaultline/diagnostics/player_input_audit.inl`. With the flag absent,
the original player input, controller, camera, collision and visibility paths
remain ordinary. It makes no direct actor/player position assignment. This
driver is needed for reproducible production-frame-chain evidence, not for
the art itself. See `PLAYER_RUNTIME_PROOF.md` for its limits.

New tests cover exact source reconstruction, all source head/mouth transforms,
connected hands, garment topology, posed area, allocation/storage invariants,
fallback dispatch, production input ordering, read-only state access and all
three rendering backends. Existing generic-fallback test inputs now use an
unregistered Player identity because PlayerBody intentionally has its own
profile. The original assertions remain. The strict full crouch-walk contact
gate remains registered and failing; ordinary contact predicates are unchanged.

The unchanged source hashes cover `character_animation.cpp`, `camera.cpp`,
`input.cpp`, `collision.cpp`, the actor roster and crew controller. No toe-pivot
experiment or shared gait redesign is part of this handoff. No source or
executable is mislabeled as the newer coordinator roster.
