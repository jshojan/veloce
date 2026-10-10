# Original N64 VI register corpus

This MIT licensed Python assembler emits an original cartridge without SDK,
firmware or commercial bytes. The guest writes VI controls and streams actual
register readbacks; expected values and grading live only in the host runner.

**65,536 cases / 983,040 values** cover every low 16-bit input with fixed high
pattern `A5A5`, capturing CTRL, ORIGIN, WIDTH, V_INTR, BURST, V_TOTAL, H_TOTAL,
H_LEAP, H_VIDEO, V_VIDEO, V_BURST, X_SCALE, Y_SCALE, TEST_ADDR and CURRENT.
CTRL is disabled between cases; CURRENT is acknowledged and observed as zero.
This is not the independent product of every 32-bit input or active timing.

The independent oracle checks register masks. An executed WIDTH mutation fails
all 256 reduced cases; fifteen field mutations each fail a row and seven malformed
or incomplete streams are rejected as ERROR. Actual output and reports are in
`build/artifacts/owned/n64_vi_registers/`. The committed ledger records generator
and image hashes. The separate component checks every 24-bit ORIGIN, all writable
and reserved bits, all 4,096 widths and scale fields, CURRENT/IRQ and active timing.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_vi_test -j
ctest --test-dir build -R '^n64_(vi_registers|vi_test)$' --output-on-failure
```

Rules follow the [VI register reference](https://n64brew.dev/wiki/Video_Interface)
and original [Nintendo VI manual](https://jrra.zone/n64/doc/pro-man/pro30/30-01.htm).
Diagnostic staged line-buffer data is unsupported; storing TEST_ADDR does not
implement its physical test pipeline. No physical-console run has validated this
cartridge. See [the core limits](../../../../cores/n64/README.md#video-interface-and-first-framebuffer).
