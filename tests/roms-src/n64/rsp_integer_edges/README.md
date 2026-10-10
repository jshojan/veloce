# Original N64 rsp_integer_edges corpus

MIT licensed original assembler/program; no SDK, firmware or commercial bytes.
All 8,192 groups of eight consecutive VS halfwords × all 32 incoming lane-flag combinations × eleven functions. VT lanes are 0, 1, 0x7ffe, 0x7fff, 0x8000, 0xfffe, 0xffff, 0. Element selection rotates with group/function and covers all 16 encodings collectively, rather than every selector and VT partner for every VS value.

**2,883,584 cases / 51,904,512 actual captured 64-bit words.** The CPU executes
VADD, VSUB, VABS, VADDC, VSUBC, VAND, VNAND, VOR, VNOR, VXOR and VNXOR on the
RSP and captures destination, all accumulator slices, sources, VCO/VCC/VCE,
SP PC/status and MI. VMUDH seeds the upper accumulator slices. The guest computes
no verdict. An independent unbounded-integer/per-bit truth-table oracle grades
all eighteen fields, including unclamped accumulator LOW16 versus saturated
destination. The CPU iterates flag patterns before advancing the compact table;
this keeps the reproducible archive below 1 MiB.

Output uses ordered chunks of at most twelve 64-bit words. An executed capture
mutant must fail all reduced cases; eighteen individual field mutations and
seven malformed streams must be rejected. Artifacts:
`build/artifacts/owned/n64_rsp_integer_edges/`.

```bash
python3 tests/roms-src/build.py n64/rsp_integer_edges --original-only --check
ctest --test-dir build -R '^n64_rsp_integer_' --output-on-failure
```

Rules follow the hardware-test author's primary
[nemu64-test arithmetic tests](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vector_arithmetic.rs).
These original cartridges have not run on a physical console. Full-width
operand-pair products, physical timing/interlocks, reciprocal/square-root/move
and rare vector operations, complete RDP/audio and retail boot/playability remain
pending. N64 stays 40% on the fixed delivery checklist.
