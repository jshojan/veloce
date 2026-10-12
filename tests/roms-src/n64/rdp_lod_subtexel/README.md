# Original N64 subtexel affine LOD observations

This MIT-owned cartridge loads five distinct solid 16-bit texel tiles into TMEM. For each case, its guest patches one signed S or T gradient along screen X or Y into a two-cycle triangle, draws it, then emits ten framebuffer words and three DP/MI register words to a hexadecimal file. The ROM contains no commercial data or expected pixels.

The **49,152 cases / 638,976 observed words** cover every gradient from 0 through 511 units of 1/32 texel per pixel on each of four axes, both signs, maximum levels zero, one and three, RGBA16/IA16 and RDRAM/XBUS command delivery. The independent oracle derives LOD from adjacent signed coordinate differences. A negative delta has a one-unit absolute-value offset; magnification reuses the primitive tile and produces fraction zero when another mip level exists. At maximum level zero, and whenever the coarsest level is reached, both cycles use that tile and the fraction clamps to 255. It then computes combiner, blender, coverage, untouched guards and DP/MI register results. Executed wrong-T-gradient, wrong-Y-gradient, wrong-fraction, wrong-tile and wrong-capture ROMs, 13 field corruptions and seven malformed streams check the oracle.

This is a bounded affine, vertical-major-edge profile with one active gradient per case and minimum level zero. Mixed fractional gradients, perspective, detail/sharpen and full mipmap filtering remain unfinished. The [Nintendo 64 Programming Manual](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro13/13-07.htm) describes LOD tile and fraction behavior. [Angrylion RDP implementation notes](https://emudev.org/2021/09/21/Angrylion_RDP_Comments) document the signed-difference and distant-level rules used for this profile. These ROM observations verify the implementation against the stated model; a physical hardware capture remains separate evidence.

```sh
python3 tests/roms-src/build.py n64/rdp_lod_subtexel --original-only --check
ctest --test-dir build -R '^n64_rdp_lod_subtexel$' --output-on-failure
```

Output stays in `build/artifacts/owned/n64_rdp_lod_subtexel/`.
