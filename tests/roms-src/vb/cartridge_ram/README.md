# Original Virtual Boy cartridge RAM corpus

Repository-owned MIT guest source and 2,048-byte marked cartridge, with SHA-256
`ef5287e3a7c0e72d48de6db268f3363806e7c1e6d4f9782767db167993a2a6a3`. No SDK, firmware, commercial
cartridge bytes or imported emulator implementation is included.

## Cartridge configuration and native region

The [authored hardware reference](https://files.virtual-boy.com/download/978651/stsvb.html)
places cartridge RAM at `$06000000–$06ffffff` and describes power-of-two memory
mirrors. The repository's original cartridge profile now declares an optional
RAM module: `VVBCRAM1` follows `VVBHB001` at `size-$1f8`, followed by a big-endian
32-bit capacity at `size-$1f0`. Capacities are powers of two from 1 KiB through
64 KiB. Absence of the descriptor retains the original no-RAM profile.

Reads/writes use the native cartridge-RAM region, the board's 27-bit address
mask, little-endian data and capacity masking. Wide accesses can wrap at the
module boundary; crossing the board region rejects before any mutation. This
is functional storage for the explicitly configured original module. Commercial
cartridge RAM-width/lane wiring, native capacity detection, wait states and
physical bus timing remain unverified. Retail cartridges still do not load.

## Actual register observations

The guest uses an 8 KiB module and writes every byte value at every byte offset
through four selected mirrors: `$06000000`, `$06002000`, `$06ffc000` and
`$86000000`. It reads the target and canonical mirror, captures PSW and source/
offset registers, and reads neighboring guard bytes. Eight actual words per row
are target, canonical mirror, PSW, source, offset, preceding/following guards,
and an unrelated sentinel. LD.B sign extension is independently graded.
The ROM contains no expected answers or verdict. The production plugin forwards
actual word writes through its debug transport; the capture tool writes those
bytes to a strict register file with ROM identity and ungraded completion.

All **8,388,608 snapshots / 67,108,864 actual words** match, zero mismatches.
The host derives expected data and preservation independently of builder/CPU
arithmetic. It also checks all 8,192 actual exported battery bytes after the
guest completes. This finite byte domain does not exhaust full-width word
operands, all mirrors, module wiring or the whole platform.

The reduced positive has 1,024 rows. Executed wrong-read and missing-store
guests fail 960 rows / 960 words and 1,023 rows / 2,046 words respectively.
Eight single-field faults fail, and seven malformed streams return ERROR.
Exact identity, group order, row count/width, completion, trailer and EOF are
required. Capture binaries, sources, guest, assembler and grader are hashed
before and after execution.

## Persistence and complete states

The component checks **136,839,168 RAM bus observations**: every byte/value on
all seven declared capacities and four selected mirrors, plus every repeated-
halfword pattern on halfword/word widths at four selected wrap/end addresses.
It checks byte ownership, reset retention, exact state replay, malformed length/
capacity rejection, legacy neutral initialization and atomic board-boundary
rejection. The previous core fails the declared-capacity contract.

The production frontend discovers the actual plugin and writes an exact 8 KiB
`.sav` on unload. Reset retains it; fresh ROM load and a fresh plugin-manager
session restore every byte. Shutdown flushes it. A malformed battery file is
rejected and the new cartridge retains neutral RAM. Wrong-length API imports
reject atomically; unload removes the battery capability and buffer. Tests use
an isolated artifact directory and do not modify user save files.

RAM-bearing states use **VVBS0004**, retaining the prior CPU/WRAM/pad prefix,
then an eight-byte capacity and all cartridge bytes: **66,216 + capacity bytes**.
No-RAM ROMs keep **VVBS0003 = 66,208 bytes**. Prior VVBS0002/VVBS0003 states load
with neutral cartridge RAM. Old readers cannot consume VVBS0004. Capacity and
ROM identity validate before mutation. Active production-plugin state and
emitted-output continuations match exactly; old no-RAM instruction/controller
state regressions remain passing.

```bash
python3 tests/roms-src/build.py vb/cartridge_ram --check
cmake --build build --target vb_plugin vb_cartridge_capture vb_cartridge_ram_test vb_battery_frontend_test -j4
ctest --test-dir build -R '^vb_(cartridge_ram|cartridge_ram_test|battery_frontend_test)$' --output-on-failure
```

Files and provenance stay ignored under `build/artifacts/owned/vb_cartridge_ram/`
and `build/artifacts/owned/vb_battery_frontend/`. CI/CD remains disabled.
VB stays **25%**: complete native save-media scope, retail/native format detection
and physical module wiring are still unverified, so no saves milestone is earned.
