# Original RSP instruction-memory rewrite ROM

This MIT licensed N64 cartridge is generated from original Python-emitted
MIPS code. It executes all 256 eight-bit ADDIU immediate values after a CPU
store into SP instruction memory, then the same values after eight-byte
RDRAM-to-IMEM DMA. It also primes and rewrites **every one of the 1,024
aligned IMEM instruction slots** by each path, including execution across the
4 KiB bank wrap. Each case starts the RSP at the selected PC, waits for BREAK,
and writes four actual CPU-visible words to the file test channel: the RSP's
DMEM store, the rewritten IMEM instruction, SP status and SP PC. No commercial
ROM, firmware, SDK code or screenshot is used.

`run.py` independently grades **2,560 cases / 10,240 actual register words**,
rebuilds and checks the archived ROM, requires the exact file stream, and
rejects an executed cartridge whose instruction immediate is XORed with one
in all 2,560 cases. A second executed cartridge with SP DMA disarmed produces
1,279 expected DMA-path mismatches and leaves CPU-store rows correct. The
oracle also rejects a malformed register row. Results are under
`build/artifacts/owned/n64_rsp_imem_rewrite/`.

Run with `ctest --test-dir build -R '^n64_rsp_imem_rewrite$' --output-on-failure`.
This finite corpus checks functional IMEM rewrite visibility and invalidation
at every aligned slot, RSP restart, DMEM stores, HALT/BROKE status and PC.
It does not grade physical DMA latency, RSP pipeline timing or every possible
instruction encoding.
