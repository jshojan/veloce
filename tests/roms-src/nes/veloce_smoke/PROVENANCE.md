# Provenance: nes/veloce_smoke

Not an upstream test ROM. `local_source: true` in `recipe.json`: the source
under `src/` is Veloce-authored (this repository's own licence applies), and
`golden_gate` is `false` because there is no upstream shipped binary to diff
against -- there never can be one for a ROM that only exists to exercise our
own `tests/roms-src/include/veloce-test.inc`.

## What it proves

1. Every `VT_*` macro `veloce-test.inc` (ca65) exports is used at least once
   (`VT_BEGIN`, `VT_TEST`, `VT_PASS`, `VT_FAIL`, `VT_REG`, `VT_LOG`, `VT_END`),
   so a change that breaks the emitter breaks this ROM's known transcript
   instead of surfacing as a mysterious failure in a real suite later.
2. Both addressing-mode conventions the header comment documents: `VT_TEST
   #2, "fail_line"` / `VT_FAIL #2, "fail_line", #$2a, smoke_got` use an
   immediate (compile-time constant); `VT_TEST smoke_id, "putc_and_hex"` /
   `VT_PASS smoke_id, "putc_and_hex"` use a zero-page operand round-tripped
   through RAM first, standing in for a real suite's runtime loop counter
   (AccuracyCoin, NES-4) or observed register value (a CRC compare, NES-5).
3. Determinism (docs/testing/VELOCE-RESULT.md, plan section 3's "shared
   selftest runs it twice and byte-diffs the two files"): running the built
   ROM twice under `VELOCE_TEST_OUT` produces byte-identical result files.
   Verified directly for this report:
   - Two independent `build.py` builds of `veloce_smoke.nes` are
     byte-identical (`sha256 6ed7935827e9c9a6e7fec884a82efa397b06991d83904840f16493f1adae9bdc`
     both times).
   - Two headless runs of that ROM under `VELOCE_TEST_OUT` produce
     byte-identical `.result` files (`diff` clean).

## Expected transcript (fixed at build time, not measured)

Every value in the transcript below is a literal in `src/veloce_smoke.s` --
nothing is "the emulator's observed behaviour" here, since the point of this
ROM is to test the *channel*, not the CPU/PPU/APU. `CHECK 2 FAIL` is
deliberate (`VT_FAIL #2, "fail_line", #$2a, smoke_got` with `smoke_got` set
to `$15` two lines earlier), to prove `VT_FAIL`'s `exp=`/`got=` hex
formatting works, not to signal a bug.

```
VELOCE 1 nes veloce_smoke
TEST 1 putc_and_hex
REG x=7f
CHECK 1 PASS putc_and_hex
TEST 2 fail_line
CHECK 2 FAIL fail_line exp=2a got=15
LOG smoke ok
END 1/2
```

Measured end-to-end against the built ROM (`build/bin/veloce`, headless,
`FRAMES=60`): the app-owned trailer is `#VELOCE end reason=terminator
frames=1 cycles=27204 status=1` (status 1 because `END 1/2` is not a full
pass; `frames=1` because the ROM finishes and calls `rts` back into
`reset`'s infinite loop within the first frame, and the sink's `END` line
ends the run right there per `VELOCE_TEST_EXIT=1`'s default).

## The asm6f / NESASM3 dialects: real bugs, not just "assembled cleanly"

Only the ca65 `veloce_smoke.nes` is built here (NES-2's scope: "veloce_smoke.nes
built via build.py"); wiring a second and third `veloce_smoke` per dialect
is left for whoever picks up NES-6 (asm6f group) / NES-4 (NESASM3,
AccuracyCoin), since those are the first real consumers and can validate the
include against their own real ROM. But `veloce-test-asm6f.inc` and
`veloce-test-nesasm.inc` were not just hand-assembled -- the exact
instruction sequence from `veloce-test.inc` was hand-translated into each
dialect, assembled against the pinned toolchain, wrapped in a throwaway
iNES header, and run through `build/bin/veloce` headless until it produced
this same transcript byte-for-byte. That process found two real, otherwise
silent bugs, now documented in each file's own header comment so nobody
re-loses the time finding them again:

- **asm6f** (`freem/asm6f` `fd03633e`): a forward conditional branch
  (`BCC`/`BNE`/`BEQ` to a not-yet-defined label -- `vt_nibble`, `vt_dec8` and
  `vt_puts` all have one) inside an `IFNDEF`/`ENDIF` block, combined with
  *any* `MACRO` used anywhere later in the same file, makes pass 2 report a
  spurious `Illegal instruction` at the macro's call site. Repro'd down to a
  two-instruction minimal case; removing either the enclosing `IFNDEF` or
  the forward branch makes it go away. Worked around by dropping the
  whole-file double-include guard (the small `IFNDEF VT_PTR` check has no
  branch in it, so it's unaffected).
- **nesasm** (`ClusterM/nesasm` `2b39ec6`): `LDA (VT_PTR),y` *assembles with
  no error* but silently encodes as something other than zero-page indirect
  indexed -- nesasm's indirect-addressing syntax is `[VT_PTR],y`, not
  `(VT_PTR),y`. Without a real ROM run, this would have shipped a
  `veloce-test-nesasm.inc` that assembles clean and crashes (an infinite
  spew of `?` bytes over the port from executing string data as code) the
  moment any suite calls `vt_str`/`vt_line`. Also needed: nesasm's
  `.INCLUDE` directive needs the leading dot (the bare `INCLUDE` this pinned
  build's own directive-list documentation shows is an "Unknown
  instruction") and a global label already in scope before it.

Both fixes are one line each; see the two files' own header comments for
the worked example. Neither dialect has its own `veloce_smoke.nes` in this
repo, so there is no committed golden hash for either -- only the manual
verification recorded here (byte-identical transcript against the ca65
build, from a hand-built throwaway ROM, not committed).
