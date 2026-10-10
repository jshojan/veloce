# Original N64 RI register corpus

This MIT licensed Python assembler emits original MIPS code in `ri_registers.z64`,
marked `V64HB001`. It executes through the frontend and production N64 plugin,
recording actual guest register reads in a host file. No SDK, firmware or
commercial ROM bytes are included. The guest has inputs and output routines,
without expected results or a pass verdict.

All **524,288/524,288 cases** matched **3,145,728 guest values**. Each row records
mode, configuration, selection, refresh, latency and settled error indication.
The domain covers every 19-bit refresh value, every combination of 16 mode
values × 128 configuration values × 64 receive/transmit selections (four times),
and every latency value (correlated with mode). High reserved write bits are supplied
as ones. The independent arithmetic host oracle does not import CPU or builder
field helpers. This does not enumerate the Cartesian product of all registers.

An executed selection-bit mutation fails all 256 reduced cases. Six captured-field
mutations each fail one row, and seven invalid streams produce ERROR. Identity,
domain, width, count, completion and trailer checks reject truncated output.
Raw observations and JSON reports remain in `build/artifacts/owned/n64_ri_registers/`.
The ROM manifest tracks generator and archived image hashes.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_ri_test -j
ctest --test-dir build -R '^n64_(ri_test|ri_registers)$' --output-on-failure
```

Register fields follow the archived [Nintendo RCP header](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm).
RI current-load and write-error are write-only; undefined reads reject. Reading
error indication returns zero because a hardware bus-error producer is absent.
Current-load records the configuration supplied when the write occurs; changing
configuration later does not change that internal latch. Analog current control,
mode-dependent RAM access, refresh scheduling, errors under bus contention,
RDRAM chip registers/discovery and physical timing remain pending. These are
functional register tests, not physical hardware accuracy measurements.

The component gate separately checks all refresh values and mode/config/select
combinations, all latencies, 65,536 write-error inputs, current-load latching,
invalid widths/alignment/directions without mutation, rejected state fields,
exact replay, old-state conversion, reset and loaded-plugin execution.
