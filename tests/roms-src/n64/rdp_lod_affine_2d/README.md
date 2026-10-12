# Original N64 four-axis affine LOD observations

This MIT-owned cartridge uses five distinct solid 16-bit texture tiles and a two-cycle triangle. For each case, the guest patches signed S/T gradients for horizontal and vertical screen movement into the RDP command, draws it, then emits ten framebuffer words and three DP/MI register words to a hexadecimal file. The cartridge contains no commercial data or expected framebuffer pixels.

The **524,280 cases / 6,815,640 observed words** exhaust all nonzero four-tuples of integer gradient magnitudes from 0 through 15 texels per pixel (`16⁴ − 1`), with both common signs, RGBA16/IA16 texels and RDRAM/XBUS command delivery. The independent oracle selects the largest S/T difference across X/Y, computes the level and eight-bit fraction, decodes the two source tiles, then calculates combiner, blender, coverage and register results. Executed wrong-T-gradient, wrong-Y-gradient, wrong-fraction, wrong-tile and wrong-capture ROMs, 13 field corruptions and seven malformed streams are rejected.

The profile uses an affine vertical major edge, matching d/dEdge and d/dY gradients, exact integer-texel steps, a maximum level of three and minimum level zero. It does not cover fractional gradients, independent gradient signs, perspective correction, detail/sharpen or general mipmap filtering. The [Nintendo 64 Programming Manual](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro13/13-07.htm) defines LOD tile and fraction behavior; the maximum of adjacent S/T differences is described in the [Angrylion RDP implementation notes](https://emudev.org/2021/09/21/Angrylion_RDP_Comments).

```sh
python3 tests/roms-src/build.py n64/rdp_lod_affine_2d --original-only --check
ctest --test-dir build -R '^n64_rdp_lod_affine_2d$' --output-on-failure
```

Output stays in `build/artifacts/owned/n64_rdp_lod_affine_2d/`.
