# GhostLoop: received-network proof and separate lifecycle repair

The exact accepted-eight executables and immutable source manifest are preserved separately. The stale-proxy failure sequence uses the original accepted network/presentation behavior plus opt-in diagnostics and the early Ghost WIP mesh (38,358 Near / 15,933 Far triangles). It is not the untouched accepted-eight binary. That 96-frame sequence is `ghost-evidence/network-wip01`; its exact diagnostic executable and relevant app/network/diagnostic source are in `lifecycle-baseline`. The 64-frame pre-repair/repaired functional comparison keeps this same WIP mesh constant. Frames 66–73 are received empty rosters, and 89–96 are disconnected. Those failures are not replaced by a static stage or relabeled as transport timeout evidence.

The separately authorized repair is narrow: initially hide the proxy; reset cached presentation/history once when an active peer changes; hide/reset once on absent roster or disconnect; clear NetClient's connection-owned remote cache when disconnecting. No packet layout, last-arrival ordering, timeout, interpolation, collision, controller, mission or scene behavior was redesigned. The independent functional delta is `ghost-evidence/lifecycle-repair/LIFECYCLE_DELTA.patch`. Opt-in diagnostic/capture changes are reviewed separately from that delta and the art.

## What is actually received

`ghost_loopback_peer` and `ghost_network_protocol_tests` include the production codec implementation, using its `pack_state`, header writer/reader and actual `NetClient` socket/poll code. They use an ephemeral localhost UDP endpoint and wholly synthetic states. There is no external server, user traffic or direct assignment into the client's remote roster. Sent packet hex, actual received snapshots, visible entity transforms/materials and posed cache observations are saved independently.

The app's production block takes X/Z and heading from the accepted remote snapshot, intentionally fixes Y to 0.9 m, and derives gait from actual received position displacement and delta time. The protocol carries no velocity or crouch field. Gait proof therefore follows received position deltas; crouch images are explicitly stage stress cases. The existing cash/heat material equations remain numerically unchanged.

For reproducible original/new art pairs, both executables include the same lifecycle repair and opt-in diagnostic code. Only Ghost profile dispatch is disabled in the original-model executable, which is separately compiled, linked, source-hashed and fingerprinted. Its link map and dependency records are preserved in `final-model-only-baseline`.

A diagnostic-only rendezvous waits up to one second for the peer's counter written after its actual UDP send, then invokes production polling and presentation. This gives repeatable packet schedules; it does not reproduce normal asynchronous timing. Earlier asynchronous runs and the silence capture remain separate. Sent ticks and received frames are never treated as interchangeable.

## Lifecycle and timing limits

Accepted empty snapshots hide the proxy. Repeated empty frames retain the same mesh identities/capacities and pose revisions, with no repeated presentation reinstall. Real disconnect clears the cached remote roster. Same-peer and different-peer reconnects start travel and cash history from zero before subsequent received movement advances gait.

The protocol has no sequence/timestamp ordering. The real socket test deliberately sends a newer-looking position followed by an older-looking one; the last arriving valid snapshot wins, as before. Invalid-magic packets are rejected. No new packet while the socket remains connected retains the previous snapshot. In the silence capture, 80 later simulation frames retained that received state during a 32.1-second wall-time run. No transport-loss timeout or automatic stale-peer expiry is claimed or added.

The original/fixed functional comparison retained 64 of 64 byte-identical active frames (frames 2–65), independent of the final model-only art comparisons. The final source also has ordinary startup with networking off and unanswered-localhost startup controls, preserving whole-frame pixels and local PlayerBody state across the two art variants. Source-identical actor/scenery data remain in every world capture.

## Visual observations

The normal cyan silhouette is readable while walking and changing heading. Moving cars, street poles and other people legitimately occlude some forward views. The reverse camera preserves the world and supplies clearer complete-back, empty-roster and return images. Night close-ups show face/eye/jaw continuity; the true distant Far path preserves the vest cue with reduced groom/finger detail.

Under identical albedo/emissive numbers, the new shared textured material whitens more strongly during the existing cash/heat effect than the old untextured model. Numeric modulation and transport behavior are preserved; identical emission pixels are not claimed. No shared shader/atlas edit was used to disguise that difference.
