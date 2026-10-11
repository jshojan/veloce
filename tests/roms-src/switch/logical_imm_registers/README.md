# Original Switch AArch64 logical-immediate register guest

Original MIT assembler and archived uncompressed NSO0. No SDK, firmware, keys
or commercial bytes; no expected answers in the guest. Actual STR captures
X0 (source), X2 (result), MRS NZCV X3, input NZCV X6 and context X5 through
emulator-only `vnxdebug`, graded independently from an output file.

## Domain

**1,449,984 rows / 7,249,920 actual 64-bit words**: sixteen boundary/mixed-bit
inputs × two initial NZCV values (9/F) × **45,312** words. AND, ORR, EOR and ANDS
execute every legal N/immr/imms encoding in both widths, including duplicate
encodings of the same mask: 3,648 W encodings + 7,680 X encodings = 11,328 masks.
All-ones element masks and reserved encodings are rejected separately.

The production decoder selects the element width from N/imms, rotates a run
of ones and repeats the element. The independent host instead constructs legal
run/element descriptors forward, then uses binary strings and per-character
logic. Fixed references check N/Z and cleared C/V for ANDS. Other forms preserve
flags. W writes zero-extend. Source register 31 is XZR; destination 31 is SP
for AND/ORR/EOR and XZR for ANDS. Component alias checks cover that distinction.

Identity, domain, every field, count, completion/trailer and EOF are strict.
The executed EOR mutant fails all **128** short rows; five field mutations fail
once each and seven malformed-stream controls error. Actual input flags are
captured, not assumed. Original ADRP/ADD fixups handle far labels; large loops
use short conditional exits and unconditional backwards branches.

The forward-element/per-bit component checks **197,296,128 snapshots**:
197,197,824 legal-mask cases (272 repeated-byte/boundary inputs, four operations,
all sixteen input NZCV values), plus 98,304 register/alias/SP/XZR cases. All
**20,224** reserved N/immr/imms/operation encodings are rejected with unchanged
registers, SP and flags. The initial reference assertion for a repeated
32-bit element in X width was corrected to `0x100000001`; it is not scalar `1`.
The previous core failed the new component and emitted no guest register row.

Production ABI checkpoint/replay compares actual output and complete
`VNXS0001` states. [Pinned LLVM opcode fields](../../../reference/switch-logical-imm/README.md)
match every generated instruction word. These are finite source domains,
not all full-width source values, complete ISA/hardware accuracy or native timing.

```bash
python3 tests/roms-src/build.py switch/logical_imm_registers --original-only --check
cmake -S . -B build
cmake --build build --target veloce switch_plugin switch_logical_imm_test switch_plugin_state_test -j
ctest --test-dir build -R '^switch_logical_imm' --output-on-failure
```

Artifacts: `build/artifacts/owned/switch_logical_imm/`. Switch stays 25% delivery
completion. Full CPU/extensions/exceptions, memory/MMU, native boot/HOS, retail
compatibility, GPU/audio/controllers/devices, saves/netplay and physical/native
corpus remain unfinished. CI/CD remains disabled.
