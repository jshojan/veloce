# Optional pinned LLVM logical-immediate encoding comparison

Reference: LLVM 20.1.3 [arm64-logical-encoding.s](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-20.1.3/llvm/test/MC/AArch64/arm64-logical-encoding.s).
Download outside the repository; no third-party reference bytes are vendored.
The checker pins SHA-256
`fc6cc18123d4892ad182a364383c4b90d2cf03eca750ceaeb00e43f65ee3114f`.

It parses LLVM's AND/ORR/EOR/ANDS immediate examples and compares all **45,312**
original guest instruction words against the eight fixed opcode/width patterns.
Register routing is checked separately; mask operand fields differ. The LLVM
source also contains an explicit `AND SP` and ORR-from-XZR encoding example.

```bash
python3 tests/reference/switch-logical-imm/check_encodings.py /tmp/aarch64-logical-encoding.s
```

All fixed-field comparisons pass. Mask result semantics are independently
verified by guest/component oracles. This is not physical execution or timing.
