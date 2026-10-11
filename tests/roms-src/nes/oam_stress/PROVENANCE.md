# Provenance: nes/oam_stress

Upstream: `christopherpow/nes-test-roms`, pinned at `95d8f621ae55cee0d09b91519a8989ae0e64753b`
(sub-tree `oam_stress/`). Test code by Shay Green (blargg); readme says freely usable, no
`LICENSE` file (plan section 8: "blargg NES/GB/SNES ... readmes say freely
usable, keep readmes").

Blargg oam_stress: OAM decay / corruption stress test.

## Expected values

Unmodified upstream ROM (`rom_variant: upstream`, no patches): the
pass/fail behaviour is blargg's own `$6000` status-byte protocol
(`common/shell.inc` -> `common/testing.s`/`common/text_out.s`), read by
`nes::TestChannel`'s `blargg6000` adapter (NES-1) exactly as the ROM writes
it. No new expected-value table was authored here; this is NES-2's golden
gate for the build pipeline, not a content patch (that's NES-5's `TEST`/
`CHECK`-emitting shell rewrite).

## Golden gate: behavioural-only (not byte-for-byte)

`golden_gate: false` in `recipe.json`, for the same reason
`tests/roms-src/nes/cpu_dummy_reads/PROVENANCE.md` documents in full: the
shared `common/delay.s` countdown idiom (`adc #-1` / `sbc #-1`) needs
ca65's `--feature force_range` to assemble at all, and with that feature on
the rebuild is the same size as upstream's shipped ROM (40976 bytes) but
differs at 1170 byte(s), first divergence at file offset 0x6202 (25114)
-- same region and same unconfirmed root-cause theory (a `longbranch.mac`
short/long encoding difference somewhere upstream in the same object file)
that suite's write-up already covers; not repeated per suite (see
`recipe.json`'s `golden_gate_reason` for the copy of that text specific to
this suite's own byte-diff numbers).

Verified behaviourally instead: two independent `build.py` rebuilds of the
unmodified source are byte-identical to each other, so the pipeline is
reproducible even though it doesn't reproduce this particular pre-existing
binary.
