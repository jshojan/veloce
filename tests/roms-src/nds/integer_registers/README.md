# Original Nintendo DS dual-CPU integer corpus

The standalone [builder](build.py) emits two original ARM guest programs and a
marked cartridge with `.nds` segment descriptors. It contains no SDK, BIOS,
firmware, commercial assets, copied logo, expected answers or guest pass verdict.
The [host grader](run.py) consumes actual shared-plugin/frontend register output
and computes results with independent unbounded integer arithmetic. Instruction
semantics follow the [ARM architecture manual](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a).

## Declared domain

**44,238,336/44,238,336 four-word snapshots matched, zero mismatches**,
split into
29,492,224 ARM9 and 14,746,112 ARM7 observations:

| Groups | Operand domain | ARM9 | ARM7 |
|---|---|---|---|
| Fourteen binary arithmetic/logical/test operations | Every repeated-byte pair | 29,360,128 | 14,680,064 |
| MOV/MVN | Every repeated-byte source; unused first operand zero | 16,384 | 8,192 |
| Fourteen binary full-width boundary groups | All pairs of sixteen operand patterns | 114,688 | 57,344 |
| MOV/MVN full-width boundaries | Sixteen source patterns; unused first operand zero | 1,024 | 512 |

All sixteen AND/EOR/SUB/RSB/ADD/ADC/SBC/RSC/TST/TEQ/CMP/CMN/ORR/MOV/BIC/MVN
operations use the flag-setting ARM encoding with unshifted register operand.
Every incoming NZCV runs on both CPUs; ARM9 includes both Q states (32 flag
combinations). ARM7 has 16 combinations without writing reserved Q. The profile
supplies System-mode CPSR `$DF` below those flags.

The full-width patterns are 0, 1, 2, 15, 16, 127, 128, 255, `$7FFF`, `$8000`,
`$7FFFFFFF`, `$80000000`, `$FFFFFFFE`, `$FFFFFFFF`, `$55555555` and `$89ABCDEF`.
This is exhaustive over the declared repeated-byte pairs and flag states,
not all 32-bit operands or all DS hardware.

The guest records destination r4, CPSR immediately after the operation, and
unchanged r0/r1 operands. Test/compare instructions must preserve r4's seeded
`$DEADBEEF`. The host derives carry/no-borrow and signed overflow independently;
logical instructions preserve C/V, and every operation preserves ARM9 Q.
ARM9 executes from loaded main RAM and ARM7 from private WRAM. ARM7 polls a main
RAM flag until ARM9 completes output, preventing channel interleaving. Both
programs execute their own loops, arithmetic, capture and character formatting.

The reduced control retains all boundary groups and sixteen repeated-byte
values, for 347,136 snapshots. Executed ADC-to-ADD mutation failed only ADC
groups on both CPUs; seven damaged files returned ERROR. Controls never
replace the full declared domain. Domain completeness and hand reference
vectors, including ARM9 Q, are checked before execution.

Only observed registers are scored. No screenshots or guest verdicts are used.
Whole-image CRC32 identity, exact CPU/group/row order and counts, both completion
records, frontend terminator metadata and absence of extra data are mandatory.
The debug port is repository-only; see the [core profile](../../../../cores/nds/README.md).

```bash
python3 tests/roms-src/build.py --original-only --check
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target veloce nds_plugin nds_machine_test -j
ctest --test-dir build -R '^nds_' --output-on-failure
```

Raw output/reports remain in `build/artifacts/owned/nds_integer/`, uploaded by
owned-hardware CI. Source/image SHA256 are in the manifest. Register aliases,
shifters, WRAM allocations, malformed cartridges and exact state replay have
separate component evidence and do not enlarge this original-ROM domain.
Timing, physical cache coherency, CPU extensions, Thumb, interrupts, graphics,
audio, controls, firmware/retail boot and most platform hardware remain missing
or unverified. Full DS completion is unknown; DSi enhanced mode is unsupported.
