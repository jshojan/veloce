# Original 64-bit integer mode restriction corpus

MIT source; no SDK, firmware, private cartridge or external emulator code. Build:
`python3 tests/roms-src/n64/doubleword_privilege/build.py`.

Run `ctest --test-dir build -R '^n64_doubleword_privilege(_test)?$' --output-on-failure`.
The native gate loads the production N64 plugin and archived original cartridge;
there are no guest verdicts or expected answers. The guest writes the attempted
encoding, input Status, direct/taken-delay placement, actual handler count,
Cause/EPC/handler Status, restored kernel Status, GPR5, HI/LO and sequence into
physical RAM. The host captures those twelve 64-bit words in `V64TRAP1` records;
the independent Python oracle computes expected values and validates identity,
ordering and actual completion. Artifacts remain under
`build/artifacts/owned/n64_doubleword_privilege/`.

## Native domain

**5,768,792 cases / 69,225,504 real RAM/CP0 words**:

- 17 SPECIAL doubleword functions: nine shifts, four multiply/divide forms and
  four add/subtract forms. Six immediate shifts use every five-bit shift field;
  the other eleven use fixed valid operand encodings (203 instruction words).
- All 65,536 immediate encodings of DADDI/DADDIU, LDL/LDR, LWU, SDL/SDR, LLD/LD,
  SCD/SD, using fixed base/target registers and fixed operand seeds.
- Four 32-bit Status profiles (User/Supervisor, with unrelated KX/UX/SX flags),
  both ordinary and actual taken-delay execution, for every instruction word.

Low virtual code is mapped through the real TLB to uncached RAM. CU0 is explicitly
enabled so the original test's post-instruction code can restore kernel Status;
it does not make doubleword integer operations legal in a 32-bit user mode. The
actual RI handler records CP0 metadata, resumes at mapped low code and returns
through ERET. Misaligned/noncanonical memory operands still produce RI before
translation or data access. LLAddr/cache interference and other CPU/device
families are separate domains.

The guest diagnostic yield is a capture boundary, not a physical VI field or
instruction timing reference. After the full capture, twelve records and full
native states are compared across save/restore and production fast rollback.
An actual NOP substitution retains requested instruction tags and must fail all
**3,032 short cases**. Eleven result-field faults plus an invalid sequence field,
and six malformed identity/length/order/completion streams, are rejected.

## Component domain and limits

The component gate sweeps all 20 SPECIAL operand bits for each of the 17
functions in both 32-bit User and Supervisor modes. For the eleven major opcodes
it sweeps every immediate at fixed registers and all base/target allocations at
four offsets. Additional extension/delay/BEV cases and all valid privilege,
extension and EXL/ERL override combinations verify the blocked/allowed boundary.
Rejected instructions preserve GPRs, HI/LO, LLbit and BadVAddr, perform only the
instruction fetch and produce RI vector/EPC/BD/Status metadata.

Rules follow [NEC VR4300 chapter 16](https://hack64.net/docs/VR43XX.pdf), notably
DADD p.409, shifts pp.425–431, LD p.441 and LWU p.467. Kernel32 remains legal;
User/Supervisor need their own UX/SX bit. These tests do not prove every numerical
operand, physical exception/pipeline timing, all 32-bit address normalization,
all CP0/COP1 rights or whole hardware accuracy. N64 remains 50% of the fixed
delivery checklist. State remains S031 (9,527,616 bytes); CI/CD stays disabled.
