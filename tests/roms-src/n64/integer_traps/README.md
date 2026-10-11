# Original N64 integer trap corpus

Repository-owned MIT source, emitted without an SDK, IPL or firmware bytes.
The cartridge executes all twelve VR4300 register/immediate integer trap forms
in an explicit kernel 64-bit profile. It prints no expected answers or PASS.
Its own exception handler captures actual EPC, Cause and Status, resumes with
ERET, and stores raw source/handler/HI/LO observations to RAM `$00002000`.
Uncached instruction blocks are written and fenced with SYNC before execution.
An emulator-only debug yield allows the production plugin harness to read the
RAM through its existing memory API and write a binary register file.

## Scope

**15,728,640 cases / 188,743,680 actual 64-bit words**:

- Six register forms: every 10-bit software code, every ordered pair of 16
  full-width operand patterns, ordinary and actual taken-branch delay execution.
- Six immediate forms: every 16-bit immediate, each of 16 operand patterns,
  ordinary and actual taken-branch delay execution. Unsigned immediates still
  sign-extend before comparison.
- Twelve observations per case: requested encoding, two source GPRs, placement,
  actual handler-entry count, captured Cause/EPC/Status, post-return Status,
  HI/LO guards and actual guest sequence counter.

The independent host oracle converts signed values with Python integers and
checks the actual file, ROM identity, domain, ordered sequence and real completion.
The guest only performs the operation and reports its observations. A short
15,360-case control spans negative immediates and full-width operand boundaries.
Actually executing opposite conditions must fail all 15,360 cases. Twelve
individual field faults, six malformed/identity/footer controls and a reordered
stream are rejected. Native full-state/fast rollback replay compares actual
RAM output and complete states after interruption in the guest output routine.

The component gate adds **37,810,176 observations** across all source register
encodings (including `$zero`/aliases), all code/immediate fields, defined 32-bit
operands and the 64-bit profile, nested EXL/BEV/EPC behavior, register preservation
and actual taken/untaken ordinary/likely/annulled branch execution.

This is exhaustive over the named encoding fields and listed operand patterns,
not all 32-bit/64-bit operand pairs, privilege/pipeline timing, or the entire CPU.

```bash
python3 tests/roms-src/build.py n64/integer_traps --original-only --check
cmake --build build --target n64_plugin n64_integer_trap_test n64_integer_trap_capture
ctest --test-dir build -R '^n64_integer_trap(s|_test)$' --output-on-failure
```

Files: `build/artifacts/owned/n64_integer_traps/`. Full binary output is about
1.51 GB; its SHA is streamed rather than loading that file into memory.
Semantics follow the NEC [VR4300 instruction/exception manual](https://hack64.net/docs/VR43XX.pdf),
§6.4.14 and the TEQ/TGE/TLT/TNE instruction descriptions. Physical hazard/CPI
behavior, LL/SC, full CP0/devices and a full hardware corpus remain pending.
