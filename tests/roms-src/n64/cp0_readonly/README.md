# Original N64 CP0 read-only register corpus

The MIT assembler and archived `cp0_readonly.z64` are repository-owned originals:
no SDK, firmware, retail ROM bytes or external emulator code. Rebuild with
`python3 build.py`. The shared original assembler lives in `../integer_traps/`.

## Hardware rule and test domain

[NEC VR4300 manual](https://hack64.net/docs/VR43XX.pdf) defines BadVAddr as
read-only (6.3.2, p.164), PRId as read-only (5.4.5, p.151), and CacheErr as
read-only with zero-returning fields (6.3.11, p.178). MTC0/DMTC0 software writes
to these registers must not replace their values. BadVAddr must still be updated
by actual address exceptions. This does not classify every CP0 register as
read-only; for example Random has a documented writable compatibility bit 5.

The cartridge initializes BadVAddr through a real unaligned LW exception at
`0xfedcba9800060001`, an original six-instruction handler and ERET. PRId uses
the current homebrew profile's revision `0x0b22`; CacheErr starts at zero.
Every MTC0/DMTC0 target runs every **65,536-value sixteen-bit source domain at
all four positions** with four surrounding bit patterns, in ordinary and
actual taken-branch delay placement: **12,582,912 cases / 150,994,944 actual
RAM/register words**. This exhausts each selected sixteen-bit domain, not every
64-bit source cross-product. Execution uses an explicit Kernel64 profile.

At physical RAM 0x2000, each attempt writes twelve real 64-bit guest words:
requested write encoding, unchanged source, surrounding pattern, field position,
placement, target MFC0 word read, target DMFC0 full read, BadVAddr full read,
PRId read, CacheErr read, Status and sequence. The cartridge inserts NOP gaps
between CP0 writes/reads and dependent use; no physical pipeline timing is
claimed. BFF yields delimit diagnostic records rather than physical VI fields.

## Capture, independent oracle and controls

```sh
ctest --test-dir build -R '^(n64_cp0_readonly|n64_cp0_readonly_test)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

The existing `n64_integer_trap_capture` producer loads the production core,
reads actual guest RAM, requires completion and writes a ROM-identity-bound
`V64TRAP1` file. Twelve records and complete native states must match ordinary
and fast rollback execution. The Python oracle independently computes encoding
order, source fields, sign-extended word reads and preserved full register
values; only cartridge construction is imported from the assembler.

A mutant actually executes DMFC0 EPC in place of the requested target read;
all **3,072 short cases** are rejected. Twelve result/sequence mutations and
six identity/order/length/completion faults reject. Before/after hashes bind
core, capture producer, builder, oracle, shared assembler, recipe and archived
ROM. Ignored evidence is under `build/artifacts/owned/n64_cp0_readonly/`:
full/short/`executed-wrong-read` `.registers` and JSON reports, field/stream
mutations and `provenance.json`. Private ROMs are not part of this suite.

The component gate covers **22,020,097 observations**: the same source domains
in Kernel32/Kernel64, all source-register allocations with one-hot/one-clear
values, privilege/extension/EXL/ERL/CU0 combinations and delay metadata. CP0
unusable exceptions precede width restrictions; rejected writes preserve the
registers, GPRs, HI/LO and LLbit and never access the data bus. A real load fault
still updates BadVAddr. The previous core fails the read-only preservation gate.

Full CP0 Status/Config and reserved-register behavior, cold-reset revision
selection, pipeline hazards, physical TLB/cache/timer timing, complete CPU and
whole hardware remain unfinished. No ABI or S031 layout change; this earns no
additional full delivery milestone, and N64 remains **50%**.
