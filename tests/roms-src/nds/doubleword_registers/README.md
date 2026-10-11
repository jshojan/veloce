# Original DS ARM doubleword register/RAM corpus

The MIT generator produces an original marked native `.nds` container with
an ARM9 driver and idle ARM7. It includes no firmware, SDK, Nintendo logo,
commercial code, expected guest answers or pass verdict. Actual registers and
RAM words are emitted through the existing `dsdebug` channel and graded by a
separate host integer/address oracle.

## Supported behavior

LDRD/STRD transfer an even register and its following odd register to/from two
consecutive words. Immediate and unshifted register offsets, add/subtract,
offset/pre-index/post-index modes, base write-back and valid aliases are
supported. Address calculation wraps modulo 2^32; flags are unchanged. A PC
base without write-back uses the instruction address plus eight. Sources and
addresses are read before destinations/write-back change. Semantics and operand
restrictions follow [ARM DDI0100I A4.1.26/.102 and A5.3](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a).

The supplied little-endian, CP15 U=0 profile requires eight-byte alignment on
ARM9/ARM11. ARM7, odd pairs, R14/R15 pairs, reserved forms, invalid offset
registers and prohibited aliases stop diagnostically before data access.
ARMv6 permits base=index with write-back; ARM9 rejects it. Configurable U/A/E,
MMU attributes, exclusive monitors, physical exceptions and timing are pending.

The interpreter makes low-address then high-address bus calls in one scheduled
CPU step. Architecture does not require that order or atomic transfer. If the
second load is rejected, neither destination nor base is committed; the first
bus read may already have effects. If the second store is rejected, the first
store's effect remains, with base and registers unchanged. These diagnostic
choices are explicitly tested and do not establish physical data-abort behavior,
bus arbitration, rollback or MMIO transaction correctness.

## Executed original domain

The full cartridge grades **16,646,144 nine-word snapshots**
(**149,815,296 actual guest words**), zero mismatches in the 247.76-second
full CTest run (including controls):

| Domain | Observations |
|---|---:|
| All repeated-byte word pairs, both Q values, AL, both operations and all twelve address forms | 3,145,728 |
| All 256 offset fields, eight full-width patterns squared, all 32 NZCV/Q assignments, both operations/all forms | 12,582,912 |
| Seven additional full-width register offsets including signed/wrap boundaries, patterns and flags, all register forms | 172,032 |
| Conditions 0–13 with pattern pairs/all flags at offset 255, both operations/all forms; AL covered above | 688,128 |
| Base aliases either transferred register, no write-back, immediate/register and both directions | 32,768 |
| Store index aliases either source register, every register addressing mode/direction | 24,576 |

The target pair is at `02001808`; adjusted initial bases make every transfer
aligned even with odd offsets. Post-index write-back may produce an unaligned
base without a second access. Each case initializes distinct source/destination
seeds, two target words and neighboring guards. Alias cases change effective
source registers exactly as the independently modeled initial
register array specifies. The original corpus uses one target RAM location;
full address space, all 64-bit word pairs and every offset/condition/alias product
remain outside this finite domain.

Every row contains actual R0–R3, CPSR, the preceding guard, low target word,
high target word and following guard. The guest captures CPSR immediately after
the instruction, before output helpers change flags. The host calculates modular
addresses, conditions and the complete expected register/RAM snapshot; all nine
words and preserved guards are graded. Literal anchors pin load/store, post
write-back, wrapping pre-index and skipped conditions.

## Decoder and negative controls

`arm_doubleword_decode_test` performs **610,513** checks: register-field/profile
combinations for both operations and P/U/I/W modes, every immediate field and
selected full-width register offsets, all flags, wrap boundaries, PC reads,
alignment rejection, failed first/second bus transactions and nonzero SBZ.
Each register-field assignment uses one derived incoming flag setting; flags
are exhaustively combined separately with offset cases. It checks complete
register arrays and CPSR, stored words and transaction counts. The default
alignment restriction is model evidence, not a console measurement.

This gate found and repaired the preexisting MSR-register decoder accepting
reserved bits 11–4. Separate checks verify all fifteen supported source registers
with every flag value and all 4,096 rotated-immediate MSR encodings on ARM7,
ARM9 and ARM11; legitimate flag writes remain supported.

Before implementation the original cartridge produces zero observations. The
**1,114,880-row** reduced control passes. An executed LDRD-to-STRD mutant
produces **373,120 mismatches**, with unchanged STRD groups. Individual
register/RAM bit corruptions fail exactly one row each. Missing/malformed rows,
false completion, missing/unterminated trailers and extra data report ERROR.
PASS requires the complete ordered stream, ROM identity and frontend trailer.

DS ARM9 runs the original cartridge. DSi/3DS share the implementation, with
ARM11 and ARM7 rejection checked separately by the decoder gate. Registers,
CPSR and RAM use existing state fields; no state format change is required.
Raw captures/reports remain in `build/artifacts/owned/nds_doubleword/`.
Generator/image hashes are archived in the manifest. Firmware/retail boot,
privileged modes/physical exceptions, remaining CPU extensions, bus/device
coverage and physical timing are still unfinished; completion scores do not rise.

```sh
python3 tests/roms-src/build.py nds/doubleword_registers --original-only --check
ctest --test-dir build -R '^(nds_doubleword_registers|arm_doubleword_decode_test)$' --output-on-failure
```
