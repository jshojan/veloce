# Original N64 affine LOD fraction observations

This MIT-owned cartridge loads five distinct solid texels into TMEM and draws a two-cycle textured triangle with exact signed affine S gradients. The guest emits ten framebuffer words and three DP/MI register words per case. It contains no commercial ROM data or expected framebuffer pixels.

The **360 cases / 4,680 observed words** cover gradients of 1–15 texels per pixel in both directions, RGBA16 and IA16, RDRAM and XBUS command delivery, and three combiner mux variants: LOD fraction as the RGB factor, the alpha factor, or both. The independent oracle uses the signed coordinate difference, including the one-unit negative-delta offset. Magnification selects one tile with fraction zero; reaching maximum level selects one tile with fraction 255. Between levels, it computes the eight-bit fraction, decodes source texels, then computes combiner, blender, coverage and register results. Executed wrong-fraction, wrong-tile and wrong-capture ROMs, 13 field corruptions and seven malformed streams are rejected.

This finite profile uses exact horizontal integer gradients, solid textures, maximum level three, minimum level zero, and no perspective, detail or sharpen. This cartridge does not grade fractional/T/Y gradients, per-pixel perspective LOD or full mipmap filtering. The [Nintendo 64 Programming Manual tile-selection section](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro13/13-07.htm) describes tile levels and the normalized LOD fraction; [Angrylion RDP implementation notes](https://emudev.org/2021/09/21/Angrylion_RDP_Comments) describe the signed-difference and distant-level rules.

```sh
python3 tests/roms-src/build.py n64/rdp_lod_fraction --original-only --check
ctest --test-dir build -R '^n64_rdp_lod_fraction$' --output-on-failure
```

Output stays in `build/artifacts/owned/n64_rdp_lod_fraction/`.
