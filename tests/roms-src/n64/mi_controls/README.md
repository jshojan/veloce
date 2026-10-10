# Original N64 MI control corpus

This MIT licensed Python assembler emits `mi_controls.z64`, marked `V64HB001`,
with original MIPS code and no SDK, firmware or commercial ROM bytes. The
frontend loads the production plugin and writes actual guest reads to a file.
The guest has inputs and observation routines, without expected values or verdicts.

All **393,216/393,216 rows** match **3,145,728 guest values**:

- 131,072 mode transitions: eight prior flag combinations × all 16,384 low
  command words. Previous repeat count is 85; each write replaces it.
- 262,144 mask transitions: all 64 prior masks × all 4,096 command words.

Each row captures previous mode, submitted command, resulting mode, revision,
previous/resulting mask, pending interrupts and CPU Cause. Real PI and SI DMA
create both pending requests before the loop. The oracle computes transitions
independently with arithmetic, without CPU or assembler field helpers. High
reserved write bits are ones. Mode command clear/set pairs use set precedence.
DP acknowledgement is exercised with no guest DP producer; injected DP cases
are separately covered by the component.

An executed repeat-count-bit mutation fails all 128 reduced mode rows; the
1,024 reduced mask rows remain correct. Eight field mutations each fail one row;
seven invalid streams produce ERROR. Identity, domain, width, count, completion
and trailer checks reject incomplete output. Raw observations and reports stay
in `build/artifacts/owned/n64_mi_controls/`. Source/image hashes are in the manifest.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_mi_test -j
ctest --test-dir build -R '^n64_(mi_test|mi_controls)$' --output-on-failure
```

Fields follow authored [libdragon MI definitions](https://github.com/DragonMinded/libdragon/blob/trunk/include/mi.h).
Revision `02020102`, read-only write behavior and command ordering are grounded
in the authored [MI model](https://github.com/ares-emulator/ares/blob/master/ares/n64/mi/io.cpp)
and [revision definitions](https://github.com/ares-emulator/ares/blob/master/ares/n64/mi/mi.hpp).
The core stores repeat/EBus/upper controls; their RDRAM transaction effects,
ninth-bit memory, repeat writes, chip selection, missing SP/AI/VI/DP producers,
physical bus/interrupt timing and native boot remain pending.

The component additionally checks all 16,777,216 prior-mode/command transitions
(including every old count), all mask transitions, 512 SI/PI/injected-DP pending
and mask combinations, DP acknowledgment with other sources preserved, actual
CPU interrupt/ACK/ERET, invalid access/state atomicity, exact replay, old-state
conversion, reset and loaded-plugin execution.
