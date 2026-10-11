# Original AArch64 single-transfer register corpus

Original MIT uncompressed NSO0 and assembler, with no firmware, SDK, keys,
retail assets or external emulator source. Reuses only the original assembler
from `../ccmp_registers/build.py` and `../arithmetic_registers/build.py`.

## Defined behavior and supplied profile

Arm's published definitions describe [immediate loads](https://www.scs.stanford.edu/~zyedidia/arm64/ldr_imm_gen.html),
[signed byte loads](https://www.scs.stanford.edu/~zyedidia/arm64/ldrsb_imm.html),
[register-offset loads](https://www.scs.stanford.edu/~zyedidia/arm64/ldr_reg_gen.html),
[unscaled loads](https://www.scs.stanford.edu/~zyedidia/arm64/ldur_gen.html) and
[unprivileged loads](https://www.scs.stanford.edu/~zyedidia/arm64/ldtr.html).

Thirteen integer transfer kinds are enabled: byte/halfword/word/doubleword stores
and zero-extending loads; signed byte/halfword/word loads to X registers;
and signed byte/halfword loads to W registers. W results zero the upper half.

- Unsigned 12-bit offsets scale by access size. Unscaled, unprivileged,
  pre-index and post-index forms use signed 9-bit **byte** offsets.
- Post-index accesses the old base; pre-index accesses the updated base.
  Only pre/post-index forms write back, after a successful access.
- Register offsets support UXTW, LSL, SXTW and SXTX, with optional access-size
  scaling. Offset register 31 reads zero; base 31 is SP, transfer 31 is XZR.
- NZCV is preserved. No-writeback base/destination/index aliases are supported.
  Writeback base/destination overlap diagnoses before access, selecting an
  allowed constrained-unpredictable outcome. Reserved extension options,
  SIMD/FP forms and unsupported prefetch/atomic encodings diagnose.
- The supplied process map enforces 16-byte SP alignment and permits unaligned
  general-base normal-memory accesses. Unprivileged transfers use the same
  process permissions; text/RO remain protected. SCTLR/EL/UAO/MMU controls,
  tag checking, physical alignment/configuration and precise architectural
  exception entry remain unfinished.

Addresses wrap modulo 2^64 before the process bus validates the effective
address. Register tests can have a large base and canceling index that reach
mapped RAM; this does not establish canonical virtual-address/MMU behavior.
A failed access leaves destination, base, PC and RAM unchanged in this profile.
Instruction timing, memory ordering and physical faults remain unverified.

## Actual original-guest domains

The image emits **7,188,480 snapshots / 57,507,840 actual 64-bit register words**:
28,080 groups × all 256 repeated-byte input seeds.

| Domain | Groups |
|---|---:|
| 13 transfer kinds × four signed-immediate address classes × all 512 fields | 26,624 |
| 13 kinds × eight selected unsigned offsets × two bases × two flag surrounds | 416 |
| 13 kinds × four register extensions × both scaling bits × five indices × two flags | 1,040 |

Signed-immediate groups assign X8 to even offsets and SP to odd offsets; flags
are all clear/set according to encoded offset bit 1. This is a correlated
base/flag domain, **not every independent offset/base/flag combination**.
Unsigned fields are 0, 1, 2, 7, 255, 256, 2048, 4095. Register indices are 0,
1, 255, 0xffffffff00000001 and 0xffffffffffffffff; register groups use X8.
The component additionally exhausts unsigned fields, all register fields/aliases
and boundary faults. These domains do not cover every independent 64-bit source,
index, base, memory mapping or CPU configuration.

Rows have eight actual words: result/source register, post-operation base,
memory readback, MRS NZCV, unchanged input seed, requested encoding, unchanged
index and initial base. Ordinary scalar stores initialize load bytes; stores
start from the complement. Complement guards on both sides make shifted reads
and partial shifted stores detectable even for repeated-byte data. Memory
readback uses a scalar unsigned load after the tested instruction. No expected
answer, framebuffer or guest verdict is graded.

Text starts at zero, RO at 0x3b0000, writable data/BSS at 0x3c0000. Register
cases choose a base that reaches the mapped target after extension/scaling.
The native format is uncompressed NSO0; firmware/HOS/retail boot is unimplemented.
Switch remains **25%** of the fixed delivery checklist.

## Independent grading and controls

```sh
ctest --test-dir build -R '^switch_single_transfer_' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

The host imports only the constructor. It independently enumerates domains and
encodings, builds bytes and applies signed conversion/modulo arithmetic to
results and offsets. Every word, ROM CRC, channel, group count, completion,
ungraded trailer and EOF is checked. Emulator/core, CPU/bus, oracle/builder,
original assembler, recipe and archive hashes must remain fixed.

The **3,328-case** short image uses four byte values, five signed fields,
three unsigned fields and two register indices. Executed wrong-result and N-flag
capture mutations reject all 3,328 rows.
Wrong immediate offsets reject all **1,664 immediate rows**; unsigned replacement
rejects **640 negative signed-load rows**; omitted stores reject all **1,024 store
rows**. Eight field faults and seven malformed/incomplete/false-completion
streams reject.

The component checks **17,457,707 observations**: every signed/unsigned immediate
in its documented byte/boundary domains, all valid register fields/aliases for
four extensions and both scaling bits, selected reserved/overlap encodings,
SP alignment and ordered bus faults. Result bytes and surrounding untouched
bytes are independently graded; access address/width/count are checked. The
prior interpreter fails the actual new signed unscaled-load result gate.

Production-plugin replay compares actual output and complete **4,194,648-byte**
VNXS0001 states. The machine gate checks single-access SP diagnostics and
cross-map read/write faults before register/base/byte mutation, complete-state
fault replay, and unprivileged stores respecting executable protection.
ABI/state layout are unchanged. Whole CPU/system/exception/MMU behavior,
physical timing, firmware/HOS/retail boot, graphics/audio, native controllers and
netplay remain unfinished. CI/CD remains disabled; private ROMs are excluded.

Archived original image: **3,690,420 bytes**, SHA256
`b831c415f5d25d1d9b618c92487fce2a7443651c69ca77853c73de97bd90e84e`.

Executed integration: full Release build and all **29 Switch gates** pass,
including prior original instruction corpora and production replay. The new
7,188,480-row capture has zero mismatches and unchanged source/binary provenance.
All **147 original archives** reproduce; completion rendering and whitespace
checks pass. CI/CD remains disabled.
