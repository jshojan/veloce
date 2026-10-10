# Original N64 cartridge DMA corpus

The MIT licensed Python assembler builds `pi_dma.z64` with original MIPS code,
marked `V64HB001`. The actual frontend loads the production N64 plugin and
writes guest observations to a file through the emulator debug channel.
No SDK, firmware or commercial ROM bytes are included.

All **524,288/524,288 cases** matched: 32,768 even lengths (2–65,536) × four
cartridge offsets (0, 2, 65,534, 65,536) × two RDRAM destinations × two MI masks.
Each case records sixteen 32-bit words: first/last transferred halfwords,
two adjacent guards, final DRAM/cart addresses, length readback, PI status,
MI pending/mask, CPU Cause, then post-acknowledgement status/pending/Cause,
and submitted length/cart address. These are **8,388,608 actual guest values**.
The host oracle computes bytes independently with arithmetic; the ROM contains
source patterns and case inputs, with no expected results or pass verdict.

An executed program that transfers two extra bytes fails all 256 reduced cases.
Sixteen captured-field mutations each fail one row; seven damaged streams produce
ERROR. Strict identity, domain, width, row count, completion and trailer checks
prevent truncated observations from passing. Results and JSON reports remain in
`build/artifacts/owned/n64_pi_dma/`; source/image hashes are in the ROM manifest.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_pi_test -j
ctest --test-dir build -R '^n64_(pi_test|pi_dma)$' --output-on-failure
```

This measures settled cartridge reads with eight-byte-aligned RDRAM, even
cartridge addresses and even lengths. It does not cover every source byte or
full DMA domain, asynchronous timing, odd/misaligned quirks, peripheral writes,
save media, native retail boot or physical hardware. The component gate checks
every byte of an 8 MiB transfer and broader addresses, masks, IRQ and state cases.
