# Default netplay plugin, transport, and rollback modules

The plugin provides direct IPv4 TCP multiplayer and owns rollback execution.
The former simulated lobby has been replaced with a host/join UI using actual
sockets and a compatibility handshake. Two peers are supported. Two-controller cores give each peer a controller;
single-controller cores give the host its keypad and the joining peer viewer
status. Public matchmaking, session codes, peer-name exchange, chat and additional
spectators remain pending.

## Use in the frontend

1. Load the same ROM in both applications using the same compatible core binary and
   matching settings and saved RAM. Connection preparation recreates the core and
   reloads the ROM, preserving battery saves through the normal loader.
2. Choose **Netplay → Host Game**, select a TCP port, and click **Listen**. Port
   zero requests an available port, shown in the panel. The host listens on all
   IPv4 interfaces. Share the chosen port and reachable host IPv4 address.
3. On the other application choose **Netplay → Join Game**, enter that address
   and port, use the same rollback window, and click **Connect**.
4. Both cores reload and pause during connection. Matching full ROM/build/state
   identities start play automatically, with local input mapped to player 1 on
   the host and player 2 on the client for two-controller cores. For a single-
   controller core, the host owns the only controller and the client watches. Each player's normal keyboard/gamepad
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

`src/rollback_session.{hpp,cpp}` now implements a separate one/two-controller execution
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
State buffers query the current serialization requirement with
`save_state_fast(nullptr, 0)` and grow each history slot only as needed, reusing
its high-water capacity. The query must be nonzero and within the core's stable
maximum; actual writes must fit the queried bound. Invalid sizes and allocation
failures stop before executing an unsaved frame. Fixed-size cores retain their
full-state cost. Core serialization may still allocate internally.

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

Periodic desync checks/recovery, reconnects, additional spectators, broader core support,
rendered GUI tests, and physical two-machine testing remain. Delay-only mode and
input delay scheduling are unimplemented; `get_input_delay()` reports zero.
RTT measurement is unimplemented and ping reports zero. State resynchronization
is rejected with a notification. Network faults beyond the measured malformed
frames and ROM mismatch are not yet exhaustively tested. Internet latency/loss,
cross-platform peers, and Windows socket behavior remain unverified.

## Single-controller DS sessions

The DS core advertises one controller and implements deterministic rollback
states/hash. The production plugin accepts it, identifies the joining peer as a
viewer and normalizes viewer input to zero. Controller mode is pinned in the
compatibility identity. The rollback module also rejects nonzero viewer input
confirmations. `join_as_spectator()` supports this one-controller mode;
`get_spectator_count()` reports its one connected viewer. Additional viewers and
spectators alongside two active controllers remain unsupported.

`nds_netplay_plugin_test` verifies two TCP processes over 100 frames against a
third complete baseline state and an independent eight-word guest-register
oracle. Viewer rollback and lifecycle checks pass; DS audio is empty and has no
replay-audio claim. Artifacts remain in `build/artifacts/netplay-nds/`. This mirrors
one DS instance and does not emulate wireless multiplayer between handhelds.

### 3DS host/viewer profile

The experimental 3DS core now provides one-controller `INetplayCapable` and
complete 135,791,096-byte fast states. The production TCP gate matches both
independent peers to a 100-frame solo baseline and grades twelve original
guest words with a separate arithmetic oracle. The viewer predicts/replays the
host HID input; native 3DS wireless/link multiplayer remains pending. Full
states use about 129.5 MiB each, or ~1.14 GiB for the window-8 nine-state ring
per peer, plus core/scratch memory. Serialization avoids temporary state
vectors, but compression/page sharing and sustained real-time performance
remain unverified. Run `ctr_netplay_plugin_test`; register/state/log/report
artifacts are in `build/artifacts/netplay-3ds/`. CI/CD stays disabled.

### Wii U sparse-state profile

The experimental Wii U core provides one-controller `INetplayCapable` for its
original virtual input latch. Its full logical 32 MiB MEM1/2 GiB MEM2 state is
canonical nonzero pages; unchanged format preserves compatibility. The original
probe's snapshots range up to 21,492 bytes, with a 17,392-byte final state. Both
production TCP peers match the 100-frame baseline exactly, and twelve guest
words are independently graded, including input-dependent high-RAM readbacks.

History slots and frontend snapshots query current size and grow as needed;
they retain high-water capacity rather than allocating the 2,183,168,992-byte
upper bound for every frame. Variable-state tests verify growing/shrinking
replay and failure handling; dense workloads can still need substantial memory.
Native GamePad/VPAD/KPAD and wireless multiplayer remain pending. Run
`wiiu_netplay_plugin_test`; artifacts are in `build/artifacts/netplay-wiiu/`.
CI/CD remains disabled. Sustained performance, page sharing/delta states and
Internet/physical/cross-platform sessions remain unverified.
