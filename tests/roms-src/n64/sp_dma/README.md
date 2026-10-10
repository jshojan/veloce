# Original N64 SP DMA register corpus

This MIT licensed Python assembler emits an original marked N64 cartridge,
without SDK, firmware or commercial bytes. Guest code initializes RAM and SP
byte ramps (with a distinct IMEM pattern), writes SP transfer registers and
captures actual first/last bytes, final SP pointer, status, full and busy into
register-output files. Guest code contains no expected data or pass verdict.

All **65,536/65,536 cases** match **393,216 guest values**: both directions ×
both SP banks × start offsets 0, 8, 4080 and 4088 × every 12-bit raw length.
Source/destination low address bits are deliberately nonzero. Lengths round up
to eight-byte multiples; bank crossing wraps within the selected 4 KiB bank.
The host oracle uses arithmetic independently of the core and assembler helpers.
An executed length-bit mutation fails all 256 reduced cases. Six field mutations
fail one row each, and seven invalid streams produce ERROR. Strict identity,
domain, width, count, completion and trailer checks reject incomplete output.
Raw observations and reports remain under `build/artifacts/owned/n64_sp_dma/`;
source/image hashes are in the manifest.

The ROM measures transfer endpoints and the declared control fields. The C++
component additionally checks all transferred bytes and untouched RAM/bank guards
for that length/bank/direction domain. It checks 262,144 transfers covering every
8-bit count × every aligned 12-bit skip × both directions, with eight-byte rows.
These row tests do not exhaust the independent length × count × skip product.
It also checks real CPU-cache staleness, state replay, bounds/stride rejection,
interrupts and loaded-plugin integration.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_sp_test -j
ctest --test-dir build -R '^n64_(sp_dma|sp_test)$' --output-on-failure
```

Behavior follows the [RSP programmer guide](https://bukosek.si/hardware/collection/sgi-o2/n64-rsp-programmers-guide.pdf)
and author's [DMA hardware tests](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/sp_memory/dma.rs).
External test code was read, not copied or executed here. Transfers settle
synchronously; busy/full queues and physical clocks are not modeled. DMA outside
implemented RAM and non-eight-byte skips reject atomically. Exact address/length
readback, open-bus values, unaligned skip behavior and physical validation remain
pending. RSP execution, graphics/audio and retail playability remain unverified.
