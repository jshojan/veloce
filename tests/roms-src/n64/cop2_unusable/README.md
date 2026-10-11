# Original N64 disabled-COP2 exception cartridge

MIT source and archive are original; the generator uses the repository's small
assembler. No firmware, SDK, retail code, reference emulator or expected answers
are embedded. The guest exception handler exports Cause, EPC, Status, operands,
HI/LO and sequence through raw RAM; the independent host grades the actual file.

CU2 is cleared in an explicit 64-bit kernel profile. The native corpus includes:

- LWC2/LDC2/SWC2/SDC2: all 65,536 offsets and sixteen address patterns, in ordinary
  and actual taken-branch delay slots, using base 18 and target 19.
- MFC2/DMFC2/CFC2/MTC2/DMTC2/CTC2: all target/register encodings, both placements.
- BC2 F/T/FL/TL: all offsets, both placements.
- COP2 operations: the low sixteen function bits, both placements; other function
  bits stay zero in this native cartridge.

**9,056,256 cases / 108,675,072 actual RAM/CP0 words** must show CpU code 11 and
CE=2, exact EPC/BD, handler entry EXL and ERET exit, unchanged source/HI/LO and
exact sequence/completion. BadVAddr and the absence of data bus access are checked
by the component gate, not inferred from RAM graphics.

The component gate covers **every one of the 2^26 payload encodings** for all
five COP2 major opcodes: **335,585,280 observations** including supplementary
delay/nested/BEV/register combinations. Only the instruction fetch reaches its
Memory spy. The reused GPR patterns are not all possible numerical operands;
the native cartridge does not duplicate the entire component encoding domain.

An executed cartridge substitutes disabled COP1 operations while retaining
COP2 input tags; all 14,496 short cases must fail CE grading. Seven output-field
faults and six identity/order/completion/length faults are rejected. Native full
state and ordinary/fast rollback compare twelve real records and final state.
Hashes and actual files are written under ignored build artifacts.

```bash
python3 tests/roms-src/n64/cop2_unusable/build.py /tmp/cop2.z64
cmake --build build --target n64_cop2_unusable_test n64_cop2_capture
ctest --test-dir build -R '^n64_cop2_unusable(_test)?$' --output-on-failure
```

Rules follow the NEC [VR4300 manual](https://hack64.net/docs/VR43XX.pdf), §6.4.3
and §6.4.12. Coprocessor unusable precedes reserved decode and data faults.
CP2 is an expansion coprocessor, **not the RSP vector unit**. Enabled CU2
behavior remains unsupported and uses the existing deterministic RI convention;
that behavior is not claimed as a hardware golden. Physical pipeline/exception
latency, complete CP0 and whole-CPU/device accuracy remain unverified.
