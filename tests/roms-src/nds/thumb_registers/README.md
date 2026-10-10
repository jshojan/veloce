# Original Nintendo DS Thumb register corpus

This standalone MIT generator emits an original `.nds` cartridge without an SDK,
firmware, BIOS, Nintendo logo, commercial code, expected answers or guest pass
verdict. ARM9 and ARM7 start in the supplied marked-cartridge ARM profile. For
every case, an ARM driver sets the inputs and flags, enters a Thumb stub with
BX, executes the target instruction, captures PC with a Thumb high-register MOV,
and returns to ARM with BX. MRS then captures CPSR before printing changes flags.
ARM9 signals through shared main RAM before ARM7 prints its stream. Both programs
execute from main RAM. This is an emulator test profile, not physical firmware boot.

## Finite domain

The full domain matched **51,426,816/51,426,816 five-word observations**,
zero mismatches (100% of this finite domain):
34,284,544 ARM9 and 17,142,272 ARM7, across 1,172 groups per CPU. It includes:

- All sixteen low-register ALU operations: AND/EOR/LSL/LSR/ASR/ADC/SBC/ROR,
  TST/NEG/CMP/CMN/ORR/MUL/BIC/MVN. Every pair of 256 repeated-byte words and
  sixteen full-width boundaries; NEG/MVN need only their single source domain.
- Register ADD/SUB with the same operand pairs/boundaries.
- All 32 immediate encodings for LSL/LSR/ASR, all eight ADD/SUB small immediates,
  and all 256 MOV/CMP/ADD/SUB large immediates, on sixteen full-width patterns.
- All sixteen incoming NZCV states on both CPUs, plus both ARM9 Q states.

The register shift domain includes every low-byte count. Cases record the actual
destination, CPSR, unchanged original source registers, and Thumb PC read. The
host computes the PC from the cartridge's segment header and marked instruction
section, independently of the builder's label resolver; each PC read must reflect
the halfword instruction address plus four. Thumb instruction locations are
program structure, not embedded expected results. The observed CPSR has T clear
after the return to ARM; component tests separately inspect live and saved T states.

The independent host uses unbounded integer arithmetic, signed range checks,
width reduction, and mathematical shifts/rotations. It grades every row and
requires the exact CPU/group order, cartridge CRC, counts and frontend trailer.
It captures all CPSR bits, but does not score ARM7 Thumb MUL's architecturally
unpredictable C bit in 1,052,672 snapshots. ARM9 MUL preserves C, and both CPUs
preserve V. The implementation deterministically retains ARM7 C without claiming
its physical value. Q is preserved on ARM9. The primary
[ARM architecture reference](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a)
also makes equal source/destination Thumb MUL unpredictable before ARMv6;
this ROM uses distinct registers. No whole-platform completion percentage follows.

## Grader controls and artifacts

The reduced cartridge keeps all boundaries and immediate groups, and uses byte
values 0/127/128/255: **1,083,264 observations**. An executed ADC-to-SBC mutant
must fail only the ADC groups (13,056 mismatches). Four single-row controls
accept ARM7 MUL's undefined C, reject defined ARM7 V and ARM9 C, and reject an
incorrect Thumb PC. Seven malformed files must return ERROR: missing/malformed
row, reordered group, false completion, missing/unterminated trailer and extra
data. The guest emits observed values only; host grading establishes PASS.

Raw files and JSON reports remain in `build/artifacts/owned/nds_thumb/` and are
uploaded by owned-hardware CI. The manifest records generator/image SHA256.

```bash
python3 tests/roms-src/build.py nds/thumb_registers --original-only --check
ctest --test-dir build -R '^nds_thumb_registers$' --output-on-failure
```

High-register arithmetic/PC and SP address forms, conditional/unconditional
branches, BLX register differences and exact Thumb/plugin state replay have
separate component checks. Full 32-bit operand domains, all aliases, instruction
latency, physical-hardware comparison, cache coherency, loads/stores, stack/block
transfers, long calls, SWI/exceptions and ARM9 DSP extensions remain unverified
or unimplemented. Retail DS and DSi boot, peripherals, graphics/audio/input and
DS netplay remain incomplete.
