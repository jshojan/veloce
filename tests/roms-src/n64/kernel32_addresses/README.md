# Original N64 Kernel32 wide integer addresses

The repository-owned MIT cartridge and Python assembler contain no SDK,
firmware, retail ROM bytes or external emulator code. The archived
`kernel32_addresses.z64` reproduces using `python3 build.py`.

## Hardware rule and domain

[NEC VR4300 manual](https://hack64.net/docs/VR43XX.pdf), chapter 16, explicitly
ignores VA63:32 during address creation for wide integer memory forms in 32-bit
Kernel mode. LD p.441, LDL p.445, LDR p.448, LLD p.455, LWU p.467,
SCD p.490, SD p.491, SDL p.496 and SDR p.499 describe this rule. KX selects the
kernel address width; EXL/ERL force effective kernel privilege. The chapter 5
User-mode discussion on p.128 warns that invalid upper bits can fault. This
suite does not assert that every memory instruction or privilege ignores them.

The native cartridge executes all **65,536 immediate encodings** for nine
families: LDL, LDR, LWU, SDL, SDR, LLD, LD, SCD and SD. Each encoding runs at
four profiles (Kernel32/Kernel64, zero/FEDCBA98 upper base) and in ordinary or
actual taken-branch delay slots: **4,718,592 cases / 56,623,104 captured words**.
Base reconstruction preserves the signed offset while selecting each of eight
byte lanes. Cached mapped data and a real LLD reservation make the SCD success
controls defined; uncached code avoids a host-generated execution shortcut.

Every attempt exports twelve actual 64-bit guest RAM words at physical 0x2000:
requested encoding, input Status, placement, preserved base, handler count,
Cause, EPC, BadVAddr, result register, final eight memory bytes, restored Status
and sequence. A six-instruction original handler captures CP0 fields and uses
ERET. Alignment and extended-address errors preserve the target and memory;
Kernel32 successful loads/merges/stores are independently graded numerically.
BFF record yields are diagnostic capture boundaries, not physical VI fields.

## Execution and independent grading

```sh
ctest --test-dir build -R '^(n64_kernel32_addresses|n64_kernel32_address_test)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

The existing `n64_integer_trap_capture` producer loads the real plugin, reads
actual RAM, verifies actual guest completion and writes the identity-bound
`V64TRAP1` stream. Twelve records and complete native states must match ordinary
and fast rollback. `run.py` independently computes encoding order, offsets,
base/address values, alignment, exception metadata, merge lanes and memory
bytes; it imports only the assembler's cartridge constructor.

An executed NOP substitution is rejected in all **1,152 short cases**. Each
result/sequence field is mutated, and six identity/order/length/completion stream
faults are rejected. Core, producer, source, oracle and archived ROM fingerprints
must stay fixed throughout capture. Ignored evidence lives under
`build/artifacts/owned/n64_kernel32_addresses/`: full/short/NOP `.registers` and
JSON reports, field/stream mutations and `provenance.json`.

The component gate checks **4,730,235 observations**, covering every immediate
with four noncanonical high halves and two placements, compatibility segment
and low-half carry boundaries, all privilege/extension/EXL/ERL combinations,
valid extended TLB addresses, and ordinary word/FP invalid-address controls.
Its Kernel32 pair comparison proves address/fault/bus invariance; the native
Python oracle supplies separate numerical load/store/merge expectations.

These tests do not exhaust all 64-bit bases or operand values, physical
cache/atomic interference, arbitrary TLB page layouts, reverse endian behavior,
ordinary word/FP/CACHE address corner cases, or complete CPU/hardware/timing.
They earn no additional complete delivery milestone: N64 remains **50%**.
