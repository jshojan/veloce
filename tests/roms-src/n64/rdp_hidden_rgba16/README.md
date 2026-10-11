# Original N64 RGBA16 and hidden-coverage observations

This original MIT cartridge issues real RDP packets and emits **655,360 cases ×
13 actual 64-bit words = 8,519,680 RAM/register observations**. Four framebuffer
doublewords, four EBUS hidden-metadata doublewords, two guard doublewords and
three DP/MI registers are captured. The EBUS helper executes from uncached SP
DMEM and clears test mode before returning to RDRAM code.

The domain includes 524,288 RGB-seed/alpha/profile/source cases and 131,072
selector/pattern/source cases. Four profiles select forced blending, AA, coverage
wrapping, alpha multiplication/selection and threshold decisions. All 256
blender selector combinations are covered with selected source partners. Both
RDRAM and wrapping XBUS delivery are exercised. Background is fixed RGBA16
`0x5a5a` with zero coverage; geometry is a fixed 4×4 constant-shade triangle.
Joint background/color/alpha/geometry/texture/depth domains remain incomplete.

A separate host quarter-grid, quotient and per-halfword metadata oracle grades
actual results without importing command packing or production code. The full
corpus and 1,616-case control pass; a separately executed capture mutant fails
all control rows. Thirteen field flips and seven stream controls are rejected.
There are no expected output bytes, commercial ROMs, SDK, IPL or Nintendo
microcode in the cartridge.

```sh
python3 tests/roms-src/n64/rdp_hidden_rgba16/build.py
python3 tests/roms-src/n64/rdp_hidden_rgba16/run.py --emulator build/bin/veloce
ctest --test-dir build -R '^n64_rdp_(framebuffer|hidden_checkpoint)' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Artifacts: `build/artifacts/owned/n64_rdp_hidden_rgba16/`. Component tests cover
paired RGBA16/RGBA32 reads/writes, fill metadata and both triangle directions.
The production checkpoint compares subsequent actual guest output and complete
machine state through the netplay ABI. Physical console measurements remain
pending.
