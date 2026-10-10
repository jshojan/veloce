# Original V810 exception register corpus

The standalone [builder](build.py) emits an original marked Virtual Boy ROM
with reset, TRAP, illegal-opcode and nested-exception vector code. It uses no
Nintendo SDK, commercial assets, expected-value tables or guest pass verdict.
The [host grader](run.py) computes expectations independently from the
[NEC V810 architecture manual](https://www.virtual-boy.com/downloads/978644/get/978650),
including TRAP's next-instruction restore PC, illegal instructions' faulting PC,
exception-code banking, PSW entry changes and NP-priority RETI.

## Declared domain

The full ROM matched **4,685,824/4,685,824 thirteen-word snapshots**,
with **zero mismatches**:

- All **32 TRAP parameters**, each with all **131,072 writable PSW combinations
  with NP clear**. This includes all Z/S/OV/CY states, all six sticky floating
  flags, all 16 interrupt levels, ID, AE, and both EP states.
- Three unassigned major opcode families (`$1B`, `$32`, `$36`), one canonical
  encoding each, with the same complete nonfatal PSW domain.
- Every **1,024 low-bit encoding** of each of those three illegal opcodes,
  with all 16 condition-flag states and both EP states.

Every case loads its instruction into WRAM at `$05003000`, followed by an
original JMP back to the observation routine. This is CPU code execution from
RAM, not a host instruction injection. EIPC/EIPSW and FEPC/FEPSW are initialized
with distinct sentinels. The ROM then loads the measured PSW and jumps to the
instruction. All vector stubs preserve flags and set a distinct vector marker.
The handler records entry PSW and the selected saved PC/PSW, advances an illegal
instruction's saved PC by two bytes, and executes RETI. TRAP returns directly to
its architectural next-instruction PC. ECR is read-only and persists; the host
tracks its two halves independently across the complete case sequence.

Each row records, in order:

1. EIPC, EIPSW, FEPC, FEPSW, ECR and post-return PSW.
2. Vector marker, original saved PC, handler-entry PSW and saved PSW.
3. Unchanged sentinel GPRs r1, r2 and r30.

The host grades all 13 words. No framebuffer, screenshot or stdout verdict is
used. Complete group/row counts, whole-ROM CRC32 identity, completion marker and
frontend terminator trailer are required. The reduced positive control retains
the complete encoding sweep. Executed TRAP-vector and saved-context capture
mutations failed as required; seven damaged result files returned ERROR. The reduced
control never substitutes for the archived full domain.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target veloce vb_plugin vb_machine_test -j
ctest --test-dir build -R '^vb_' --output-on-failure
```

Raw register files and JSON reports are retained under
`build/artifacts/owned/vb_exceptions/` and uploaded by owned-hardware CI. The ROM
manifest records both image and standalone builder SHA256.

## Limits

This covers the declared exception domains, not all V810 exceptions, instruction
encodings, or complete Virtual Boy hardware. Valid instructions that remain
unimplemented still stop diagnostically; they are not treated as illegal.
Only the three listed major opcode families are classified by this gate.
NP-set fatal cases cannot return through a ROM handler: component checks
separately cover fatal source/PSW/faulting-PC bus writes in order and halt until
reset, including TRAP's faulting PC rather than its normal saved next PC. Those
checks also cover every listed encoding, all condition/control combinations,
sticky-flag and interrupt-level boundaries, and RETI context restoration.
Address traps have a separate [register corpus](../address_traps/README.md).
Physical hardware, bus/pipeline timing, external interrupts,
remaining instructions and exceptions, VIP/VSU, controllers and retail games
remain unverified or unimplemented. Full hardware completion is unknown.
