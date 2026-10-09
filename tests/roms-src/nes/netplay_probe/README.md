# Original NES rollback controller probe

The original 16 KiB NROM program continuously strobes `$4016`, reads eight
bits from `$4016/$4017`, and accumulates the observed controller bytes in CPU
RAM `$12/$13`. PPU/NMI and APU IRQ sources are disabled for this probe. It
contains no expected values or visual PASS verdict.

`build.py` emits the instructions, resolves labels, and constructs the iNES
header/vectors using only Python. Its committed binary and generator hashes
are covered by `tests/roms-src/build.py --original-only --check`.

The C++ `netplay_rollback_test` loads the actual NES plugin and runs a 100-frame
ordinary baseline plus two sessions with delayed/reordered remote inputs. It
requires identical full serialized states after corrections; a neutral-input
run must differ. The harness reads the guest accumulator registers into
`build/artifacts/rollback/registers.result` and saves all three final states.
The result file is a dedicated rollback-test report, not a `VELOCE-RESULT/1`
ROM-emitted verdict. No screenshot or stdout verdict is scored.

```bash
cmake --build build --target netplay_rollback_test netplay_network_peer
ctest --test-dir build -L netplay --output-on-failure
```

Measured 2026-10-09: both peers converged on the full 100-frame baseline state;
the observed accumulators were P0=145, P1=247. These are recorded observations
for this deterministic stimulus, not independent hardware reference values.
The test verifies replay/input sensitivity for this ROM; it does not establish
controller timing accuracy, all-state/mapper coverage, or live network play.

The additional `netplay_network_test` runs this ROM in two independent NES
processes over loopback TCP, with delayed polling causing rollback. Both final
states equal the ordinary baseline. Guest accumulators and full states are
retained under `build/artifacts/network/`, alongside transport reports and logs.
This verifies a local socket scenario; frontend integration and physical
two-machine testing remain.
