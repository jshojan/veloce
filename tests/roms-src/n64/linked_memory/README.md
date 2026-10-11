# Original N64 linked-memory cartridge

MIT source and the archived `linked_memory.z64` are original. The generator uses
the repository's small integer-trap assembler; no SDK, firmware, retail bytes,
reference emulator or expected answers are embedded.

The cartridge executes LL/SC and LLD/SCD in a 64-bit kernel profile. Every 16-bit
offset is exercised at both widths, with four explicit initial-data patterns,
their complemented store values, and both uninterrupted success and intervening
ERET failure. Instructions execute uncached while the target uses cached KSEG0;
uncached LL itself is architecturally undefined and is not an accuracy golden.

**1,048,576 cases / 10,485,760 actual 64-bit RAM words** are exported. Ten words
per row contain the actual sequence, width, immediate, initial value, replacement,
ERET selection, linked result, conditional result, subsequent cached memory read
and LLAddr. LL word loads sign-extend; SC stores the source low word. LLAddr records
physical address bits 31:4, independently of the chosen offset/base pair.

The producer reads physical RAM through the production plugin. A live checkpoint
between the first LL and SC must reproduce twelve ordinary/rollback records and
the entire resulting native state. S030 imports clear the previously absent LLbit;
all 61 reserved S031 flag bits must be rejected without changing state.

The Python oracle duplicates the numeric input specification, not CPU helpers.
It grades every row and requires exact identity/domain, order, completion and EOF.
An executed 256-case cartridge omitting ERET fails all 128 requested failure cases.
Four result-field mutations and six broken streams are rejected. Executable,
archive and actual register-file hashes are recorded under ignored build artifacts.

```bash
python3 tests/roms-src/n64/linked_memory/build.py /tmp/linked-memory.z64
cmake --build build --target n64_linked_memory_test n64_linked_memory_capture
ctest --test-dir build -R '^n64_linked_memory(_test)?$' --output-on-failure
```

The component gate checks **1,312,684 observations**, including register assignments, zero/alias operands,
32-bit kernel address truncation, 32-bit user/supervisor doubleword restrictions,
alignment/TLB exception precedence, load bus errors, diagnostic LLAddr writes and
repeated successful SC. Data-cache misses during load/store accesses and actual cache invalidations conservatively
clear LLbit in this functional model. Those model checks are **not hardware
timing goldens**: exact interference, snooping/DMA, instruction-cache effects,
bus exception ordering, reverse endian operation and pipeline timing remain
unverified. Different LL/SC addresses and uncached links are undefined and excluded.
The numeric input domain is four patterns, not every 32/64-bit value.

Instruction rules: NEC [VR4300 manual](https://hack64.net/docs/VR43XX.pdf), §3.1,
§5.4.7 and LL/LLD/SC/SCD/ERET instruction descriptions.
