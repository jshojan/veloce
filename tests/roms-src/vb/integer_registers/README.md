# Original Virtual Boy V810 integer corpus

A standalone Python assembler emits a marked `.vb` cartridge with original reset
instructions and test code. No SDK, copyrighted boot assets or commercial ROMs
are included. The frontend discovers the actual ABI v2 VB core plugin, executes
the ROM and streams observed register/PSW pairs into a result file. The guest
captures PSW with STSR immediately after each measured instruction. It never
calculates expected results or claims a pass; debug completion is ungraded.

All **6,512,640/6,512,640 pairs** matched the independent Python oracle:

| Domain | Operations | Operands | Incoming flags | Cases |
|---|---|---|---|---|
| Repeated-byte words | ADD/SUB/CMP/OR/AND/XOR | All 256×256 pairs (`byte × $01010101`) | All 16 Z/S/OV/CY states | 6,291,456 |
| Full-width patterns | Same six operations | 16×16 boundaries/patterns | All 16 states | 24,576 |
| Register shift amounts | SHL/SHR/SAR | 16 full-width patterns × all low-byte amounts 0–255 | All 16 states | 196,608 |

The oracle uses unbounded Python arithmetic and signed range checks for
carry/borrow/overflow; the C++ core uses unsigned word arithmetic and bit tests.
Explicit hand-checked oracle references cover overflow, borrow, carry,
logic carry preservation, CMP destination preservation and zero-count shifts.
Shared generator data defines the input domain only, not expected CPU behavior.
There are 15 ordered groups. A changed, omitted, malformed or unterminated result
is rejected; a guest stdout or screenshot cannot produce a pass.

An executed ADD-to-SUB mutation on a reduced byte/amount domain changes both ADD
groups and leaves all other groups matching. Seven damaged result-file controls
return ERROR. The runner preserves positive/negative raw files and JSON reports
in `build/artifacts/owned/vb_integer/`. CI runs the test and uploads those files.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce vb_plugin vb_machine_test -j
ctest --test-dir build -R '^vb_' --output-on-failure
```

`VVBHB001` at `size - $200` opts into the experimental homebrew loader. Startup
executes the original cartridge's reset vector, then runs the corpus from mapped
ROM. `ST.B` to emulator-only `$04000000` writes observed characters; `ST.W` to
`$04000004` completes with unknown status. The frontend adds raw-file CRC32 and
channel metadata. The generator and archived image SHA256 values are tracked in
`tests/roms-src/rom_manifest.json`; sources are MIT licensed.

Coverage is exhaustive only over the declared repeated-byte, pattern and
incoming-flag domains. Independent multi-byte 32-bit combinations, other
operands/register aliases/addressing modes, instruction timing, exceptions,
remaining V810/NVC instructions, VIP/VSU, controllers, timers and physical
hardware remain pending. Full Virtual Boy completion is unknown.
