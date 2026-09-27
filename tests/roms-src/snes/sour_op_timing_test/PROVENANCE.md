# Provenance: snes/sour_op_timing_test

Upstream: the higan/byuu `snes-test-roms` GitLab mirror
(`https://gitlab.com/higan/snes-test-roms.git`), pinned at
`26e8aa91e0d3b90b901e7e693bc4a8244b391d7b`. Sub-tree
`Sour/SnesTests/op_timing_test/` (`SourMesen/SnesTests` upstream, per that
mirror's own `README.md`).

No LICENSE file. Patch-only per the plan (section 8): only this recipe and
PROVENANCE are committed; no ROM binary ever is.

## Golden gate: behavioural-only (not byte-for-byte)

`golden_gate: false`. The unmodified source assembles and links cleanly with
the pinned cc65 commit (`ca65 -g <file> -o <file>.o` exactly as
`Sour/SnesTests/op_timing_test/build.cmd` specifies) -- no version-drift
workaround was needed here, unlike the NES blargg-shell family
(`tests/roms-src/nes/cpu_dummy_reads/PROVENANCE.md`) or this same mirror's
`jonasquinn-test-roms/snes_adc_sbc` (see "What was tried and abandoned"
below). The rebuilt `op_timing_test_v2.sfc` is the same size as the shipped
one (851968 bytes) but is not byte-identical: `cmp -l` reports the first
divergent byte at file offset 820446. That offset is well past the end of
this suite's own code+data (the linker's own `map.txt`/`.dbg` output was not
cross-checked against it for this pass -- out of scope for a P0 acquisition
item), so no root cause is claimed here, unlike the NES cpu_dummy_reads case
where the specific cc65 feature flag responsible was identified. Given
(a) the source needs no behind-the-times-assembler workaround at all, and
(b) `dma_irq_test` from the very same upstream, built the same way, IS
byte-identical (`tests/roms-src/snes/sour_dma_irq_test/PROVENANCE.md`), the
likeliest explanation is a difference in which exact toolchain snapshot
produced the *shipped* `op_timing_test_v2.sfc` in this particular mirror
commit, not a defect in this recipe's build command -- but this is not
verified.

Verified behaviourally instead (the harness's future result-file verdict,
once SNES-5 patches this shell's report path), the same exception the plan
already grants NES's 2005-era `asm6f` sources and `nes/cpu_dummy_reads`
(plan section 1 decision #9's note; section 5's build.py description).
`build.py`'s own determinism was checked directly: two builds from two
separate clones at the same pinned SHA produced sha256-identical output.

## What was tried and abandoned: the higan-mirror Blargg SNES shell

Before choosing `Sour/`, this recipe's slot was going to be
`jonasquinn-test-roms/snes_adc_sbc` (the plan's "Blargg SNES shell (10)",
SNES-4's target) from the same mirror pin, as the most direct SNES analogue
of `tests/roms-src/nes/cpu_dummy_reads`. It does not assemble at all with
the pinned cc65 commit: `common/macros.inc` does `.define wdm .byte
$42,$EA` -- a workaround for an *old* ca65 that didn't recognize `WDM` as a
real 65816 opcode -- and the pinned cc65 commit *does* recognize it now, so
`.define`ing a symbol with the same name as a real instruction mnemonic
fails with "Cannot use an instruction as macro name", which then cascades
into unrelated-looking parse errors for the rest of the file (`common/
print.s`, `common/testing.s`, `common/shell.s`, all of which `.macpack
longbranch`/`.define jge`/`.define jlt` in the same file). This is a
different cc65-version-drift failure mode than `cpu_dummy_reads`'s (a
range-check on `adc #-1`), but the same category of problem: this suite's
`common/` shell predates a batch of behavioral changes to what ca65 accepts.
Given the time budget for a P0 acquisition item, this was not pursued further
-- flagging it here for whoever picks up SNES-4 (which retargets this exact
shell): budget for a `common/macros.inc` patch (or an `--feature` flag, if
one exists for "don't treat WDM as reserved") on top of the `check_crc_`/
`set_final_result` emitter patch SNES-4 already needs to make. `Sour/` was
substituted as this item's acquisition-and-golden-gate demonstration because
it assembles cleanly and needs no such fix.

## Expected values

Unaffected by this recipe: `op_timing_test_v2` is currently scored via
`screenshot-crc` (on-screen table). Converting it to `CHECK`-per-row output
is SNES-5, not this item.
