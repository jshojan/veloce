# Original N64 detail and sharpen LOD observations

This MIT-owned cartridge draws two-cycle triangles with five distinct solid texture tiles. The guest patches signed S gradients and SetPrimColor's minimum-LOD field, then emits ten framebuffer words and three DP/MI register words per case. It contains no commercial data or expected framebuffer pixels.

The **433,152 cases / 5,630,976 observed words** cover all four detail/sharpen bit combinations, all 32 minimum-LOD values, both gradient signs, maximum levels zero/one/three, RGBA16/IA16, RDRAM/XBUS delivery, and three coordinate profiles: affine, perspective with W=`0x4000`, and perspective with W=zero. The 47 gradient magnitudes exhaust 0–31 units of 1/32 texel plus `32,33,63,64,65,127,128,255,256,511,512,1023,2047,4095,8160`.

The independent Python oracle calculates signed reciprocal intervals, LOD signals, detail tile offsets, sharpening's signed nine-bit multiplier, combiner clamping, blending, coverage and DP/MI registers. Executed wrong-style, wrong-minimum, wrong-maximum, wrong-sign, wrong-perspective, wrong-tile, wrong-fraction and wrong-capture cartridges must fail, as must thirteen field corruptions and seven malformed streams. Shared command templates keep the full case table below the homebrew RAM limit.

This finite profile uses point sampling and solid source tiles. Arbitrary fractional derivatives, non-solid mipmap filtering, physical pipeline timing and direct hardware comparison remain pending. [Angrylion's RDP notes](https://emudev.org/2021/09/21/Angrylion_RDP_Comments) document detail tile selection, signed sharpening fractions and minimum-LOD behavior.

```sh
python3 tests/roms-src/build.py n64/rdp_lod_detail_sharpen --original-only --check
ctest --test-dir build -R '^n64_rdp_lod_detail_sharpen$' --output-on-failure
```

Local result files stay in `build/artifacts/owned/n64_rdp_lod_detail_sharpen/`.
