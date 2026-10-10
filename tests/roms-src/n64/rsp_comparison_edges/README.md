# Original N64 rsp_comparison_edges corpus

MIT licensed original assembler/program; no SDK, firmware or commercial bytes.
All 8,192 groups of eight consecutive VS halfwords × four nonuniform incoming-flag profiles × eight functions. VT lanes contain 0, 1, 0x7ffe, 0x7fff, 0x8000, 0xfffe, 0xffff, 0. The element selector rotates with the input/function/profile and
collectively covers all 16 encodings. This is not the Cartesian product of
all selectors, full-width pairs and incoming flags for each operand.

**262,144 cases / 4,718,592 captured 64-bit words.** Actual VLT, VEQ, VNE,
VGE, VCL, VCH, VCR and VMRG instructions produce the destination, all three
accumulator slices, source registers, VCO/VCC/VCE, SP PC/status and MI captures.
VMUDH seeds the accumulator upper slices before the comparison; preservation
is independently graded. The guest records observations and computes no verdict.
An independent whole-integer oracle grades all 18 fields. Records have ordered
chunks of at most twelve words. An executed capture mutant must fail every
reduced case; 18 field mutations and seven malformed streams must be rejected.

```bash
python3 tests/roms-src/build.py n64/rsp_comparison_edges --original-only --check
ctest --test-dir build -R '^n64_rsp_compar' --output-on-failure
```

Rules follow the hardware-test author's primary
[nemu64-test comparison tests](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vector_arithmetic.rs).
This original cartridge has not run on a physical console. Timing/interlocks,
remaining arithmetic/logic/reciprocal instructions, complete RDP/audio and retail
boot/playability remain pending. N64 stays 40% on the fixed delivery checklist.
Artifacts: `build/artifacts/owned/n64_rsp_comparison_edges/`. Reproducible image and generator
hashes are committed in the ROM ledger.
