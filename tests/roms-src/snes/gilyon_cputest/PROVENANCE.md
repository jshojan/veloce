# Provenance: snes/gilyon_cputest

Upstream: `github.com/gilyon/snes-tests`, pinned at tag `v1.4`
(`5ecdf555da920f0bd7b157542141965a8120186d`, that tag's exact commit --
`git ls-remote --tags` confirms `v1.4` -> this SHA). MIT licence
(`LICENSE`, Copyright (c) 2023 gilyon). The patch in `patches/` is Veloce's
and is offered under the same terms. Only the patch, `recipe.json` and this
file are committed. No ROM binary is committed; `build.py` clones, patches and
builds.

Sub-directory `cputest/` builds the S-CPU side of gilyon's 65816 opcode test,
"full" variant: 1610 generated tests covering every opcode except STP/WAI in
every addressing mode, flag results, and wrapping behaviour with and without
emulation mode. It does not test cycle timing, interrupts or I/O registers.
`cputest-basic.sfc` (the `--basic` subset) is not built.

## Golden gate: behavioural-only (nothing to compare against)

`golden_gate: false`. `cputest/.gitignore` excludes every generated file this
build produces (`tests*.inc tests*.txt tests*.map`), and the repo ships no
prebuilt `.sfc`. `build.py` still builds the unpatched source first (it does
that for every recipe before applying patches), so a toolchain break would
show up there. The unpatched ROM boots on Veloce and stops on screen at test
`0274`, waiting for a key press. That is the first of the ten failures the
patched build reports below.

## The patch (`patches/0001-veloce-result-channel-cputest.patch`)

SNES-3 in the test-ROM plan. Two files change:

- **`make_cpu_tests.py`** records, for each generated test, the same expected
  values the generated compare code uses and writes them to
  `tests-full-veloce.inc`: `VT_TOTAL_TESTS`, a 24-bit pointer table
  `vt_exp_ptrs`, and one record per test. A record holds flags; expected X, Y,
  P (with E in bit 8) and A; S/DBR/D when the test checks them; each memory
  check's 24-bit address and byte; and the test name. The records go in LoROM
  banks 5-7, which the upstream ROM leaves empty. The generator never lets a
  record straddle a bank. The test code itself (`tests-full.inc`) is generated
  unchanged.
- **`main.asm`**:
  - includes `tests/roms-src/include/snes/veloce-test.ca65.inc` and emits
    `VELOCE 1 snes gilyon_cputest` plus an `INFO tests=1610 ...` line at start;
  - `init_test` emits `CHECK <n> PASS <name>` for the test that just finished.
    The ids are the 4-digit hex test numbers used in `tests-full.txt`;
  - `fail:` no longer draws registers and waits for a key. It looks up the
    failing test's record and compares fields in the same order the generated
    code does (X, Y, E:P, A, S, DBR, D, memory). It emits
    `CHECK <n> FAIL <name> field=<x|y|e:p|a|s|dbr|d|mem> exp=.. got=..` (plus
    `at=<address>` for a memory check) and `REG a= x= y= p= e= s= d= dbr=`.
    `field` is a key=value token rather than part of the name, so a check has
    the same name when it passes and when it fails, and the baseline diff can
    match the two. It then continues with
    the next test through upstream's own jump-table path. If every tabulated
    value matches, the test failed one of its hand-written checks (a branch or
    jump that took the wrong path), and the line says `field=additional-check`
    with no exp/got. Reporting runs on a stack at `$0FFF`, so it never
    overwrites the page-1/page-2 locations the tests check;
  - the BRK/COP handlers at `$1000-$100F` are `jml vt_brk_cop` instead of
    `STP`. An unexpected software interrupt becomes
    `CHECK <n> FAIL <name> field=unexpected-brk-cop` and the run goes on. There is
    no REG line, because the test never reached `save_results`. The BRK/COP
    tests install their own handlers first, exactly as upstream;
  - "Invalid test order" (an errant jump into another test) emits
    `CHECK <n> FAIL invalid test order (errant jump) exp=<expected> got=<n>`
    and `END`, then halts as upstream does;
  - `success` emits the last test's CHECK line and `END <pass>/<total>`. It
    shows "Failed" instead of "Success" on screen when any check failed.

Reporter state lives in WRAM `$0E00-$0E0D` plus two long pointers at direct
page `$22-$27`. No test reads or writes any of these addresses (checked
against every address in `tests-full.txt` and the generated stores).

## Expected values

Every `exp=` value comes from `make_cpu_tests.py`. The patch adds a writer
next to the existing `tests-full.txt` writer, fed from the same variables.
The compare code in the ROM is unchanged, so PASS/FAIL per test is gilyon's
own judgement. The table only explains which field failed and by how much.
gilyon's README documents the two undocumented behaviours the suite checks:
PLB reading `$200` with S=`$1FF` in emulation mode, and the `(dp,X)`
high-byte wrap when D's low byte is nonzero. For decimal mode it points to
appendix A of 6502.org's decimal-mode document.

## Verification (2026-09-27)

- Built with `python3 tests/roms-src/build.py snes/gilyon_cputest`;
  `--check` rebuilds it byte-identically. The sha256 and tool versions (ca65
  V2.18 Git 5552824 from the pinned `tools/rom-toolchain` image) are in
  `tests/roms-src/rom_manifest.json` under
  `snes/gilyon_cputest/cputest-full.sfc`.
- Two runs under `VELOCE_TEST_OUT` gave byte-identical result files,
  trailer included (168 frames).
- Result on Veloce: `END 1600/1610`. The ten failures are:

  | Test | Instruction | Reported |
  |---|---|---|
  | 0274 | `jsl $FE8000` (E=1, S=$0100) | `field=mem exp=03 got=00 at=0000fe` |
  | 0277 | `jsr ($FFFF,x)` (E=1) | `field=mem exp=02 got=00 at=0000ff` |
  | 03c2 | `pea $9876` (E=1) | `field=mem exp=76 got=51 at=0000ff` |
  | 03c4 | `pei ($ff)` (E=1) | `field=mem exp=54 got=51 at=0000ff` |
  | 03c7 | `per -$8000` (E=1) | `field=mem exp=03 got=51 at=0000ff` |
  | 03d9 | `plb` (E=1, S=$01FF) | `field=e:p exp=0130 got=01b0` |
  | 03dc | `pld` (E=1, S=$01FF) | `field=d exp=1356 got=128b` |
  | 03ed | `phd` (E=1) | `field=mem exp=57 got=7d at=0000ff` |
  | 0446 | `rtl` (E=1, S=$01FF) | `field=additional-check` |
  | 061a | `sbc #$f1e2` (16-bit, decimal) | `field=a exp=59db got=58db` |

  Nine are one core behaviour. On real hardware, the 65816's native-only
  stack instructions (JSL, JSR (a,x), PEA, PEI, PER, PHD, PLD, PLB, RTL) do
  not wrap S to page 1 in emulation mode; Veloce wraps them. The tenth is a
  16-bit decimal-mode SBC result that is off by $100. These are core bugs,
  not patch artefacts: the unpatched ROM also stops at the first one (0274).
- The BRK/COP and invalid-order paths were exercised with a throwaway
  variant (not committed) that replaced test 1's instruction with BRK and
  gave test 5 the wrong number. It produced
  `CHECK 0001 FAIL adc #$edcb field=unexpected-brk-cop`, then PASS for tests 2-3,
  then `CHECK 0007 FAIL invalid test order (errant jump) exp=0005 got=0007`
  and `END 3/5`.
