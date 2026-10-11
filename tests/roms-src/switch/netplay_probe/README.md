# Original Switch two-input netplay probe

Repository-owned MIT source; no SDK, firmware, keys or commercial bytes.
The reproducible uncompressed NSO0 is 376 bytes, SHA-256
`d0342eaf8d76b741605eab551f2cc3b07b7bebe4f564d6292a7e7b6c05aac9da`.

## Executed program and independent oracle

Eight setup instructions precede a fourteen-instruction loop. Two word loads
read emulator instrumentation at `0xffff000c` (host) and `0xffff0014` (client).
These addresses are synthetic inputs, not Switch HID registers or HOS services.
Both peers own a full 32-bit word. Across 100 frames, host input is
`(frame * 2246822519 + 0x13579bdf) mod 2^32`; client input is
`(frame * 2654435761 + 0x2468ace1) mod 2^32`.

The supplied process executes 100,000 instruction attempts per frame. Guest STR
instructions publish eight actual words at RAM `0x1000..0x101f`: host sum,
client sum, last host, last client, iteration counter, last XOR, zero and guard
`0x89abcdef`. The peer writes these RAM values to a strict file stream. There
are no expected answers or correctness verdicts in the guest.
[Independent grading](../../../netplay_network_test.py) derives each result
from load/store attempt positions, including transfers crossing frame boundaries;
it does not import the CPU or builder arithmetic.

The production default netplay plugin connects two independent loopback TCP
processes after different prior input histories, polls while paused, forces late
confirmations and rollback, then drains exactly 100 confirmed frames. Offline,
host and client complete 4,194,664-byte states and all eight guest words agree.
Executed same-length wrong-host and wrong-client load variants each corrupt
three words and are rejected. Eight individual field mutations and four malformed
register streams reject. ROM mismatch and fragmented zero-length, oversized,
unknown-type and truncated transport frames reject. Input binary/source hashes
are recorded before execution and checked afterward.

## State and scope

The adapter test replays all 64 individually selected bits across two inputs;
checks complete-state hash, short-buffer guards, invalid-state atomicity, legacy
`VNXS0001` migration to neutral inputs, ordinary local input, reset and unload.
The input-port component checks 65,536 correlated word pairs / 786,432
observations, including widths, read-only behavior and instruction-fetch refusal.
Neither is an exhaustive 64-bit input-pair corpus.

This proves production rollback for an original integer process profile.
Native Joy-Con/HID, HOS, wireless console links, firmware/retail boot, graphics,
audio and physical timing remain unfinished. The core emits no audio; the gate
requires zero live and replay audio chunks rather than claiming audio coverage.

```bash
python3 tests/roms-src/build.py switch/netplay_probe --check
ctest --test-dir build -R '^switch_(netplay_plugin|netplay_adapter|input_ports)_test$' --output-on-failure
```

Artifacts stay ignored under `build/artifacts/netplay-switch/`: actual result
streams, complete states, executed control captures, peer logs and `network.json`.
CI/CD is disabled; these gates run locally.
