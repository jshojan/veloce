# Original N64 data cache coherence corpus

This MIT licensed Python assembler emits `cache_coherence.z64`, marked `V64HB001`,
with original MIPS code and no SDK/firmware or commercial bytes. It enters KSEG1
and enables cached KSEG0 with CP0 Config.K0=3. The production frontend/plugin
writes observed guest registers to a host file; the guest contains no expected
results or pass verdict.

All **2,097,152/2,097,152 cases** match **12,582,912 guest values**: all 512
D-cache indices × all sixteen byte lanes × all 256 initial bytes. Each case
loads through a cached alias, changes backing memory through its uncached alias,
stores a new byte in cache, then writes back. Six words capture the stale cached
value, changed bus value, cached store, pre-writeback bus value, post-writeback
bus value and TagLo. The independent arithmetic oracle imports no CPU/cache
implementation or assembler field helpers. New values follow a declared schedule;
this does not enumerate all independent old/new byte pairs or the entire hardware.

An executed writeback-to-invalidate mutation fails all 64 reduced cases.
Six field mutations each fail one row, and seven invalid streams produce ERROR.
Identity, domain, width, count, completion and trailer checks reject incomplete
output. Raw observations (~110 MiB) and reports remain in
`build/artifacts/owned/n64_cache_coherence/`; source/image hashes are in the ledger.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_cache_test -j
ctest --test-dir build -R '^n64_cache_(test|coherence)$' --output-on-failure
```

The functional cache model follows the [NEC VR4300 manual](https://hack64.net/docs/VR43XX.pdf),
chapters 5, 11 and CACHE instruction details. Instruction-cache staleness, tag
operations and dirty collisions have separate component evidence; this original
ROM measures data-cache alias coherence only. TLB/extended addressing, cache
errors/parity, pipeline/hazard timing, physical validation, native RAM remapping
and complete device transactions remain pending. Retail boot/playability are
unverified; the local Super Mario 64 probe now stops at missing RSP PC MMIO.
