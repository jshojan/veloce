# Optional Switch guest encoding reference

This validates the opcode fields of all fourteen original multiply/divide guest
forms against the actual [LLVM 20.1.3 arithmetic encoding tests](https://github.com/llvm/llvm-project/blob/llvmorg-20.1.3/llvm/test/MC/AArch64/arm64-arithmetic-encoding.s).
The downloaded reference stays outside the repository; the checker pins its
SHA-256 as well as the release tag. It checks fixed opcode/width fields and
original guest Rd/Rn/Rm routing, allowing the source examples' different register
numbers. High multiply's fixed Ra and reserved bits remain in the comparison.
This checks encoding, not executed arithmetic or physical hardware timing.

```bash
curl --fail --location https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-20.1.3/llvm/test/MC/AArch64/arm64-arithmetic-encoding.s -o /tmp/aarch64-arithmetic-encoding.s
python3 tests/reference/switch-muldiv/check_encodings.py /tmp/aarch64-arithmetic-encoding.s
```

All **14 forms** pass this comparison. No LLVM source bytes are vendored.
Result semantics are independently measured by the [original guest and oracle](../../roms-src/switch/muldiv_registers/README.md)
and the bit-serial multiply/divide component. Their domains and remaining scope
are documented in the [core README](../../../cores/switch/README.md).
