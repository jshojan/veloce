# Original Game Boy byte ALU corpus

The original 32 KiB ROM executes `ADD A,E`, `ADC A,E`, `SUB A,E`, `SBC A,E`,
`AND A,E`, `XOR A,E`, `OR A,E`, and `CP A,E` for all 256 accumulator values,
all 256 operand values, and all 16 incoming Z/N/H/C combinations. That is
**8 × 256 × 256 × 16 = 8,388,608 observations**. This includes flags that do
not affect an operation, so their replacement is measured too. Only register
E addressing is covered; other operand locations, instruction timings, and
other CPU instructions remain separate domains.

The ROM restores input A/F through `PUSH BC; POP AF`, executes one operation,
and captures the actual output through `PUSH AF; POP BC`. It emits all A/F
pairs as lowercase hexadecimal in `REG af=` rows of 32 cases, ordered by
operation, incoming flags, lhs, then rhs. Interrupts, LCD, and sound are
turned off. The stack resides in WRAM. The low four F bits are observed and
must be zero. CP must retain the original A while setting subtraction flags.

The independent Python oracle uses integer sums/differences, nibble carry/borrow,
and bitwise operations. It checks every pair, with hand-calculated reference
points for carry, half carry, borrow, zero, logic, and CP preserving A. It imports
no emulator code and accepts no ROM PASS verdict. The opcode/flag definitions
are grounded in [Pan Docs](https://github.com/gbdev/pandocs/blob/master/src/CPU_Instruction_Set.md),
[register/flag layout](https://github.com/gbdev/pandocs/blob/master/src/CPU_Registers_and_Flags.md),
and the [RGBDS instruction reference](https://github.com/gbdev/rgbds/blob/master/man/gbz80.7).

## Measurement channel and limits

Each character is written to SB (`$FF01`), followed by SC (`$FF02`) = `$81`.
Veloce's existing test channel observes SB at that start request. This corpus
intentionally does not wait for physical link transfers between characters;
it measures CPU results through the register observation hook, **not serial
transfer timing or delivery on a physical cable**. No screenshot or stdout
verdict is scored. It uses the core's post-boot entry and includes no Nintendo
logo; retail boot ROM acceptance and physical hardware execution are unverified.

`LOG complete cases=8388608` and `END observations_complete` declare completion
without grading. The file sink writes an unknown-status trailer (`status=-1`).
The host requires all groups, exact row sizes/counts, a matching ROM identity,
completion/trailer records, and no trailing data before it can return PASS.

## Reproduce and run

```bash
python3 tests/roms-src/build.py gb/alu_exhaustive
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce gb_plugin
ctest --test-dir build -R '^gb_alu_exhaustive$' --output-on-failure
```

The builder emits SM83 opcodes, resolves labels, and calculates the cartridge
header/global checksums using only Python. Its committed binary and generator
SHA256 values are recorded in `rom_manifest.json`. Source and ROM are MIT licensed.

The runner saves every A/F observation in
`build/artifacts/owned/gb_alu/alu_exhaustive.result`, with group counts,
mismatches, examples, ROM hash, frame count, and cycle count in the JSON report.
A separate 16-value operand ROM changes one executed ADD to SUB. Only that
group must fail. Missing rows, malformed rows, reordered groups, a false PASS
record, missing/unterminated trailers, and data after the trailer must produce
measurement errors. Their raw files and JSON reports are retained too.

For development, `run.py --smoke` runs all eight operations and all incoming
flags on operand values 0–15 (32,768 pairs). A smoke pass is not the full corpus.
Full-core completion is not measured by this suite.

Measured 2026-10-09: **8,388,608/8,388,608 A/F pairs matched**, with zero
mismatches across all 128 groups (100% of this declared domain). Completion
occurred after 98,441 frontend frames. The executed mutation failed only its
changed group; all seven damaged-file controls produced measurement errors.
