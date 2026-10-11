# Original N64 one-cycle triangle depth observations

This original MIT cartridge emits **524,288 cases × 21 actual 64-bit words =
11,010,048 RAM/register observations**. The corpus covers every stored 16-bit
depth word, four comparator modes and RDRAM/wrapping-XBUS command sources.
Guest CPU stores initialize the hidden delta bits from the stored halfword LSB;
other hidden-bit combinations are covered by independent component tests.
Incoming Z is `raw * 4 + 2`, with zero gradients (normalized delta 1). Color,
alpha and initial depth partners vary with the stored word; geometry is fixed.
This is a selected joint domain, not the complete native graphics corpus.

The guest submits an RGBA16 shaded Z triangle with depth comparison/update,
alpha threshold, antialiasing, force blending and full coverage destination.
It reads color and depth RAM plus hidden metadata through an SP helper in MI
EBUS mode, then emits two register lines per case: eight color/hidden words and
thirteen depth/hidden/guard/DP/MI words. No expected result bytes are in the ROM.
Independent host compression, comparator and quarter-grid arithmetic grades
actual output. Packet size is 184 bytes; END is `0x7001b8` or wrapping `0x10a8`.

The 80-case control and full corpus pass. A separately executed wrong-capture
ROM fails every control row; 21 field flips and seven stream controls reject.
Original source and image are reproducible from the manifest. Primitive depth,
RGBA32 guest depth, varying gradients, texture, dithering, two-cycle execution,
full joint operands and physical-console measurements remain pending.

```sh
python3 tests/roms-src/n64/rdp_triangle_depth/build.py
python3 tests/roms-src/n64/rdp_triangle_depth/run.py --emulator build/bin/veloce
ctest --test-dir build -R '^n64_rdp_depth_checkpoint_test$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Artifacts: `build/artifacts/owned/n64_rdp_triangle_depth/`. Production checkpoint
replay checks subsequent actual output and full state through the netplay ABI.
