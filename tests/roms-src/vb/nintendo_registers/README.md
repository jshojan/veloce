# Original Nintendo V810 instruction corpus

Repository-owned MIT assembler and guest program; no SDK, firmware, commercial
ROM bytes or imported emulator implementation. The 4 MiB marked cartridge
reproduces with SHA-256
`27b29f1947b0f534cc80a8d4e01f0ec44c8398f306d8abfc8c45cb59f0f697dd`.
It uses the repository's original V810 assembler from `vb/immediates/build.py`.

The instruction encodings and status effects follow the author's
[Sacred Tech Scroll](https://files.virtual-boy.com/download/978651/stsvb.html).
MPYHW's signed **17-bit** source and unchanged r30 are grounded in the author's
[physical instruction experiment](https://www.virtual-boy.com/forums/t/nintendo-instruction-cycle-counts/).
Those hardware timings are not implemented or graded here. REV reads reg1 and
writes reg2; XB swaps only the low two bytes, XH swaps halves, and all four
extensions preserve the whole PSW. CLI/SEI clear/set ID and preserve other bits.

## Actual guest observations

The guest writes its changing instruction to WRAM, sets the incoming PSW,
executes it, and captures actual registers. Each row contains eight words:
destination, PSW, source, destination before execution, r30, an unrelated GPR,
encoded instruction, and incoming PSW. It contains no expected answers or verdict.

Emulator-only word writes at `$04000008` emit each actual bus value as eight
hexadecimal characters. Byte writes at `$04000000` supply framing; word writes
at `$04000004` end with unknown status. These addresses are repository test
transport, not native hardware. The frontend owns the result file and its ROM
CRC32 metadata. The host independently derives inputs, arithmetic and expected
register preservation without importing CPU or builder oracle calculations.

| Finite domain | Actual snapshots |
|---|---:|
| XB: every low half, four fixed upper halves, all 16 low flags | 4,194,304 |
| XH: every upper half, four fixed low halves, all 16 low flags | 4,194,304 |
| REV: every halfword in low, high and repeated-half arrangements; all low flags | 3,145,728 |
| MPYHW: every 17-bit factor, four source upper surrounds, four destinations, two low flag seeds | 4,194,304 |
| CLI/SEI: each of all 262,144 writable PSWs for each instruction | 524,288 |
| **Total** | **16,252,928 / 130,023,424 actual words** |

These snapshots match independently with zero mismatches. This is the entire
declared finite domain, not every 32-bit operand pair or full CPU completion.
XB/XH use canonical reg1=0 in the guest; noncanonical source fields and reserved
encodings are excluded from physical hardware scoring.

## Controls, components and checkpoint replay

The short positive has 1,854 rows. Executed wrong-result and wrong-PSW guests
each fail all 1,854 rows with one word per row. Signed-16 preprocessing of MPYHW
fails 224 rows / 288 words, isolated to multiply groups: 224 source words and
64 result words. Independent signed-16 arithmetic predicts those counts.
Eight individual field faults fail, and seven malformed streams return ERROR.
The grader requires exact identity, group order, row length/count, completion,
unknown-status trailer and end of file. Capture binaries, sources, assembler,
ROM and grader are fingerprinted before and after execution.

The component passes **35,657,783 observations**: all reg1/reg2 combinations,
r0 and r30 aliases, eight operand patterns/all low flags; all signed 17-bit
factors on four upper surrounds/four destinations/all low flags; all writable
PSWs for six instructions; every nonzero reserved field; unsupported extension
subopcodes; and a rejected tail fetch. The multiplication reference uses modular
shift-and-add instead of the CPU's signed multiplication. It compares complete
GPR/SR arrays and instruction progression. XB/XH ignore noncanonical reg1 in the
supplied profile; reserved fields diagnose before register/status mutation.
Neither policy establishes a physical illegal-instruction exception.

A further 65,536 transport patterns verify exact word formatting and rejected
byte/halfword writes. Complete machine state replays each instruction family.
The actual shared plugin replays an active output chunk and full 66,208-byte
state for each family, including frame/step counters, CPU, WRAM and controller.
`VVBS0003` stays unchanged; legacy `VVBS0002` pad migration is retained.

## Run and remaining scope

```bash
python3 tests/roms-src/build.py vb/nintendo_registers --check
cmake --build build --target veloce vb_plugin vb_nintendo_test vb_nintendo_plugin_state_test -j4
ctest --test-dir build -R '^vb_nintendo_(test|registers)$' --output-on-failure
```

Actual files, controls and provenance stay ignored in
`build/artifacts/owned/vb_nintendo/`. CI/CD remains disabled. VB stays **25%**
of the fixed delivery milestones; this CPU subset earns no whole-CPU milestone.
Search bit strings, floating point, remaining NVC/system registers and exception
producers, caches, physical timing, VIP/VSU, hardware controller polling,
retail boot, saves and netplay remain unfinished.
