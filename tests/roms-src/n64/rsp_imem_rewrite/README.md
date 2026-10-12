# Original RSP instruction-memory rewrite ROM

This MIT licensed N64 cartridge is generated from original Python-emitted
MIPS code. It executes 256 ADDIU immediate values after a CPU store into SP
instruction memory, then the same 256 values after eight-byte RDRAM-to-IMEM
DMA. Each case starts the RSP at PC 0, waits for BREAK, and writes three
actual CPU-visible register words to the file test channel: the RSP's DMEM
store, the rewritten IMEM instruction, and SP status. No commercial ROM,
firmware, SDK code or screenshot is used.

`run.py` independently grades **512 cases / 1,536 actual register words**,
rebuilds and checks the archived ROM, requires the exact file stream, and
rejects an executed cartridge whose instruction immediate is XORed with one.
It also rejects a malformed register row. Results are under
`build/artifacts/owned/n64_rsp_imem_rewrite/`.

Run with `ctest --test-dir build -R '^n64_rsp_imem_rewrite$' --output-on-failure`.
This finite corpus checks functional IMEM rewrite visibility, RSP restart,
DMEM stores, and HALT/BROKE status. It does not grade physical DMA latency,
RSP pipeline timing, all instruction encodings, or all possible IMEM addresses.
