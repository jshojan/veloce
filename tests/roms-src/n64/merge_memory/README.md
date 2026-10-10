# Original N64 unaligned merge-memory corpus

The original ROM executes LWL/LWR, LDL/LDR, SWL/SWR and SDL/SDR through the
loadable N64 core and real headless frontend. It emits actual GPR results for
loads and actual memory snapshots for stores. Sources and ROM are MIT licensed;
Python emits all MIPS instructions, labels, tables and strings. No SDK,
Nintendo IPL/PIF, logo or commercial ROM bytes are included.

## Declared domain

| Domain | Word instructions (four lanes each) | Doubleword instructions (eight lanes each) | Total |
|---|---|---|---|
| Every pair of repeated-byte memory/register values, 0–255 | 4 × 4 × 256² = 1,048,576 | 4 × 8 × 256² = 2,097,152 | 3,145,728 |
| Full-width boundary patterns, 16 × 16 | 4 × 4 × 16² = 4,096 | 4 × 8 × 16² = 8,192 | 12,288 |
| **All observations** | **1,052,672** | **2,105,344** | **3,158,016** |

The byte domain replicates each byte into all four/eight memory bytes and all
four/eight register bytes. Every memory-byte/register-byte combination is
executed at every address lane, including the aligned/full-width ends. This
covers every transferred and preserved byte value, **not every possible
independent multi-byte bit pattern**. Boundary tables add distinct bytes,
zero/all ones, sign boundaries, alternating bits and irregular full-width
values, so byte-order errors cannot hide behind repetition. `WORD` and `WIDE`
in `build.py` define those exact 16-pattern domains. Word input registers are
sign-extended to 64 bits. All observations in this domain are defined and graded.

Groups are boundary then byte domain; within each domain the order is
LWL/LWR/LDL/LDR/SWL/SWR/SDL/SDR, ascending lane, memory pattern, register pattern.
That is 96 groups. Each trial reloads both inputs, resets an eight-byte scratch
block, and executes one tested instruction. Loads capture the actual result in
GPR 21; word loads must sign-extend the merged bit 31 into the upper 32 bits.
Stores capture the whole eight-byte block with LD. Word-store observations
include the untouched adjacent four-byte sentinel `$2468ACE0`. Output routines
preserve the result. `REG r=` contains one 16-digit hexadecimal register value
or memory snapshot, not an expected value or verdict.

The independent host oracle performs byte-array slicing and replacement,
rather than reproducing the core's shifts/masks. Left loads copy memory from
the addressed byte through the end of its aligned block into the high register
bytes; right loads copy through the addressed byte into low register bytes.
Left/right stores perform the corresponding copies into memory. Word load
results are sign-extended. Hand references use different bytes in all positions
and exercise load/store, left/right, both widths, partial/full transactions and
sign extension. The instruction definitions are grounded in the
[NEC VR4300 manual](https://hack64.net/docs/VR43XX.pdf), chapter 16.

## Register channel and scope limits

The ROM uses the explicit `V64HB001` homebrew entry profile. It enters at
`$80000400`, with its payload copied from cartridge `$1000` into physical RAM
`$400`. Scratch RAM is at `$80040008`, outside the ROM payload. This bypasses
PIF/IPL/CIC and is not retail boot or physical-console validation.

Guest byte stores to the emulator-only `$BFF00000` observation register forward
characters to `ITestSink`; a word store to `$BFF00004` completes with an unknown
verdict. These debug registers are instrumentation, not Nintendo hardware. The
application owns the raw register file. No screenshot or stdout verdict is
scored. All group counts/rows, raw-file CRC/core/channel, metadata, completion,
`END observations_complete`, unknown-status trailer and absence of trailing
data are required before the host can grade PASS.

This ROM covers single instructions in RDRAM, big-endian kernel unmapped
addressing, the stated value domains and every byte lane. Paired accesses,
both instruction orders, cross-block results, SP storage, negative offsets,
base/target aliasing, $zero, protected stores, invalid masked writes and partial
state continuation are separately covered by `n64_machine_test`; those unit
checks are not counted as extra ROM corpus observations. A bus trace confirms
masked stores do not perform a read-modify-write data read. Transaction timing,
caches, TLB/privilege/reverse-endian modes, MMIO partial writes, fault priorities,
interrupt overlap and physical hardware remain unverified. Full 32/64-bit
operand enumeration and full N64 completion are not measured.

## Reproduce and artifacts

```bash
python3 tests/roms-src/build.py n64/merge_memory
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_machine_test
ctest --test-dir build -R '^n64_' --output-on-failure
```

Binary and generator SHA256 are recorded in `rom_manifest.json`. The runner
retains `merge_memory.result/.json` under
`build/artifacts/owned/n64_merge_memory/`, with group counts, mismatches,
examples, ROM hash, instruction steps and frame count. This corpus executes
the archived big-endian `.z64` image. The earlier N64 corpora separately cover
execution in `.v64`/`.n64` archive formats. No physical cycle count is claimed:
`cycles` in the trailer is interpreter instruction steps, and `frames` is a
100,000-instruction frontend scheduling quantum, not an emulated VI field.

A separate ROM uses the full boundary domain plus repeated byte values 0–15.
It executes LWR in the first LWL group and SWR in the first SWL group. Only
those two groups may fail. Missing/malformed rows, reordered groups, false-PASS
completion, missing/unterminated trailers and trailing data must be measurement
errors. All raw negative files and JSON reports are retained alongside the
positive artifacts.

Measured 2026-10-09: **3,158,016/3,158,016 observations matched**, zero mismatches
across 96 groups (100% of the declared finite domain). Completion occurred after
7,397 scheduling frames and 739,627,629 instruction steps. The executed mutant
failed only its two changed groups (244 load and 250 store mismatches); all
seven damaged-file controls were rejected.
