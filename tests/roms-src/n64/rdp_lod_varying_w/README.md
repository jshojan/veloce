# Original N64 varying-W perspective LOD observations

This MIT-owned cartridge draws two-cycle perspective-textured triangles with independently selected S/T and W screen gradients. The guest emits ten framebuffer words and three DP/MI register words per case. It contains no commercial data or expected framebuffer pixels.

The **147,456 cases / 1,916,928 observed words** exhaust 0–127 units of 1/32 texel per pixel for one of four S/T axes, both signs, maximum levels zero/one/three, W bases `0x2000` and `0x5000`, six X/Y W slope pairs (`0`, `+128`, `-128`, and one mixed pair), RGBA16/IA16 and RDRAM/XBUS delivery. W gradients are in the triangle's integer W field per pixel. The independent oracle performs reciprocal division at each pixel and its X/Y neighbors, then grades the tile pair, LOD fraction, combiner, blender, coverage and registers. An executed zero-W-gradient mutant and the other gradient, tile, fraction and capture mutants must fail.

This is a finite functional profile with exact integer triangle gradients and solid source tiles. W crossing/overflow, arbitrary fractional gradients, detail/sharpen, full mipmap filtering, physical timing and direct hardware comparison remain pending.

```sh
python3 tests/roms-src/build.py n64/rdp_lod_varying_w --original-only --check
ctest --test-dir build -R '^n64_rdp_lod_varying_w$' --output-on-failure
```

Output stays in `build/artifacts/owned/n64_rdp_lod_varying_w/`.
