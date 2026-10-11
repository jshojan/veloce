# Original N64 primitive-depth observations

Original MIT cartridge: **1,048,576 cases × 21 actual 64-bit words =
22,020,096 RAM/register observations**. Each 16-bit SetPrimDepth field is swept
through all bit patterns, with four selected partner values, four Z modes and
RDRAM/wrapping-XBUS delivery. The top depth bit is ignored; primitive Z overrides
conflicting triangle coefficients and delta remains a raw command field.

Guest CPU writes initialize depth RAM and its paired metadata. An SP helper
reads MI EBUS hidden bits, and guest instructions emit color, color hidden,
depth, depth hidden, guards and DP/MI registers on two lines per case. No expected
bytes are embedded. Independent host compression, comparator and quarter-grid
arithmetic grades actual results. Packets are 192 bytes; END is `0x7001c0` or
wrapping `0x10b0`.

The full corpus and 160-case control pass. A separately executed wrong-capture
ROM fails every control row; all 21 field flips and seven stream controls reject.
All field operands are visited, but command field partners, geometry, gradients,
coverage, framebuffer format (RGBA16) and joint graphics state remain selected.
Component tests separately cover RGBA32 and the no-comparison shifter path.
Physical-console evidence and full retail graphics remain pending.

```sh
python3 tests/roms-src/n64/rdp_primitive_depth/build.py
python3 tests/roms-src/n64/rdp_primitive_depth/run.py --emulator build/bin/veloce
ctest --test-dir build -R '^n64_rdp_primitive_checkpoint_test$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Artifacts: `build/artifacts/owned/n64_rdp_primitive_depth/`. S028 production
checkpoint replay checks actual subsequent output and complete state; S027
imports preserve their prefix and initialize primitive depth to zero.
