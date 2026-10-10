# Default netplay plugin, transport, and rollback modules

The plugin provides direct IPv4 TCP multiplayer and owns rollback execution.
The former simulated lobby has been replaced with a host/join UI using actual
sockets and a compatibility handshake. Two players are supported; there is no
public matchmaking, session code, peer-name exchange, chat, or spectator mode.

## Use in the frontend

1. Load the same ROM in both applications using the same NES core binary and
   matching settings and saved RAM. Connection preparation recreates the core and
   reloads the ROM, preserving battery saves through the normal loader.
2. Choose **Netplay → Host Game**, select a TCP port, and click **Listen**. Port
   zero requests an available port, shown in the panel. The host listens on all
   IPv4 interfaces. Share the chosen port and reachable host IPv4 address.
3. On the other application choose **Netplay → Join Game**, enter that address
   and port, use the same rollback window, and click **Connect**.
4. Both cores reload and pause during connection. Matching full ROM/build/state
   identities start play automatically, with local input mapped to player 1 on
   the host and player 2 on the client. Each player's normal keyboard/gamepad
   binding supplies its local input. Disconnection or failure pauses the app.

The frontend polls even while paused. `execute_frame()` owns the live frame;
the frontend must not also call the core's frame method. Late confirmations
replay through `RollbackSession`, and the streaming callback gates duplicate
audio through `is_rolling_back()`. ROM/core replacement, reset, and external
savestate loads disconnect before changing the core. A changed core pointer or
frame is also detected during polling/execution. Pause is local; a running peer
will stall at the rollback limit until confirmations resume.

Netplay plugin ABI is **v2**, adding core/ROM/build identity access, polling,
owned-frame execution, and transport inspection. External netplay plugins must
be rebuilt; the registry rejects ABI v1 before invoking its factory. Emulator
plugin ABI remains v2. Old simulated-lobby settings are no longer used.

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

## TCP peer transport

`src/peer_connection.{hpp,cpp}` implements nonblocking IPv4 TCP listen/connect,
partial sends/receives, bounded queues, and a versioned binary hello/ack exchange.
The handshake requires matching full-ROM hash and core ROM CRC32, core/build identity, core ABI, initial
frame/state hash, and rollback window, with opposite player slots. Inputs carry
frame numbers and controller bits in explicit big-endian fields. Poll on the
emulator thread even while paused; heartbeats run every second and active peers
time out after ten seconds without a valid message. The listener waits until
connected or explicitly closed. Close releases sockets and discards queued output;
callers completing a finite session must drain `has_pending_output()` first.

This is a direct two-peer transport, without encryption, authentication, DNS,
IPv6, NAT traversal, relay service, reconnects, or thread synchronization. TCP
delivery ordering can increase latency when packets are lost. Compatibility
hashes detect differing ROMs/builds/start states; they are not a security boundary. The
test uses the exact shared-library binary hash as the build identity, so that
scenario requires identical binaries. Cross-platform compatibility is unverified.

## Verified evidence

```bash
cmake --build build --target veloce netplay_rollback_test netplay_network_peer netplay_plugin_peer
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

`netplay_network_test` starts two independent NES processes over actual loopback
TCP sockets. Delayed polling exercises rollback; both 100-frame final states
match every baseline byte. A ROM mismatch rejects both peers. Separate local
connections send byte-fragmented zero-length, oversized, unknown-type, and
truncated messages; all fail cleanly. Registers, complete states, logs, and a
JSON report are retained under `build/artifacts/network/`. Local socket
permission is required. POSIX sockets are tested here; Winsock support is
implemented but has not been tested on Windows.

`netplay_plugin_test` repeats the socket scenarios through the production
netplay shared plugin, dynamically loading it and the actual NES core in each
independent process. Both full states match the fresh-load baseline even after different prior
frame histories (17 host frames and 31 client frames). It checks that
callbacks/start occur only after handshake, each frontend call advances at most
one live frame, replay/live audio callbacks both occur, and replacement of the
host core pointer or external frame advancement stops and pauses a listening
session. Reports, registers, logs, and states are saved under
`build/artifacts/netplay-plugin/`. This tests the plugin/host contract; it does
not render the application's GUI or establish physical two-machine behavior.

## Remaining work

Periodic desync checks/recovery, reconnects, spectators, broader core support,
rendered GUI tests, and physical two-machine testing remain. Delay-only mode and
input delay scheduling are unimplemented; `get_input_delay()` reports zero.
RTT measurement is unimplemented and ping reports zero. State resynchronization
is rejected with a notification. Network faults beyond the measured malformed
frames and ROM mismatch are not yet exhaustively tested. Internet latency/loss,
cross-platform peers, and Windows socket behavior remain unverified.
