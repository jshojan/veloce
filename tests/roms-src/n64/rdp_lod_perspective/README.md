# Original N64 fixed-W perspective LOD observations

This MIT-owned cartridge loads five distinct solid 16-bit texture tiles and draws a two-cycle perspective-textured triangle. The guest patches one signed S or T derivative along screen X or Y while keeping W constant, then emits ten framebuffer words and three DP/MI register words to a hexadecimal file. No commercial data or expected framebuffer pixels are embedded.

The **147,456 cases / 1,916,928 observed words** exhaust gradients of 0–255 units of 1/32 texel per pixel on each of four axes, both signs, maximum levels zero/one/three, six W values (`0x1000`, `0x2000`, `0x3000`, `0x4000`, `0x5000`, `0x7fff`), RGBA16/IA16 and RDRAM/XBUS command delivery. The independent oracle implements the perspective reciprocal arithmetic, compares divided coordinates at each pixel with its screen-X/Y neighbors, then computes tile, LOD fraction, combiner, blender, coverage and register results. Non-power-of-two W values yield different fractions within one triangle. Executed wrong-W, wrong-T, wrong-Y, wrong-fraction, wrong-tile and wrong-capture ROMs, 13 field corruptions and seven malformed streams check the oracle.

This profile covers constant-W perspective LOD and exact 1/32-texel input gradients with solid source tiles. W gradients, perspective overflow/crossing behavior, mixed derivatives, detail/sharpen and full mipmap filtering remain unfinished. [Nintendo's texture-mapping manual](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro13/13-07.htm) describes LOD after perspective correction; [Angrylion RDP implementation notes](https://emudev.org/2021/09/21/Angrylion_RDP_Comments) describe the adjacent-coordinate comparison. Direct physical-hardware comparison remains pending.

```sh
python3 tests/roms-src/build.py n64/rdp_lod_perspective --original-only --check
ctest --test-dir build -R '^n64_rdp_lod_perspective$' --output-on-failure
```

Output stays in `build/artifacts/owned/n64_rdp_lod_perspective/`.
