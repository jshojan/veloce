# Optional pinned LLVM shift/unary encoding comparison

Download the LLVM 20.1.3 [AArch64 arithmetic encoding source](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-20.1.3/llvm/test/MC/AArch64/arm64-arithmetic-encoding.s)
outside this repository. No reference source bytes are vendored. The checker
requires SHA-256 `1c82c2245a76354ab6db697412df103f3b4ee72999ad61813b5809eda3fc78a6`,
parses the variable-shift and one-operand instructions and encoding examples,
and compares all nineteen original guest opcode/width fields. Source and
destination register fields are checked separately.

```bash
python3 tests/reference/switch-bitops/check_encodings.py /tmp/aarch64-arithmetic-encoding.s
```

All nineteen forms match. This verifies encoding identity; independent guest
and component oracles verify finite result domains. Execution on physical
hardware and native timing remain pending.
