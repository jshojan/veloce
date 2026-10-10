# Original V810 multiply/divide register corpus

The standalone [generator](build.py) emits an original, marked Virtual Boy
cartridge with reset and divide-zero vector code. It includes no Nintendo SDK,
commercial image, expected results or guest pass verdict. The independent
[host grader](run.py) reads the file produced through the actual VB plugin and
frontend. Instruction behavior follows the
[NEC V810 architecture manual](https://www.virtual-boy.com/downloads/978644/get/978650),
particularly MUL/MULU, DIV/DIVU and exception processing/RETI.

## Declared domain

**4,210,688/4,210,688 nine-word snapshots matched, zero mismatches**,
with all 16 incoming Z/S/OV/CY states:

- MUL and DIV: every signed byte pair (-128 through 127), embedded in 32-bit
  registers, and all pairs of 16 full-width boundary patterns.
- MULU and DIVU: every unsigned byte pair (0 through 255), and the same
  full-width pattern pairs.
- Division includes **8,704 zero-divisor cases**, signed minimum / -1 overflow,
  truncation toward zero, signed remainders, and unsigned result sign flags.

Each row contains nine observed 32-bit words: destination r20, high product or
remainder r30, post-instruction PSW, handler marker, fault EIPC, fault EIPSW,
fault ECR, handler-entry PSW, and source r25. r30 starts at `$13579BDF` each case.
The handler records actual system registers before flag-changing work, advances
EIPC by the two-byte instruction length and executes RETI. Captured exception
fields are zero-initialized for each case; only the handler fills them. The
vector stub preserves flags. The host obtains instruction addresses from the
assembly layout, independently computes expected arithmetic and exception
state, and grades every word. No pixels or stdout verdicts are scored.

The reduced controls execute MUL-to-MULU and EIPSW-to-ECR capture mutations.
They must fail only the corresponding arithmetic or zero-divisor observations.
Seven damaged files (missing/malformed row, reordered group, false END, missing
or unterminated trailer, trailing data) must return ERROR. File identity includes
actual whole-ROM CRC32; row/group counts and completion must match exactly.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce vb_plugin vb_machine_test -j
ctest --test-dir build -R '^vb_' --output-on-failure
```

Raw observations and JSON reports are retained in
`build/artifacts/owned/vb_muldiv/`. Image and
standalone generator SHA256 values are in the ROM manifest. The reduced controls
are generated separately and never substitute for the archived full domain.

## Limits

This is exhaustive over the declared byte domains and incoming flags, not every
32-bit operand pair or Virtual Boy hardware. Component checks separately cover
r0/r30/source aliasing, nested exception context and NP-priority return, fatal
fault write order/halt, and saved exception continuation. These do not expand
the original-ROM domain. Physical V810 hardware, instruction latency, pipeline
hazards, external interrupts, other exception producers, retail cartridges,
VIP graphics, VSU sound and controller hardware remain unverified or missing.
