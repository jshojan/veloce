# Original N64 RSP divider/move corpus

MIT licensed original assembler/program; no SDK, firmware or commercial bytes.
**5,898,240 cases / 123,863,040 actual 64-bit captures.** All 65,536 halfword
values × nine instructions × ten latch/operand profiles are enumerated:
VRCP, VRCPL, VRCPH, VMOV, VRSQ, VRSQL, VRSQH, VNOP and VNULL.

Profiles zero/one sweep selected VT values with an unprimed divider or a primed
0x1234 high half. Profiles two through nine sweep every high half with low halves
0, 1, 0x7fff, 0x8000, 0xffff, 0xe834, 0x1234 and 0x8001. VT is broadcast;
element and destination encodings rotate with input/function. These originals
do not cover every mixed-vector selector/alias or every 32-bit input value.

Actual guest instructions capture destination, all accumulator slices, source
vectors, VCO/VCC/VCE, SP PC/status and MI. A subsequent high instruction exposes
the operation's full output high half. A replay of the operation from the same
seed, followed by low/high probes, exposes whether a pending input was consumed.
Each row has 21 fields, in ordered chunks of at most twelve words. The guest
computes no verdict. Independent rational quantization, Python integer square
roots and latch algebra grade every field. An executed capture mutant must
fail all 180 reduced cases; 21 field mutations and seven malformed streams must
be rejected. Artifacts: `build/artifacts/owned/n64_rsp_divider/`.

```bash
python3 tests/roms-src/build.py n64/rsp_divider --original-only --check
ctest --test-dir build -R '^n64_rsp_divider' --output-on-failure
```

The reference is the hardware-test author's primary
[reciprocal, square-root and move tests](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vmov_vrcp.rs).
The production tables are independently derived with integer inequalities;
no external implementation code or tables are copied. This original has not
run on a physical console. Timing/interlocks, full-width input products, rare
vector operations and complete RDP/audio/retail compatibility remain ungraded
or unfinished. N64 stays 40% on the fixed delivery checklist.
