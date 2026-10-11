# Original SNES stack register and RAM captures

This original 4 MiB LoROM executes 5,632 CPU cases and writes actual S, D,
DBR and RAM observations to `$21FE`. It emits no expected values or pass verdict.
The unknown-status completion hint stops execution; an independent host integer
oracle checks byte order, accessed addresses, page boundaries and final registers.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target snes_plugin veloce
ctest --test-dir build -R '^snes_stack_boundaries$' --output-on-failure
```

Current evidence: **5,632 cases**, **45,408 RAM-byte observations** and
**16,896 S/D/DBR observations** match, with byte-identical repeat captures.
The domain is 11 instructions × 256 initial stack pointers × two E modes.
In emulation mode this enumerates every permitted initial S value ($0100–$01FF).
Native mode covers that same page, **256/65,536 native S values**; the rest of
the native address domain remains pending. Operand words use complementary
byte patterns; they do not exhaust the 16-bit operand domain.

Instructions: PEA, PEI, PER, PHD, JSL, JSR (abs,X), PLD, PLB, RTL, and legacy
PHA/PLA controls. Probes observe memory before their own PHB/PLA operations;
hex output uses a jump-based helper that preserves S and measured memory.
Guard bytes distinguish pages. Indirect-call pointers stay in ROM in all
program banks. The emulator implementation is not imported by the oracle.

Reference: [WDC W65C816S datasheet, stack addressing](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf).
PLB behavior also has independently published expected values in the pinned
[gilyon catalog](../gilyon_cputest/PROVENANCE.md). Source-generated code addresses
supply the call/return stimulus; the host computes stack byte order and addresses.
The CPU restores SH=1 after special instructions finish in emulation mode while
allowing their byte accesses to cross page one; legacy operations retain wrapping.

The runner executes a separate original mutant that flips one captured S byte
with EOR. It must produce a graded failure. Missing, reordered and truncated
captures must be measurement errors. Actual captures, JSON reports, mutant
images and controls are retained under `build/artifacts/owned/snes_stack_boundaries/`.

Decimal SBC, complete instruction/flag/addressing domains, interrupts, physical
bus timing and retail compatibility remain unfinished. This ROM does not establish
whole-CPU accuracy. The archived image and generator are Veloce-authored MIT code;
no commercial ROM bytes or foreign emulator implementations are included.
