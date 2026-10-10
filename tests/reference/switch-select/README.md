# Optional pinned LLVM conditional-select encoding comparison

Reference is the LLVM 20.1.3 original AArch64 encoding test:
[arm64-arithmetic-encoding.s](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-20.1.3/llvm/test/MC/AArch64/arm64-arithmetic-encoding.s).

Download it outside the repository. The checker requires exact SHA-256
`1c82c2245a76354ab6db697412df103f3b4ee72999ad61813b5809eda3fc78a6`, parses
the eight conditional-select forms and their little-endian encoding examples,
and compares the original guest's fixed instruction fields and W/X widths.
Guest operand routing is checked separately; reference register fields differ.

```bash
python3 tests/reference/switch-select/check_encodings.py /tmp/aarch64-arithmetic-encoding.s
```

All eight forms match. Third-party source bytes are not vendored. This checks
opcode identity; result semantics use separate component and guest oracles.
Physical instruction execution and Switch hardware timing remain pending.
