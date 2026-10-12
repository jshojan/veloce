# Original N64 mixed-gradient perspective LOD observations

This MIT-owned cartridge draws two-cycle perspective-textured triangles whose S, T and W gradients vary independently across screen X, screen Y and the triangle edge. The guest emits ten framebuffer words and three DP/MI register words per case. No commercial data or expected framebuffer pixels are embedded in the ROM.

The **24,576 cases / 319,488 observed words** exhaust eight declared S/T vectors, all 16 independent sign masks, two edge-gradient arrangements, four initial W values (`0`, `64`, `0x2000`, `0xff80`), six W screen-gradient pairs, RGBA16/IA16 and RDRAM/XBUS command delivery. W values and derivatives exercise zero, negative and crossing behavior. The independent Python oracle calculates reciprocal division and signed-interval overflow for the current pixel and its X/Y neighbors. Any flagged coordinate forces the distant tile and LOD fraction. It then grades tile blending, coverage, framebuffer bytes and DP/MI registers. Executed wrong-edge, wrong-sign, wrong-W, wrong-W-sign, wrong-zero-W, wrong-T, wrong-Y, wrong-tile, wrong-fraction and wrong-capture ROMs, 13 field corruptions and seven malformed streams must fail.

This is a finite functional profile with exact 1/32-texel S/T gradients, integer W gradients and solid source tiles. Arbitrary fractional gradients, non-solid mipmap filtering, detail/sharpen, physical timing and direct hardware comparison remain pending. [Angrylion's RDP notes](https://emudev.org/2021/09/21/Angrylion_RDP_Comments) describe the adjacent divided-coordinate comparison and LOD clamp when division flags are set.

```sh
python3 tests/roms-src/build.py n64/rdp_lod_perspective_mixed --original-only --check
ctest --test-dir build -R '^n64_rdp_lod_perspective_mixed$' --output-on-failure
```

Output stays in `build/artifacts/owned/n64_rdp_lod_perspective_mixed/`.
