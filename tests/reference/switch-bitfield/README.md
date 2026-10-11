# Optional pinned LLVM bit-field/extract encoding comparison

Reference: LLVM 20.1.3 [arm64-bitfield-encoding.s](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-20.1.3/llvm/test/MC/AArch64/arm64-bitfield-encoding.s).
Download outside this repository; third-party source bytes are not vendored.
The checker pins SHA-256
`285401d5bc03ad13fd29f4d3352611917d4c8ecf299336613fd88de92664eba6`.

It parses LLVM's BFXIL/SBFX/UBFX and EXTR encodings, compares every one of the
**15,456 generated guest words** against the eight fixed opcode/width patterns,
and checks register routing and legal immediate ranges. The aliases identify
BFM/SBFM/UBFM underlying instructions. Reference register/immediate fields differ
from the guest and are masked for the fixed-pattern comparison.

```bash
python3 tests/reference/switch-bitfield/check_encodings.py /tmp/aarch64-bitfield-encoding.s
```

All comparisons pass. This establishes encoding identity, not physical execution
or Switch hardware timing. Separate guest/component oracles grade finite results.
