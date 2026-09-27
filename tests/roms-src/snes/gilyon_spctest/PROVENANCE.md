# Provenance: snes/gilyon_spctest

Upstream: `github.com/gilyon/snes-tests`, pinned at tag `v1.4`
(`5ecdf555da920f0bd7b157542141965a8120186d`, the same pin as
`snes/gilyon_cputest`). MIT licence (`LICENSE`, Copyright (c) 2023 gilyon).
The patch in `patches/` is Veloce's and is offered under the same terms. Only
the patch, `recipe.json` and this file are committed. No ROM binary is
committed; `build.py` clones, patches and builds.

Sub-directory `spctest/` builds gilyon's SPC700 opcode test: 1368 generated
tests (every opcode except SLEEP and STOP, all addressing modes, with and
without the P flag). It does not test cycle timing, dummy reads, the S-SMP's
memory-mapped registers (timers) or the DSP. The generator splits the tests
into three S-SMP programs (500 per program) that the S-CPU uploads through the
IPL protocol one after another.

## Golden gate: behavioural-only

`golden_gate: false`. `spctest/.gitignore` excludes `spc_tests*.asm`, `*.spc`
and `tests.txt`, and v1.4 ships no `.sfc`. The `gilyon/spctest.sfc` in the
higan/snes-test-roms mirror is an older build: 1320 tests, from before v1.4's
"Add missing 'or' tests for SPC-700" commit. It is a different source and not
a byte reference. `build.py` builds the unpatched source first. That unpatched
v1.4 ROM passes all 1368 tests on Veloce, as the SNES-1 `spcport` adapter
reports (last test `0557`).

## The patch (`patches/0001-veloce-result-channel-spctest.patch`)

SNES-3 in the test-ROM plan. **Only the S-CPU side changes. The S-SMP test
programs are assembled from gilyon's generated source unmodified**, so the
tests and their compare order are exactly upstream's.

- **`make_tests.py`** also writes `spc_expected.inc` for the S-CPU:
  `VT_TOTAL_TESTS`, a 16-bit pointer table `vt_exp_ptrs`, and one record per
  test. A record holds flags (SP checked, memory checked, extra hand-written
  code), the expected P, A, X and Y, and the test name. It goes in LoROM bank 3,
  which the 128 KiB upstream ROM leaves empty.
- **`spctest.asm`** turns gilyon's APU-port protocol into VELOCE-RESULT/1 on
  the `$21FE` port:
  - `VELOCE 1 snes gilyon_spctest` and `INFO tests=1368 ...` at start;
  - when a program reports success (port0 = 1) at last test L, every test from
    the first unreported one up to L gets `CHECK <n> PASS <name>`. This is
    exact: the S-SMP's `init_test` enforces sequential test numbers and jumps
    to `fail` on the first mismatch, so a success report means every test in
    the program ran and passed;
  - when a program reports failure (port0 = 2) at test N, the tests before N
    get PASS. After gilyon's handshake returns A/X/Y, the S-CPU emits
    `CHECK <N> FAIL <name> field=<p|a|x|y> exp=.. got=..` for the first of P/A/X/Y
    (the S-SMP's own compare order) that differs from the table, then
    `REG a= x= y= p=` and `END <pass>/<pass+1>`. The S-SMP reports nothing
    else back, so a failure on an SP, memory or hand-written check says
    `field=unreported` instead of giving exp/got. (`field` is a key=value token,
    so the name is the same as on the PASS line.)
  - a watchdog in `wait_result`: if ports 2/3 (the running test number) do not
    change for 120 frames before the S-SMP reports, the test is reported as
    `CHECK <n> FAIL <name> field=s-smp-hung` and `END` closes the
    transcript. A hung S-SMP therefore gives a verdict instead of an unscored
    run that never ends. The `$4210` vblank flag used for counting is set every
    frame whether or not NMI is enabled;
  - the reporter lives in bank 2 and is called with `JSL` wrappers. Bank 0 is
    nearly full with the first S-SMP program, and the inline strings would
    overflow it by about 2.4 KiB. The emitter include is placed there too.

Upstream stops at the first failing test and the patch keeps that
behaviour (the plan scopes spctest to the S-CPU side). The config sets
`expected_checks: 1368`, so an early stop at test N scores FAIL with
progress N/1368 rather than a pass.

## Expected values

Every `exp=` value comes from `make_tests.py`: the same `ep`/`ea`/`ex`/`ey`
variables that generate the S-SMP compare instructions (`cmp a, #ep`,
`cmp result_a, #ea`, `cmp x, #ex`, `cmp y, #ey`). The S-SMP decides PASS or
FAIL; the S-CPU table only explains a failure.

## Verification (2026-09-27)

- Built with `python3 tests/roms-src/build.py snes/gilyon_spctest`;
  `--check` rebuilds it byte-identically. The sha256 and tool versions (ca65
  V2.18 Git 5552824, spcasm 1.2.0, from the pinned `tools/rom-toolchain`
  image) are in `tests/roms-src/rom_manifest.json` under
  `snes/gilyon_spctest/spctest.sfc`.
- Result on Veloce: 1368 `CHECK ... PASS` lines and `END 1368/1368` in 137
  frames. The SNES-1 `spcport` adapter stays silent because the ROM writes
  the port first.
- Every failure path was exercised with throwaway variants (not committed).
  Each variant edited the generated S-SMP source and/or `spc_expected.inc`:
  - a P mismatch at test 3 gave `CHECK 0003 FAIL adc a, [$01]+y field=p exp=80 got=00`,
    then `REG` and `END 3/4`;
  - a failure at test 0 gave `CHECK 0000 FAIL ...` and `END 0/1` with no PASS
    lines;
  - an extra memory compare gave `field=unreported`;
  - an infinite loop in test `01f5` gave 501 PASS lines,
    `CHECK 01f5 FAIL clr1 $02.1 field=s-smp-hung` and `END 501/502`
    after the 120-frame watchdog.

  The hang variant used a branch-to-self loop, not `STOP`. Veloce's S-SMP
  treats `SLEEP`/`STOP` (`$EF`/`$FF`) as 2-cycle no-ops instead of halting
  (`cores/snes/src/spc700.cpp`, the two cases just before `TSET1`). That is a
  core inaccuracy spctest does not cover (it excludes both opcodes).
