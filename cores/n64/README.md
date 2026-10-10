# Experimental Nintendo 64 core plugin

`n64_plugin` builds `n64.so`/`n64.dll`/`n64.dylib` and implements emulator ABI v2.
The frontend discovers it alongside the other core plugins. It executes original
MIPS III integer homebrew ROMs with 64-bit GPRs, big-endian memory, delay slots,
HI/LO, and basic CP0 exception context. **It does not run retail N64 games.**
Full-core completion is unknown; the available corpus measures defined results
on selected operands, not a percentage of N64 hardware implemented.

## Implemented execution

- Word and doubleword add/subtract, trapping and nontrapping forms; logic,
  signed/unsigned comparisons, immediate arithmetic and logic.
- Word and doubleword shifts, immediate/variable forms (including +32 forms).
- Word and doubleword signed/unsigned multiplication/division, MFHI/MFLO/MTHI/MTLO.
- J/JAL/JR/JALR, BEQ/BNE/BLEZ/BGTZ, REGIMM sign branches and link forms,
  plus likely variants with an annulled untaken delay slot.
- Aligned byte/halfword/word/doubleword loads/stores, sign/zero extension.
- Basic MFC0/MTC0, ERET from EXL, integer overflow, SYSCALL/BREAK, reserved
  instruction, alignment/address exception EPC/BD context. CP0 is partial;
  register masks, privilege modes, interrupt delivery and precise bus/TLB
  exception classes still need implementation.
- 8 MiB RDRAM, KSEG0/KSEG1 aliases, read-only cartridge mapping, and 8 KiB SP
  memory. SP memory is storage only; no RSP execution or DMA exists yet.

Implementation is original C++ and grounded in the
[NEC VR4300 user manual](https://hack64.net/docs/VR43XX.pdf), especially the
instruction descriptions and exception EPC/BD behavior. No SDK, Nintendo IPL,
PIF ROM, copyrighted boot logo, or commercial ROM is included.

## Explicit homebrew entry profile

Only images marked `V64HB001` at normalized header offset `$34` load. They also
need the standard `$80371240` big-endian cartridge prefix, an aligned entry in
KSEG0/KSEG1 RAM at header offset `$08`, and a payload starting at ROM offset
`$1000`. The loader zeroes RAM, copies that payload to physical RAM `$400`, and
enters the header's address with zero GPRs/HI/LO and interrupts disabled in
kernel mode. This is a repository-defined test entry profile, **not PIF/IPL/CIC
boot emulation**. The entry must point within the copied payload. The loader
rejects unmarked images and malformed/truncated headers without replacing an
already loaded machine. Maximum image size is bounded by the RAM payload size.

`.z64` big-endian, `.v64` byte-pair swapped and `.n64` four-byte reversed images
are normalized before execution. Test file CRC32 identifies the entire raw
input file. State identity additionally binds the normalized full image.

Two emulator-only observation registers live at uncached `$BFF00000`: `SB`
forwards one character to `ITestSink`; `SW` to `$BFF00004` ends with an unknown
verdict. These addresses are debug instrumentation, not Nintendo hardware.
The frontend owns register files. Ordinary 16-bit RAM-watch API accesses map
the first 64 KiB of RAM; a wider inspection API remains needed.

## Evidence and reproduction

The original [integer ROM](../../tests/roms-src/n64/integer_registers/README.md)
records 28,160 observations for 20 operations. Its independent Python oracle
matches all **28,127 defined results**; 33 divide-by-zero/overflow observations
are retained but unscored. All low-byte shift amounts are exercised on 16
patterns per word width, and arithmetic/HI/LO operations use 16×16 patterns.
This is not exhaustive over all 32-bit/64-bit values or the full ISA.

The original [doubleword multiply/divide ROM](../../tests/roms-src/n64/muldiv64/README.md)
adds **262,623/262,623 defined LO/HI pairs** for DMULT/DMULTU/DDIV/DDIVU,
with 263,168 total observations and 545 undefined inputs unscored. It executes
every signed/unsigned byte pair as 64-bit inputs, plus 16×16 full-width
boundaries. Products use portable 32-bit limbs in the core and independent
Python 128-bit arithmetic in the oracle. Signed division uses unsigned
magnitudes so min/-1 and divisor zero cannot invoke undefined host arithmetic.
The full 64-bit operand domain and multiply/divide stalls/hazards remain pending.

`n64_machine_test` checks taken/untaken/likely branch paths, delay execution,
JAL/JALR link/alias cases, $zero, word/doubleword overflow and EPC/BD, nested
exceptions, ERET, SYSCALL/BREAK/RI, alignment faults, big-endian load/store
extension, aliases, ROM protection, doubleword multiply/divide references and
full-width HI/LO state continuation, pending-delay state restore, and actual
shared-plugin state continuation equality. It also checks all three image byte
orders, invalid state/load rejection, and unload. The ROM runner separately
executes all three archive formats through actual frontend discovery.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target veloce n64_plugin n64_machine_test -j
ctest --test-dir build -R '^n64_' --output-on-failure
```

`VN64S001` states store explicit big-endian scalars, all GPR/CP0 registers,
HI/LO, both PCs, pending delay state, instruction/frame counters, completion,
RAM and SP storage. ROM data is immutable and referenced by identity. Size,
version, ROM identity and basic field validation precede restoration. The
instruction counter is an execution measure, **not actual VR4300 cycles**;
each frontend frame is a bounded 100,000-instruction scheduling quantum,
not an emulated VI field. The nominal metadata rate is NTSC/93.75 MHz only.

## Remaining hardware and modern features

PIF/IPL/CIC and retail boot, TLB/address modes, caches and pipeline timing,
full CP0/interrupts, unaligned merge loads/stores, LL/SC, traps,
FPU, RSP/vector instructions, RDP, VI, AI, PI/SI/MI/RI and DMA, controllers,
save media, video/audio output, expansion devices and physical hardware
validation remain. Unimplemented opcodes raise a reserved-instruction exception.
Unsupported memory access currently uses an address exception approximation.
There is no N64 netplay capability yet. Video is a blank buffer, audio is empty,
and input is not routed to SI/PIF. Windows/macOS builds and rendered GUI usage
remain unverified. The external four-platform suite matrix does not yet include
N64; its original ROM and component gates run in the owned-hardware CI job.
