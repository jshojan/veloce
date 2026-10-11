# Original N64 TagLo/TagHi/PErr field corpus

Original MIT source, cartridge and independent file oracle; no external emulator,
SDK, firmware or retail assets. Uses the original assembler in
`../cache_coherence/build.py`. Rebuild with `python3 build.py`.

## Hardware rule and finite domain

[NEC VR4300 manual](https://hack64.net/docs/VR43XX.pdf), Figure 5-18 (p.155)
and Figure 6-11 (p.178): TagLo contains a twenty-bit physical tag at bits27:8
and primary cache state at bits7:6; other bits read zero. TagHi reads zero.
PErr has an eight-bit software diagnostic field and zero reserved bits; the
VR4300 does not implement hardware parity.

The cartridge executes MTC0 and DMTC0 for all three registers, all 65,536
values at each of four sixteen-bit positions, four surrounds and direct/actual
taken-delay placement: **12,582,912 cases / 150,994,944 actual guest words**.
Code/tables use uncached KSEG1 and Kernel64. Each case initializes PErr=0x42,
TagLo=0x012345c0, TagHi=0, then writes its target. CP0 NOP gaps separate readback
and use. Reserved bits are deliberately exercised; no undefined CACHE state is
executed by this cartridge.

Each RAM row at physical 0x2000 contains twelve 64-bit words: write instruction,
unchanged source, surround, field position, placement, target register, word and
full target readbacks, TagLo, TagHi, PErr and sequence. Non-target fields verify
write isolation. BFF diagnostic yields delimit records, not physical VI fields.

## Execution and independent oracle

```sh
ctest --test-dir build -R '^(n64_diagnostic_fields|n64_diagnostic_fields_test)$' --output-on-failure
```

The capture producer executes the production plugin and exports actual guest
RAM using the existing V64TRAP1 stream. The oracle computes expected fields by
bit extraction/arithmetic independently of CPU implementation; it imports only
the ROM constructor. It checks ROM CRC/domain, exact row order/size and guest
completion trailer. Producer/core/source/shared-assembler/recipe/archive hashes
must remain unchanged. Twelve ordinary/fast-rollback records and complete
states match. The short domain contains **3,072 cases**; an executed wrong
DMFC0 Config read rejects every case. Twelve field/sequence faults and six
malformed identity/length/order/completion streams reject.

The component gate checks **32,505,856 observations**: 22,020,096 field/source/
permission cases plus **10,485,760 actual CACHE consumer cases**. The consumers
execute software TagLo writes, Index_Store_Tag and Index_Load_Tag for all
1,048,576 physical tags, both write forms and every defined primary state:
instruction invalid/valid, data invalid/clean/dirty. Data clean state 2 reads
back as valid state 3; dirty bookkeeping is checked separately. Undefined
instruction state 3 and state 1 are excluded. It also covers all source register
allocations, one-hot/one-clear values, Kernel32/64, CU0/width priority,
EXL/ERL, delay metadata and preservation of operands/other diagnostic fields.
The previous core fails the reserved-bit gate.

This proves current software write/readback rules and defined functional tag
consumers. Hardware reset tags are undefined; deterministic explicit entry and
existing checkpoint import do not prove physical power-on state. Malformed
legacy register snapshots are not normalized on read. Physical cache/bus timing,
undefined tag-state behavior, full CPU/hardware and complete retail play remain
unfinished. N64 remains 50% of the fixed delivery checklist.
