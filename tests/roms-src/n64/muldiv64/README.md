# Original N64 doubleword multiply/divide corpus

The original ROM executes DMULT, DMULTU, DDIV and DDIVU in the loadable
VR4300 core. Each observation captures the actual 64-bit LO and HI registers
through MFLO/MFHI and emits both into a register file. It includes no SDK,
Nintendo IPL/PIF, copyrighted logo or commercial ROM assets. Sources and ROM
are MIT licensed; Python builds the binary and label fixups without a toolchain.

## Exact domain and score

| Domain | Instructions | Inputs per instruction | Observations | Defined results |
|---|---|---|---|---|
| Signed byte values, sign-extended to 64 bits | DMULT, DDIV | Every pair in -128…127 | 131,072 | 130,816 |
| Unsigned byte values, zero-extended to 64 bits | DMULTU, DDIVU | Every pair in 0…255 | 131,072 | 130,816 |
| Full-width boundaries | All four | 16 patterns × 16 patterns | 1,024 | 991 |
| **Total** | | | **263,168** | **262,623** |

The signed byte table is ordered by raw byte 0–255, with values 128–255
sign-extended as negative operands. The unsigned table uses 0–255 directly.
Every byte pair is executed once per operation. DDIV(-128,-1) is a defined
64-bit result of +128 and is graded; no 8-bit or 32-bit quotient truncation
is allowed. These are **doubleword instructions operating on byte subdomains**,
not 8-bit instructions or exhaustive 64-bit input domains.

The 16 full-width patterns are exactly those in `WIDE` in `build.py`: zero,
1, 2, 15, the 32-bit sign/boundary values, $100000000, signed 64-bit min/max,
all ones, -2, alternating bits, and irregular $0123456789ABCDEF and
$FEDCBA9876543210. Their cross-product tests carry across both 32-bit limbs,
signed 128-bit products, wide quotients and dividend-signed remainders.

All 544 divisor-zero observations and one signed minimum/-1 overflow
observation are retained but unscored. The independent host oracle uses Python
unbounded integer products modulo 2^128 and quotient truncation toward zero;
it does not import the emulator's limb-product implementation. Hand-calculated
references include (2^64-1)^2, signed -1×-1, 2^32×2^32, min×-1, min×2,
negative quotient/remainder combinations and min/3. The architecture reference
is the [NEC VR4300 manual](https://hack64.net/docs/VR43XX.pdf), chapter 16 and
the extended multiply/divide instruction summary. Undefined-input fallback
values in the core are deterministic conventions, not hardware accuracy proof.

## Observation path and limits

Each group loads operands into GPRs 18/22, executes one instruction and captures
LO/HI into GPRs 21/23. Output routines preserve those registers. A `REG r=` row
contains 32 lowercase hex digits: LO first, then HI, each exactly 64 bits.
Group order is boundary DMULT/DMULTU/DDIV/DDIVU, then byte domains in the same
instruction order. Input ordering follows the tables, lhs then rhs.

The ROM uses the documented `V64HB001` homebrew loader profile and enters at
$80000400 after its payload is copied from cartridge $1000 to RAM $400.
This bypasses PIF/IPL/CIC and is not retail boot or physical hardware evidence.
Guest byte stores to the emulator-only $BFF00000 observation register output
characters through `ITestSink` to the frontend-owned file. A word store to
$BFF00004 ends with an unknown verdict. These debug addresses are not Nintendo
hardware registers. No screenshot or stdout verdict is scored.

The parser requires the raw-file CRC/core/channel, exact metadata, all eight
groups in order, every LO/HI pair, completion count, `END observations_complete`,
an unknown-status trailer, and no trailing data before it can grade PASS.
Multiply/divide pipeline stalls, HI/LO hazards, caches, interrupts, timing and
privilege modes remain unverified. The trailer's cycle field counts interpreter
instructions; frames are scheduling quanta. Full N64 hardware completion is not
measured by this suite.

## Reproduce and evidence

```bash
python3 tests/roms-src/build.py n64/muldiv64
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_machine_test
ctest --test-dir build -R '^n64_' --output-on-failure
```

Binary and generator SHA256 are in `rom_manifest.json`. The runner retains
`muldiv64.result/.json` under `build/artifacts/owned/n64_muldiv64/`, including
per-group observed/graded/unscored counts, mismatch examples, ROM hash, frame
count and instruction count. It also executes `.v64` and `.n64` variants through
actual frontend discovery; they must match the same corpus with their distinct
raw-file CRCs and are retained as `v64.result/.json` and `n64.result/.json`.

A mutant executes unsigned instructions in the signed boundary multiply and
divide groups. Exactly those two groups must fail, proving both result paths can
reject wrong executed instructions. Missing or malformed rows, reordered groups,
false-PASS completion, missing/unterminated trailers and trailing data must be
measurement errors. Raw negative files and JSON reports remain with the positive
artifacts and are uploaded by CI. No external ROM download is required.

Measured 2026-10-09: **262,623/262,623 defined LO/HI pairs matched**, all
263,168 observations present, 545 undefined inputs unscored, zero mismatches
across eight groups. All three archive formats matched. The big-endian run
completed after 1,054 scheduling frames and 105,320,654 instruction steps.
The executed mutant failed only its two changed groups (124 multiply and 86
divide mismatches); all seven damaged-file controls were rejected.
