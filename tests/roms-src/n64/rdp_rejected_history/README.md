# Original N64 rejected color-history observations

This original MIT cartridge emits **655,360 cases × thirteen actual 64-bit
words = 8,519,680 RAM/register observations**. A 4×4 RGBA16 one-cycle triangle
is followed by eight single-even-byte CI8 fills, one for each retained history
lane. Those fills consume the upper hidden bit from the earlier color history,
including rejected pixels. An SP helper reads the hidden metadata in MI EBUS
mode and clears the mode before returning to RDRAM code.

The words contain four pixel/CI8 pairs, four hidden-metadata pairs, two guards
and three DP/MI registers. Native alpha/coverage profiles, all 256 blender
selector combinations with selected source partners, and RDRAM/wrapping-XBUS
command delivery are covered. Packet length is 256 bytes; END is `0x700200` or
`0x10f0`. Independent host quarter-grid/coverage/history logic grades actual
observations without importing guest command packing or production code.

The full corpus and 1,616-case control pass. The separately executed wrong-
capture ROM fails every control row; all thirteen field flips and seven stream
controls reject. Background coverage/geometry/shade are fixed. Texture/depth,
all framebuffer formats and full joint state domains remain incomplete.
No expected output bytes, SDK, IPL, commercial data or Nintendo microcode are
included.

```sh
python3 tests/roms-src/n64/rdp_rejected_history/build.py
python3 tests/roms-src/n64/rdp_rejected_history/run.py --emulator build/bin/veloce
ctest --test-dir build -R '^n64_rdp_rejected_checkpoint_test$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Artifacts: `build/artifacts/owned/n64_rdp_rejected_history/`. Production replay
compares subsequent actual guest output and whole state through the netplay ABI.
Physical hardware validation remains pending.
