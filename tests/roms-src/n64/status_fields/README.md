# Original N64 Status field-write corpus

The MIT assembler, archived `status_fields.z64` and independent oracle are
repository-owned originals without SDK/firmware/retail assets or external
emulator code. The shared original assembler is in `../cache_coherence/build.py`.
Rebuild with `python3 build.py`.

## Hardware rule and domain

[NEC VR4300 manual](https://hack64.net/docs/VR43XX.pdf), 6.3.5 and Figures 6-5/6-6
(pp.166–169), defines Status as a 32-bit register. TS bit21 is read-only;
reserved bits23/19 read zero. Writable fields are CU/RP/FR/RE, ITS/BEV/SR/CH/CE/DE,
IM/KX/SX/UX/KSU/ERL/EXL/IE. Software writes preserve the old TS value, discard
upper bits and update these fields. Full downstream shutdown, trace, power and
reverse-endian behavior is not established by readback tests.

The native corpus executes both MTC0/DMTC0 writes, every selected sixteen-bit
source domain at four positions, four surrounding patterns and ordinary/actual
taken-delay placement: **4,194,304 cases / 50,331,648 actual RAM/register words**.
It holds source EXL=1 so changing KSU, widths, IE and masks cannot make the
subsequent capture instructions inaccessible. Some source encodings therefore
produce the same source value; unconstrained writes are checked by the component
gate. TS starts at zero and cannot be manufactured by the guest's software writes.
Code/tables use uncached KSEG1, and NOP gaps separate CP0 writes/reads and use.

At physical RAM 0x2000 the guest writes twelve actual 64-bit words: requested
write encoding, unchanged source, surround, field position, placement, previous
Status, word/full target readbacks, restored Status, PRId, Config and sequence.
An explicit Kernel64 Status=128 is restored after each observation. Diagnostic
BFF yields delimit records, not physical VI fields.

## Capture, independent grading and controls

```sh
ctest --test-dir build -R '^(n64_status_fields|n64_status_fields_test)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

The existing `n64_integer_trap_capture` producer loads the real core, captures
guest RAM into a ROM-identity-bound `V64TRAP1` stream and requires actual guest
completion. Twelve records and complete states must match ordinary and native
fast rollback. The Python oracle independently constructs source order, named
writable field positions, sign-extended word reads, full readbacks, restored
Status and unaffected PRId/Config; only cartridge construction is imported.

An executed DMFC0 EPC substitution is rejected in all **1,024 short cases**.
Twelve result/sequence faults and six identity/order/length/completion stream
faults reject. Producer/core/builder/oracle/shared assembler/recipe/archive
hashes stay fixed during capture. Ignored evidence is under
`build/artifacts/owned/n64_status_fields/`: full/short/`executed-wrong-read`
`.registers` and JSON reports, field/stream mutations and `provenance.json`.

The component gate checks **14,680,066 observations**: selected full source
domains at all four field positions/four surrounds in Kernel32/Kernel64, TS=0/1
fixtures, all source allocations and one-hot/one-clear values, privilege/width/
CU0/EXL/ERL/delay contexts. CP0 unusable exceptions precede width restrictions;
rejected writes preserve old Status except exception EXL, and preserve GPRs,
HI/LO, BadVAddr and LLbit without data accesses. The previous core fails software
TS preservation. Reset clears TS in the explicit entry profile.

TS=1 is a component fixture for write preservation, not a guest-induced hardware
TLB-shutdown test. Full duplicate-match shutdown/recovery, instruction-trace bus
behavior, low-power clock changes, reverse endian execution, reserved CP0, exact
hazards/reset/timing and full CPU/hardware remain pending. No ABI/S031 layout
change; this earns no full delivery milestone, and N64 stays **50%**.
