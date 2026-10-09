# Default netplay plugin and rollback module

The plugin provides lobby/UI scaffolding. Its connection and broadcast paths
still simulate peers and its input methods keep local values without network
transport. Live multiplayer is incomplete.

`src/rollback_session.{hpp,cpp}` now implements a separate two-player execution
module for `INetplayCapable` cores:

- Save state before each frame and retain a bounded history.
- Predict missing remote input by repeating the previous value.
- Restore and replay when a late confirmation differs from prediction.
- Preserve confirmed future frames and recompute later predictions.
- Stall at the speculation limit rather than dropping unconfirmed history.
- Reject expired/far-future inputs and contradictory confirmations.
- Stop on save/load failure; discard buffered replay audio and expose
  `is_replaying()` so streaming hosts can suppress duplicate samples.

This is an original implementation of the prediction/resimulation approach
described by [GGPO](https://github.com/pond3r/ggpo#whats-ggpo); it does not link
the GGPO SDK or claim its network behavior. Session calls run on the emulator
thread. A transport must validate peer identity/ROM/core compatibility and feed
frame-indexed remote inputs. The module supports two players and a window of
1–`NETPLAY_MAX_ROLLBACK_FRAMES`; caller/core frame numbers must agree at creation.
State buffers are preallocated, although core serialization may still allocate.

## Verified evidence

```bash
cmake --build build --target netplay_rollback_test netplay_default
ctest --test-dir build -L netplay --output-on-failure
```

The test loads the actual NES shared plugin with an original controller-polling
ROM. Two sessions use opposite local slots and inputs delayed by three/five
frames. Final confirmations are delivered in reverse order. After 100 frames,
both peers match the ordinary baseline's state hash **and every byte of its
serialized state**. A neutral-input control differs, proving the ROM's execution
depends on controller input. Streaming callbacks exercise replay suppression;
buffered audio discard, window stalls, duplicate/conflicting confirmations,
expired/future frames, invalid settings, and serialization failures are tested.

Guest accumulator observations and all three final states are retained under
`build/artifacts/rollback/`. This proves the tested execution/replay scenario;
it does not exhaust every NES mapper, state component, or network fault domain.

## Integration still required

The lobby/frame loop does not yet use `RollbackSession`. Peer transport,
handshake/start-state agreement, frontend wiring and audio gating, desync
checks/recovery, reconnects, spectators, broader core support, and physical
two-machine testing remain. Delay-only mode and input delay scheduling also
remain to be implemented. Do not treat simulated lobby connections as proof of
working online play.
