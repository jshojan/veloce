# Optional pinned LLVM conditional-compare encoding comparison

Reference: LLVM 20.1.3 [arm64-arithmetic-encoding.s](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-20.1.3/llvm/test/MC/AArch64/arm64-arithmetic-encoding.s).
Download outside this repository; no reference source bytes are vendored.
The checker pins SHA-256
`1c82c2245a76354ab6db697412df103f3b4ee72999ad61813b5809eda3fc78a6`.

It parses eight CCMP/CCMN W/X register/immediate examples and compares every
one of the guest's **2,112 base words** (all conditions, four register forms,
and every five-bit immediate across four immediate forms) against the fixed
opcode/width fields. Guest operands are checked separately; all sixteen
fallback nibbles are appended by the original builder.

```bash
python3 tests/reference/switch-ccmp/check_encodings.py /tmp/aarch64-arithmetic-encoding.s
```

All comparisons pass. Result semantics are independently graded. This checks
encoding identity; physical instruction execution/native timing remain pending.
