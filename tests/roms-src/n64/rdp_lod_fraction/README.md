# Original N64 affine LOD fraction observations

This MIT-owned cartridge loads five distinct solid texels into TMEM and draws a two-cycle textured triangle with exact signed affine S gradients. The guest emits ten framebuffer words and three DP/MI register words per case. It contains no commercial ROM data or expected framebuffer pixels.

The **360 cases / 4,680 observed words** cover gradients of 1–15 texels per pixel in both directions, RGBA16 and IA16, RDRAM and XBUS command delivery, and three combiner mux variants: LOD fraction as the RGB factor, the alpha factor, or both. The independent oracle chooses mip tiles from `floor(log2(step))`, computes an 8-bit fraction from the interval between adjacent powers of two, decodes source texels, then computes combiner, blender, coverage and register results. Executed wrong-fraction, wrong-tile and wrong-capture ROMs, 13 field corruptions and seven malformed streams are rejected.

This finite profile uses exact horizontal integer gradients, solid textures, maximum level three, minimum level zero, no perspective, detail or sharpen, and LOD fractions only when the natural level is within that maximum. Fractional/T/Y gradients, capped fractions, per-pixel perspective LOD and full mipmap filtering remain unfinished. The [Nintendo 64 Programming Manual tile-selection section](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro13/13-07.htm) describes tile levels and the normalized LOD fraction.

```sh
python3 tests/roms-src/build.py n64/rdp_lod_fraction --original-only --check
ctest --test-dir build -R '^n64_rdp_lod_fraction$' --output-on-failure
```

Output stays in `build/artifacts/owned/n64_rdp_lod_fraction/`.
