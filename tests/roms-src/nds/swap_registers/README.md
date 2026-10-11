# Original Nintendo DS SWP/SWPB register and RAM corpus

The MIT Python generator emits an original `.nds` cartridge with separate ARM9
and ARM7 programs. There is no SDK, firmware, commercial code, Nintendo logo,
expected-answer table or guest pass verdict. The experimental marked-segment
loader is used; native firmware/retail boot is not established by this profile.
ARM7 waits for ARM9's shared-RAM completion signal before emitting its rows.

## Domain and observations

The finite corpus matches **33,300,480/33,300,480 six-word observations**,
zero mismatches:
22,200,320 ARM9 and 11,100,160 ARM7 (199,802,880 actual guest words).
Each row records destination, CPSR, stored source register after execution,
unchanged address register, the complete affected RAM word and a neighboring
guard word. CPSR is saved before output instructions change flags. Byte exchanges
must preserve the other three bytes. Guest MMIO writes supply observations;
the frontend supplies the file identity and completion trailer.

Each CPU executes 160 groups:

- **31,457,280 observations**: every pair of 256 repeated-byte words, aligned
  SWP and SWPB at all four byte lanes, distinct and identical source/destination,
  every incoming NZCV state (ARM9 also every incoming Q), AL condition.
- **1,843,200 observations**: every pair of sixteen full-width patterns,
  both alias forms, the same five width/lane combinations, all fifteen ordinary
  conditions and every incoming NZCV/ARM9 Q state.

The full-width patterns include signed extrema, all single-byte lanes,
nonuniform bytes, guards, alternating bits, zero/one/minus one and neighbors.
This enumerates the declared byte-pair/boundary domain; it does not enumerate
all 32-bit word pairs, every memory region or physical bus transactions.

## Independent grading and controls

The host uses a four-byte array to model old RAM, the loaded value and written
bytes. A separate Boolean condition table controls whether the exchange occurs.
All six captured words are graded, including every CPSR bit and guard byte;
no flags are masked. Literal anchors cover word exchange, aliased byte exchange
and skipped conditions. Semantics and operand restrictions follow
[ARM DDI 0100I, A4.1.108–109](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a).

The reduced **266,880-observation** control retains all widths/lanes, aliases,
conditions and flag states. An executed mutant replaces SWP/SWPB with LDR/LDRB:
**117,888 observations** fail because required memory writes are absent.
Single destination/RAM bit mutations each fail exactly one row. Missing or
malformed rows, false completion, missing/unterminated trailers and extra data
report ERROR. PASS requires the full stream and strict identity/trailer.
Before implementation the original cartridge stops at its first SWP with zero
completed observations.

The native decoder gate checks **1,176,000 executions** across ARM7/ARM9/ARM11,
all legal non-PC register field combinations, all flags and width/lane choices.
It checks transaction order, base/destination/source preservation, and rejection
of PC or base aliases before a data transaction. Failed read never stores;
failed store never commits the destination. Failed conditions do not access data.
Unaligned word rotation/aligned writes have a separate legacy-model check;
they are not part of the original-ROM hardware grade.

## Limits and artifacts

The interpreter executes the whole read/store/register commit within one CPU
step. Its cooperative dual-CPU scheduler cannot execute a peer in the middle.
The original corpus serializes its emitting CPUs; it does not independently
verify contested physical bus locking/arbitration or interrupt timing.
Precise data-abort delivery, CP15 alignment controls, MMU/MPU/cache, ARMv6
exclusive-monitor integration and big endian behavior remain incomplete.
Unpredictable encodings and failed bus accesses stop diagnostically, without
claiming hardware exception behavior. ARM11 uses the supplied legacy A/U=0
model; other alignment modes are not supplied.

Raw observations, mutated captures, logs and JSON reports remain in
`build/artifacts/owned/nds_swap/`. The archived image and generator hashes are
recorded in the ROM manifest. Shared decoder support includes DSi and 3DS;
this native guest corpus is graded on both DS CPUs. Full CPU and platform
milestones remain incomplete, so percentages are unchanged.

```sh
python3 tests/roms-src/build.py nds/swap_registers --original-only --check
ctest --test-dir build -R '^(nds_swap_registers|arm_swap_decode_test)$' --output-on-failure
```
