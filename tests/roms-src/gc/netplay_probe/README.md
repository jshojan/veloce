# Original GameCube two-controller netplay probe

Repository-owned MIT PowerPC source; no SDK, firmware or commercial bytes.
The supplied original-container image is 164 bytes, SHA-256
`a068feaca09528e428dd9c2d28567f8408e137ae029e041b061d3c5bc122da7a`.

## Native controller transactions and output

Eleven setup instructions precede a 22-instruction loop. The guest submits
mode-3 controller reads to the existing serial-interface communication RAM at
`0xcc006400`. Two native digital controllers are connected on ports 0/1;
ports 2/3 remain unplugged. Each player owns its own frontend button mask.
Across 100 frames, host input is `(frame * 2246822519 + 0x13579bdf) mod 2^32`;
client input is `(frame * 2654435761 + 0x2468ace1) mod 2^32`.

Each frame supplies 100,000 instruction attempts. The guest writes eight actual
RAM words at `0x80001000..0x8000101f`: host sum, client sum, last host response,
last client response, iteration counter, last XOR, last client low response and
SI error status. The peer copies actual big-endian RAM words into a strict file
stream. There are no expected answers or correctness verdicts in the guest.
[Independent host grading](../../../netplay_network_test.py) derives wire bits
and each transaction/store attempt position without importing the CPU, serial
implementation or builder. It includes transactions crossing frame boundaries.
Native stick bytes are neutral; analog triggers use the current digital endpoints.

Two independent TCP processes use the production default plugin, erase different
prior histories during connection preparation, poll while paused, exercise late
confirmation/rollback and drain exactly 100 confirmed frames. Offline, host and
client complete **25,166,340-byte states** and all eight words agree. Same-length
executed wrong-host and wrong-client capture instructions each mismatch three
words and reject. Eight field faults and four malformed register streams reject.
ROM mismatch and fragmented zero-length, oversized, unknown-type and truncated
TCP frames reject. Binary/source input hashes are checked before and after capture.

## State and remaining scope

The adapter checks 64 individually selected input bits, two-port ownership and
unplugged ports, complete-state replay and hash, buffer guards, invalid-state
atomicity, legacy migration, ordinary single-controller input, reset and unload.
The existing SI component separately covers every 12-button combination, all
connected-port masks, transfer lengths and state persistence. The new 100-frame
trace is finite; it does not exhaust all paired button masks or native timing.

State magic/layout remains `VPPC0002`; `VPPC0001` migration initializes neutral
SI state. Fast save/load/hash allocate no memory and include all 24 MiB RAM,
CPU registers/counters and SI buffers, input masks, connected ports and status.
Audio is absent; the production gate requires zero live/replay audio chunks.

Native analog axes, calibrated origins, physical rumble, polling/wire timing,
full CPU/exceptions/MMU, native formats/firmware/retail boot, graphics/audio,
four-controller play and Wii Remotes/IOS remain unfinished. These are existing
original execution profiles, not a claim of retail compatibility.

```bash
python3 tests/roms-src/build.py gc/netplay_probe --check
ctest --test-dir build -R '^gc_netplay_(plugin|adapter)_test$' --output-on-failure
```

Artifacts remain ignored under `build/artifacts/netplay-gc/`: result
streams, complete states, executed controls, peer logs and `network.json`.
CI/CD remains disabled; gates run locally.
