# Original N64 hidden RDRAM observations

This original MIT cartridge emits **65,536 cases × nine actual 64-bit RAM/register
words = 589,824 observations**. Every 16-bit seed supplies four different
halfword patterns. Guest SB, SH, SW and SD run against a fixed RDRAM region,
first with ordinary metadata and then with MI EBUS test mode enabled. The helper
executes from uncached SP DMEM while EBUS is enabled and clears the mode before
returning to RDRAM code.

The words contain two ordinary RAM doublewords, two gathered ordinary hidden
metadata doublewords, two gathered EBUS metadata doublewords, two ordinary RAM
doublewords written in EBUS mode, and the final MI mode register. A separate
host per-byte model grades the result file without importing guest packing or
production code. There are no expected output bytes, SDK, IPL, commercial data
or Nintendo microcode in the cartridge.

The full corpus and 256-case control pass. The separately executed wrong-capture
ROM fails all 256 control rows; all nine field flips and seven stream controls
are rejected. This covers four store widths at selected addresses and correlated
operand partners. All addresses, masked merge instructions, DMA devices, cache
bus freezes, RDP rendering, depth behavior and full joint state domains are not
exhausted by this cartridge. Separate machine tests cover selected DMA/cache
transactions and every enabled-byte mask against sixteen patterns in ordinary
and EBUS modes.

```sh
python3 tests/roms-src/n64/hidden_rdram/build.py
python3 tests/roms-src/n64/hidden_rdram/run.py --emulator build/bin/veloce
python3 tests/roms-src/build.py --original-only --check
ctest --test-dir build -R '^n64_hidden_(machine_test|checkpoint_test)$' --output-on-failure
```

Artifacts: `build/artifacts/owned/n64_hidden_rdram/`. The production checkpoint
gate compares subsequent actual guest bytes and complete serialized machine
state through the netplay ABI. Physical console validation remains pending.
