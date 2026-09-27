# Provenance: nes/cpu_dummy_reads

Upstream: `christopherpow/nes-test-roms`, pinned at `95d8f621ae55cee0d09b91519a8989ae0e64753b`
(sub-tree `cpu_dummy_reads/`). Test code by Shay Green (blargg) and Joel
Yliluoma; readme says freely usable, no LICENSE file (see section 8 of the
plan: "blargg NES/GB/SNES ... readmes say freely usable, keep readmes").

## Expected values

The expected pass/fail behaviour is blargg's own test framework result codes
(`common/shell.inc` -> `common/testing.s`), unchanged by this suite's build.
No new expected-value table was authored; this suite exercises `build.py`'s
mechanics, not new test content.

## Golden gate: behavioural-only (not byte-for-byte)

`golden_gate: false` in `recipe.json`. What was tried, in order:

1. **Bare `ca65 -I common -o ... cpu_dummy_reads.s` (the suite's own
   `readme.txt` command)**: fails. `common/macros.inc` does
   `.include "longbranch.mac"`, cc65's own standard macro package (ships
   under cc65's `asminc/`, not this ROM's `common/`) — it was never on any
   include path because the toolchain image only copied cc65's `bin/`.
   Fixed generically at the image level: `tools/rom-toolchain/Dockerfile` now
   also copies `cc65/asminc/` and `cc65/cfg/` into the image (`cc65-asminc`,
   `cc65-cfg`); the recipe passes `-I /opt/rom-toolchain/cc65-asminc`.

2. **With that include path**: fails differently — `Error: Range error
   (-1 not in [0..255])` on every `adc #-1` / `sbc #-1` in `common/delay.s`
   (a delay-loop countdown idiom, used by nearly every blargg-framework NES
   test that includes `shell.inc`). Verified this is **not** specific to our
   pinned cc65 commit: the *current* cc65 HEAD, and Ubuntu's packaged
   `cc65 2.19-1`, reject the exact same minimal repro
   (`adc #-1` / `lda #-1` / `sbc #-1`) with the identical message — `ca65`'s
   `IsByteRange()` (`src/ca65/expr.c`) has always required the raw constant
   itself to be in `[0,255]`, so a bare negative immediate byte has never
   been accepted by any ca65 in cc65's git history. Some sibling suites in
   the same upstream repo route around this with an explicit mask
   (`adc #-1&$FF`, e.g. `cpu_dummy_writes`, `cpu_exec_space`); this one
   doesn't.
   **Fix, no source change**: ca65's own `--feature force_range` flag
   (`src/ca65/feature.c`) truncates instead of erroring. Added to the
   recipe's `ca65` command.

3. **With `--feature force_range`**: assembles and links cleanly (same
   40976-byte output size as the shipped ROM), but
   `cmp -l` against the shipped `cpu_dummy_reads.nes` shows **184 differing
   bytes**, first divergence at file offset `0x6027` (inside the second
   16K PRG bank). No length drift anywhere in the file, so this isn't a
   branch macro flipping between its short and long encodings (that would
   shift every following byte) — the leading theory is that `--feature
   force_range` changing the *evaluated* operand for one `adc #-1` shifted
   which of `common/shell.inc`'s `jeq`/`jne`/`jpl`/`jmi` `longbranch.mac`
   macros resolve as a short conditional branch vs. the fixed-length
   short-branch+`jmp` form somewhere upstream in the same object file,
   which is speculative and unconfirmed — determining the exact byte
   was out of scope for this pass.

Given (2)+(3), this suite is verified **behaviourally** (the harness's
result-file verdict), the same exception the plan already grants NES's
2005-era `asm6f` sources (plan section 1, decision #9's note; section 5's
`build.py` description: "a suite may only be patched after its gate passes,
exceptions recorded per suite ... e.g. the 2005 NES sources cannot be
byte-reproduced and are verified behaviourally"). `build.py`'s determinism
was checked directly instead: two independent builds of the *unmodified*
source, from two separate clones, are byte-identical to each other
(sha256 equal) — the pipeline is reproducible even though it doesn't
reproduce *this particular* pre-existing binary.

## Recommendation for whoever picks up NES-5 (cc65 group, P1)

Every "NES blargg modern (98)" suite this plan assigns to `cc65` likely
shares `common/delay.s` and hits (2). `--feature force_range` on the
`ca65` invocation (plus the `cc65-asminc`/`cc65-cfg` image paths above) is
the fix for the *build* half; each suite's *golden gate* still needs the
same case-by-case behavioural-vs-byte-exact call this file made, suite by
suite — recommend budgeting for that rather than assuming cc65 golden-gates
cleanly by default.
