# Original N64 CP0 physical Watch cartridge

MIT source and archive are original; the generator uses the repository's small
assembler. No firmware, SDK, retail bytes, external emulator or guest expected
answers are embedded. The handler exports actual Cause/EPC/Status and its count;
the guest also exports actual operands, CP0 WatchLo readback and sequence to RAM.

The native sweep covers **every eight-byte granule in 8 MiB RDRAM**, at read and
write senses, in ordinary and actual taken-branch delay slots. Watches prevent
the target operation from touching data, including the guest's own code/handler.
Then 512 original control cases use all eight byte lanes, all R/W flags,
matching/neighbor granules and EXL clear/set. Suppressed/unmatched LBU/SB operations
execute, and the guest exports their actual result and memory-byte readback.

**4,194,816 cases / 58,727,424 actual 64-bit RAM/CP0 words** are independently
graded. Fourteen words carry instruction tag, physical address, requested
WatchLo, starting Status, placement, handler count, captured Cause/EPC/Status,
post-operation Status, actual operand/result, WatchLo readback, control memory
byte and sequence. The memory-byte field is zero in the trapped sweep; its
contents are graded only for the initialized control target.

An armed WatchLo checkpoint before the first data operation is restored through
the production fast path. Twelve ordinary/rollback RAM records and the complete
final state must match. S031 already serializes both watch registers; no new
format or capacity is needed.

The component domain exhausts **all 2^29 eight-byte granules of the 32-bit
physical address space**, at both read/write senses using LW/SW: **1,073,745,425 total observations**
including the supplementary cases. Additional
checks cover flags, lanes, scalar/merge/FP/linked widths, EXL, MTC0/DMTC0 and
MFC0/DMFC0 reserved-bit/readback behavior, translation and alignment priority,
Watch before DBE, delay/BEV metadata and fetch/CACHE exclusions. Bus spies verify
that trapped operations never access data. This is not all combinations of
watch and target addresses or all numerical operand values.

An executed disabled-COP1 substitution must fail all 1,536 short cases. Eight
output-field faults and six broken streams are rejected. Executable/archive
and actual-file hashes are recorded in ignored build artifacts.

```bash
python3 tests/roms-src/n64/watch_exceptions/build.py /tmp/watch.z64
cmake --build build --target n64_watch_test n64_watch_capture
ctest --test-dir build -R '^n64_watch_(test|exceptions)$' --output-on-failure
```

Rules follow the NEC [VR4300 manual](https://hack64.net/docs/VR43XX.pdf), §6.3.8,
§6.4.3 and §6.4.17. Nonzero WatchHi addresses exceed the VR4300 physical width;
their no-match behavior is a deterministic convention, not a hardware golden.
Failed conditional-store watches, reverse endian behavior, legacy malformed CP0
state fields, physical pipeline/exception latency and exact deferred-watch
behavior remain unverified. Tests use defined linked/cache profiles where those
operations execute. Full CPU/device accuracy is still unfinished.
