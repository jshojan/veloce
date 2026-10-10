# Original V810 CAXI and port-transaction corpus

The standalone [builder](build.py) emits original code and a marked Virtual Boy
cartridge without an SDK, commercial assets, expected values or a guest pass
verdict. The [host grader](run.py) checks actual shared-plugin/frontend register
files using independent integer arithmetic and byte-array expectations. CPU
instruction semantics follow the
[NEC V810 architecture manual](https://www.virtual-boy.com/downloads/978644/get/978650).

## Declared domain

**17,055,744/17,055,744 nine-word snapshots matched, zero mismatches**,
with all four address low-bit lanes and all 16
incoming Z/S/OV/CY states:

| Group | Domain | Snapshots |
|---|---|---|
| CAXI comparison | Every pair of repeated-byte words, fixed replacement `$13579BDF` | 4,194,304 |
| CAXI replacement | Every old/replacement repeated-byte pair, matching comparison | 4,194,304 |
| CAXI boundaries | All triples of 16 full-width old/comparison/replacement patterns | 262,144 |
| Byte port pair | All repeated-byte words, OUT.B followed by IN.B | 16,384 |
| Halfword port pair | All 65,536 repeated-half words, OUT.H followed by IN.H | 4,194,304 |
| Word port pair | All 65,536 repeated-half words, OUT.W followed by IN.W | 4,194,304 |

The CAXI domain contains 4,227,072 successful comparisons. The remaining
comparisons must still issue the architectural old-word write-back. The ROM
observes loaded destination, PSW, memory, adjacent guard words, unchanged r30,
raw address and original operand registers. The host derives subtraction flags
with unbounded signed/unsigned arithmetic rather than the core's bit formulas.
All address lanes must target the aligned word.

The port groups record IN result, PSW after each of OUT and IN, the stored word,
both adjacent guards, source, raw address and original value. The host changes
the expected little-endian byte array at the aligned width and independently
zero-extends the read. High-bit byte/halfword inputs distinguish IN from signed
LD. OUT must preserve unselected bytes and truncate its source. Both operations
must preserve every incoming flag.

The ROM executes from its original reset vector and performs all operand setup,
transactions and observation itself. It streams actual registers through the
existing emulator-only debug address; no screenshots or stdout verdicts are
scored. Whole-ROM CRC32 identity, exact group/row counts, completion and the
frontend terminator trailer are mandatory.

The reduced positive control retains the full boundary triples. Executed
CAXI-to-ST.W and OUT.H-to-OUT.W mutations failed only their intended groups.
Seven damaged result files returned ERROR. Reduced controls never substitute
for the full archived domain. Domain completeness and hand-checked arithmetic
and byte-lane references are checked before execution.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target veloce vb_plugin vb_machine_test -j
ctest --test-dir build -R '^vb_' -j2 --output-on-failure
```

Raw files/reports remain in `build/artifacts/owned/vb_memory_ops/`. ROM and standalone generator SHA256 are in the manifest.

## Bus contract and limits

CAXI uses one `Memory::compare_exchange` callback. The bus owner must complete
its read, comparison and write without allowing another owner between them;
a mismatch writes the old word. The profile has one serialized CPU owner and
no asynchronous DMA, so one callback is the entire transaction. A future shared
bus owner must arbitrate that callback; external LOCK signals, wait states and
multiprocessor/multithread synchronization are not modeled or measured.

IN/OUT use distinct port callbacks. The marked original cartridge profile
explicitly shares its existing storage/debug decoder with port transactions.
This measures CPU instruction behavior against test storage, not physical
Virtual Boy port wiring, controller/timer registers or peripheral timing.
Unsupported bus owners reject the new callbacks; a failed callback stops
execution diagnostically without committing CPU result/flag output. Physical
bus-fault exception behavior remains unimplemented.

Component checks separately cover r0/r30/base/destination aliases, one CAXI
callback with original operands, actual profile read-before-write including a
mismatch, every writable PSW through port instructions, every signed displacement
and wrapping address, width/source masks, rejected callback CPU state and exact
save-state continuation. Those checks do not enlarge this original-ROM domain.
All 32-bit operand combinations, other addressing modes, timing, retail games,
remaining CPU/NVC, VIP/VSU and board peripherals remain unverified or missing;
full Virtual Boy completion is unknown.
