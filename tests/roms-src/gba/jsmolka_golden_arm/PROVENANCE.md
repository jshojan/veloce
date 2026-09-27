# Provenance: gba/jsmolka_golden_arm

Upstream: [`jsmolka/gba-tests`](https://github.com/jsmolka/gba-tests), pinned
at `a7113b67e63f83a9b321696ddd7042ccfad6c881` (MIT, copyright 2019 Julian
Smolka). `cores/gba/tests/gba-test-roms` is the same repository at the same
commit (checked out separately by the test harness's own repository list,
`cores/gba/tests/test_config.json`'s `repositories.jsmolka`); this recipe
clones it again independently under `upstream/` because `build.py`'s golden
gate needs the *original* clone (to hash the shipped binary before any
build command touches it) and must not depend on files a different tool
already mutated.

## What this proves (GBA-2 scope)

GBA-2 is "golden gate (fasmarm byte-identity for jsmolka/alyosha, devkitARM
for nba)" -- proving `tools/rom-toolchain`'s pinned FASMARM 1.44 reproduces
these suites' existing shipped binaries byte-for-byte, which is the
precondition section 5's `build.py` states for ever patching them ("a suite
may only be patched after its gate passes"). This recipe golden-gates one
representative ROM (`arm/arm.gba`, jsmolka's own canonical ARM
instruction-set test) rather than the full jsmolka+alyosha collection
(114 ROMs across both forks): rebuilding every one of them, and actually
patching them to speak VELOCE-RESULT/1, is GBA-3 (nba)/GBA-5a-5b
(jsmolka/alyosha) per `docs/testing/testrom-plan.md` section 6.4 -- separate,
larger work items that depend on this gate passing, not on this one ROM.

Verified directly (not just via `build.py --check`):

```
$ sha256sum arm/arm.gba                                   # upstream's shipped binary
77ee88662552bdc885c1080c0172ff119d54db791bd73b21808cf1ff1fe5b40e
$ $FASMARM arm.asm arm.gba                                  # rebuilt inside veloce/rom-toolchain:dev
$ sha256sum arm/arm.gba                                    # identical
77ee88662552bdc885c1080c0172ff119d54db791bd73b21808cf1ff1fe5b40e
```

`build.py gba/jsmolka_golden_arm` reproduces the same result (`golden_gate:
{"match": true}` in `rom_manifest.json`).

## Recommendation for whoever picks up GBA-5a/5b

alyosha-tas/gba-tests (the fork actually vendored for the current 114-ROM
config, `cores/gba/tests/alyosha-gba-tests`) shares the identical
`lib/macros.inc`/`lib/text.asm`/build invocation as jsmolka's original, so
this same one-ROM proof extends to it without a separate golden-gate
mechanism -- but GBA-5a/5b should still golden-gate every individual ROM it
rebuilds before patching it (some of alyosha's own additions, e.g. the LDM/
Interactions/timing/prefetcher ROMs, are FASMARM sources this recipe never
touched and could plausibly hit toolchain quirks arm/arm.gba didn't).
