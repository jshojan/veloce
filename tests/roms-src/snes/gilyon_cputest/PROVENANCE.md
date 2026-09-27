# Provenance: snes/gilyon_cputest

Upstream: `github.com/gilyon/snes-tests`, pinned at tag `v1.4`
(`5ecdf555da920f0bd7b157542141965a8120186d`, that tag's exact commit --
`git ls-remote --tags` confirms `v1.4` -> this SHA). MIT licence
(`LICENSE`, Copyright (c) 2023 gilyon).

Sub-directory `cputest/` builds the S-CPU side of gilyon's 65816 opcode test
(the "full" variant, `cputest-full.sfc`; `cputest-basic.sfc` also exists but
was not built here). `spctest/` (S-CPU + S-SMP, needs both `cc65` and
`spcasm`) was not attempted in this pass -- acquisition/build for it is left
for whoever picks up SNES-3, which covers both ROMs.

## Golden gate: behavioural-only (nothing to compare against)

`golden_gate: false`. `cputest/.gitignore` excludes every generated file this
build produces (`tests*.inc tests*.txt tests*.map`) and the repo ships no
prebuilt `.sfc` at all -- `make_cpu_tests.py` (run as this recipe's first
build step) is what generates `tests-full.inc`/`tests-full.txt` from a table
in that same script, and `main.asm` (`.include`s the generated `.inc`) is
then assembled and linked. There is no upstream binary to byte-compare, so
this is verified behaviourally: the build completes (`ca65 main.asm -o
cputest-full.o && ld65 -C lorom.cfg ...`) and produces a `cputest-full.sfc`
that boots under the emulator (manually confirmed: `HEADLESS=1 FRAMES=300
./build/bin/veloce build/roms/snes/gilyon_cputest/cputest-full.sfc` runs
without error).

## Expected values

None authored by this recipe. `make_cpu_tests.py`'s own generated table
(`tests-full.txt`) is gilyon's documented set of expected 65816 results;
SNES-3 is the item that patches `main.asm`'s `fail:` path to turn a
mismatch into a VELOCE-RESULT/1 `CHECK ... FAIL exp=.. got=..` (via
`veloce-test.ca65.inc`, already verified working -- see
`tests/roms-src/include/PROVENANCE.md`) instead of only drawing it on
screen, at which point `tests-full.txt` becomes the source for that
`REG a x y p s d dbr` / `exp=` table.

## Tool versions

Recorded in `tests/roms-src/rom_manifest.json` under
`snes/gilyon_cputest/cputest-full.sfc`.
