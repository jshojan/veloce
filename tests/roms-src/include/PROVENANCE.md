# Provenance: `tests/roms-src/include/` (SNES-2 emitter includes)

This directory holds the "one include per assembler" the plan describes
(section 5/6): a per-dialect implementation of the shared `VT_*` emitter
vocabulary (`docs/testing/VELOCE-RESULT.md` section 6) for the SNES's
`$21FE`/`$21FF` port. Three dialects were written for SNES-2 (`ca65`, `bass`,
`asar`); this file records how each was verified, and a toolchain defect
found along the way that blocks two of the three assemblers this plan pins
for later SNES work items (SNES-7, SNES-8).

## What each include is for

| File | Dialect | Serves (per the plan) |
|---|---|---|
| `snes/veloce-test.ca65.inc` | ca65 `--cpu 65816` | Sour, gilyon (S-CPU side), Blargg SNES shell (SNES-3..5) |
| `snes/veloce-test.bass.inc` | ARM9/bass v18 | krom/PeterLemon CPU + SPC700 (SNES-7) |
| `snes/veloce-test.asar.inc` | asar v1.91 | byuu/jonasquinn xkas sources (SNES-6) |

All three implement the identical vocabulary (`VT_BEGIN`/`VT_TEST`/`VT_PASS`/
`VT_FAIL`/`VT_REG_BEGIN`/`VT_REG8`/`VT_REG16`/`VT_REG_END`/`VT_LOG_STR`/
`VT_END`) over the same primitive (`STA $21FE`), and the same two runtime hex
printers (`vt_hex8`, `vt_hex16`) for values only known at runtime. Neither
`ca65` nor `bass`/`asar` has a compile-time string-character-extraction
function in common: `ca65` has one (`.strat`), so its include builds every
compile-time string with it and touches no RAM at all; `bass` and `asar` do
not, so both use the standard 6502/65816 "inline string after `JSR`" idiom
instead (the routine reads its own return address off the stack via
stack-relative addressing, prints up to the following `NUL`, then patches the
stacked return address so `RTS` resumes past the terminator) -- this needs
one small, overridable zero-page pointer (`VT_PTR_ZP`, default `$EE-$EF`).

## Verification: ca65 and asar, both confirmed end to end

Both were built with the pinned toolchain image and actually run against the
real `veloce` binary (not just syntax-checked):

- **ca65**: `tests/roms-src/snes/veloce_smoke/` (committed) builds
  `veloce_smoke.sfc` via `tests/roms-src/build.py snes/veloce_smoke` and
  exercises every macro (port probe read and branch, a genuine `PASS`, a
  deliberate fixed-value `FAIL`, an 8-bit and a 16-bit `REG` line from live
  register loads, a `LOG` line, `END`). Running it produced a well-formed
  `VELOCE-RESULT/1` transcript (`CHECK 1 PASS ...` / `CHECK 2 FAIL ... exp=1
  got=2` / `REG probe=56 x=1234` / `END 1/2`), and two runs were
  byte-identical, including the trailer -- the determinism property SH-5's
  selftest will check.
- **asar**: verified the same way with a throwaway test ROM (not committed;
  no suite depends on asar yet -- that is SNES-6) built with `asar
  --no-title-check`, exercising the identical macro set including `VT_FAIL`,
  both `REG` widths, `VT_LOG_STR` and `VT_END`. Two runs were also
  byte-identical.

Both required fixing real bugs found only by actually running the ROM, not by
reading the assembler's own docs:

1. **ca65**: `.strat`/`.sprintf` are real ca65 pseudo-functions, but passing
   `vt_putc_imm .strat(str1, I)` as a *macro argument* (not a bare
   expression) makes ca65's classic-macro-call parser split on the comma
   inside `.strat(...)`, miscounting the argument list ("Too many macro
   parameters"). Fix: wrap it, `vt_putc_imm {.strat(str1, I)}` -- `{...}`
   groups an argument for exactly this reason (the pattern already exists in
   this codebase's own upstream sources, e.g. `print_str
   {"Failed",newline}` in the Blargg SNES shell).
2. **asar**: `ldy #0` right before the byte-copy loop, executed with 16-bit
   index registers in effect (this include's own stated precondition), was
   *silently* assembled as an 8-bit immediate (`ldy #$00`, one operand byte)
   because asar guesses an ambiguous immediate's width from the operand
   value when no `.b`/`.w` suffix is given, and `0` fits in either. At
   runtime the CPU (correctly, since X was really 16-bit) read a second
   operand byte that was actually the next opcode, desynchronizing
   instruction decoding from there on -- the ROM ran and produced a `.result`
   file, but full of garbage bytes coerced to `?` by the sink's non-ASCII
   rule. No error, no warning (`Wimplicitly_sized_immediate` is disabled by
   default) -- only visible by diffing the actual transcript against what the
   ROM should have printed. Fixed with `ldy.w #0`, and the same defensive
   `.b` suffix was added to every other immediate in both the `asar` and
   `bass` includes (`cmp.b #10`, `adc.b #$26`, `adc.b #'0'`) even though those
   happen to be safe today (executed under 8-bit `A`, and each value already
   fits in a byte) -- relying on a size guess staying right forever is
   exactly what just broke once.

### SNES-3 fix: hex digits are lowercase

VELOCE-RESULT/1 says hex is lowercase, but all three includes' `vt_hex8`
printed `A`-`F`. The smoke transcript never showed it because its values
(`56`, `1234`, `1`, `2`) contain no letters. gilyon cputest was the first
suite to print real 16-bit values. The nibble adjust is now `adc #$26`
(with the carry left set by `cmp #10`, that adds `$27` = `'a'-'9'-1`) instead
of `adc #6` in all three dialects. ca65 was re-verified by running gilyon
cputest/spctest and `veloce_smoke` (rebuilt; new sha256 in
`rom_manifest.json`). The asar and bass versions got the same one-constant
change: asar was not re-run, and bass cannot build anything yet (see below).

## Toolchain defect found: `bass` and `bass-untech` cannot build anything

Both pinned tools that `tools/rom-toolchain/Dockerfile` builds from source
with the same C++17 compatibility workaround (`-include stdexcept -include
cstdint -x c++ -std=c++17 -fno-operator-names`, needed because their vendored
`nall` header library predates several standard-library changes) **segfault
immediately on the architecture-selection command alone**, before any ROM
source, output file or `VT_*` macro is involved:

```
$ bass -c 'x=1'          # pure macro-language, no architecture -> fine, prints/exits 0
$ echo 'arch snes.cpu'   | ... bass ...   # segfault, no output, nothing written
$ echo 'architecture wdc65816' | ... bass-untech ...   # segfault, identical symptom
```

Minimal repro (either tool, run inside the `veloce/rom-toolchain:dev` image):

```sh
printf 'arch snes.cpu\nprint "x"\n' > t.asm && bass t.asm            # bass: segfault
printf 'architecture wdc65816\n' > t.asm && bass-untech t.asm        # bass-untech: segfault
```

This was initially missed: `bass CPUADC.asm` against
`PeterLemon/SNES-CPUTest-CPU/ADC` (unmodified upstream, higan-mirror pin)
also segfaults, but a *pre-existing* shipped `CPUADC.sfc` was still sitting in
that directory from the golden-gate hashing step and its hash happened to
match -- because the crash happens before `bass` ever gets far enough to
overwrite it, not because the tool actually rebuilt it. Deleting the shipped
file first (`rm CPUADC.sfc && bass CPUADC.asm`) reproduces the real behavior:
no output file at all. Neither tool can build *anything* in this image today,
not just VT_*-instrumented sources -- this is unrelated to SNES-2's content
and blocks SNES-7 (bass) and SNES-8 (bass-untech) entirely until
`tools/rom-toolchain/Dockerfile`'s build of these two tools is fixed (most
likely candidate: the compiler-flag workaround above producing broken code in
the architecture-table-loading path -- both forks share that exact same
workaround and fail identically). `asar`, built without any such workaround,
has no equivalent problem.

The integration retains strict nonzero build rejection. Neither assembler's
crash produces a new image, so accepting its exit status cannot prove a build.
The fresh auto-joypad build on 2026-10-11 reproduces the bass-untech crash.
The original ca65 emitter recipe uses the unified `local_source` and
`source_dir` fields and rebuilds successfully. Historical asar and bass
observations above have not been rerun for this integration.

## What this means for later work items

- **SNES-6** (asar, byuu/jonasquinn) is unblocked: `veloce-test.asar.inc` is
  verified working.
- **SNES-3/4/5** (ca65) are unblocked: `veloce-test.ca65.inc` is verified
  working, and `snes/veloce_smoke/` is a live example of the whole
  include+build.py+manifest path.
- **SNES-7** (bass, PeterLemon) and **SNES-8** (bass-untech, undisbeliever)
  are blocked on the toolchain fix above. `veloce-test.bass.inc` is written
  and reasoned through against bass's own architecture table
  (`wdc65816.arch`) but could not be run; re-verify it (the same jsr/db/
  stack-relative approach the asar file used) once bass builds ROMs again.
