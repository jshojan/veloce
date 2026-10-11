# Original N64 ordered-dither observations

Original MIT cartridge: **1,179,648 cases × 13 actual 64-bit words =
15,335,424 RAM/register observations**. All 65,536 coupled RGB/alpha input
patterns visit nine ordered RGB/alpha selector combinations and two command
sources. RDRAM uses ordinary alpha; wrapping XBUS additionally enables both
alpha/coverage flags. Shader alpha and blend selectors expose the ordered alpha
seed. Reserved/inactive mode fields are set without altering the result.

An RGBA16 triangle uses fixed quarter-grid geometry and background. Guest
instructions read four pixel words, four EBUS hidden words, two guards and three
DP/MI registers. No expected-result bytes are in the ROM. Independent host
quarter-grid, dither, blend, alpha and packing arithmetic grades actual output.
Packets are 160 bytes; END is `0x7001a0` or wrapping `0x1090`.

The full corpus and 252-case control pass. The separately executed wrong-capture
ROM fails all control rows; 13 field flips and seven stream controls reject.
RGB/alpha partners, geometry, coverage, gradient state and format remain selected;
RGBA32 and field-mode seeds are separately checked by component tests. Random
selectors/thresholds, noise state, complete joint graphics domains and physical
hardware verification remain unfinished.

```sh
python3 tests/roms-src/n64/rdp_ordered_dither/build.py
python3 tests/roms-src/n64/rdp_ordered_dither/run.py --emulator build/bin/veloce
ctest --test-dir build -R '^n64_rdp_dither_checkpoint_test$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Artifacts: `build/artifacts/owned/n64_rdp_ordered_dither/`. Production S029
checkpoint replay compares actual subsequent guest output and whole-state bytes.
