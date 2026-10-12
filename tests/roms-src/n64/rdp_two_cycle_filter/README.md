# Original N64 two-cycle two-tile filter observations

This MIT-owned cartridge loads four rows of original 16-bit texture data into
TMEM through actual RDP commands. Tile zero samples the first 2×2 neighborhood;
tile one samples a distinct 2×2 neighborhood. The two texture-filter cycles
produce TEXEL0 and TEXEL1, and the color combiner interpolates between them
using primitive alpha. The guest writes framebuffer and DP/MI register words to
a hexadecimal file, with no expected pixels or commercial data in the ROM.

The finite corpus covers **65,536 cases × 13 guest 64-bit words = 851,968
observations**: all 32×32 fractional S/T pairs, RGBA16 and IA16 decoders,
triangular filtering with and without the mid-texel box rule, eight primitive
alpha values, and both RDRAM and XBUS command delivery. The host oracle
independently decodes each texel, computes both four-corner filters, two-cycle
color combination, framebuffer blending, coverage, untouched guards and DP/MI
register state. A 384-case control, executed wrong-capture, wrong-second-tile,
point-sample and wrong-mid-texel ROMs, 13 field corruptions and seven malformed
streams check the grader.

This is a fixed 2×2 texture neighborhood and a no-LOD profile. Independent
texture values across the full 16-bit Cartesian product, other formats,
perspective, mip selection, depth and complete RDP behavior need separate
evidence. Nintendo's [texture-filter documentation](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro12/12-05.html)
describes the RDP's triangular interpolation, and its
[texture-engine documentation](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro12/12-04.html)
describes the two-tile fetch in two-cycle mode.

```sh
python3 tests/roms-src/build.py n64/rdp_two_cycle_filter --original-only --check
python3 tests/roms-src/n64/rdp_two_cycle_filter/run.py
ctest --test-dir build -R '^n64_rdp_two_cycle_filter$' --output-on-failure
```

Output files are under `build/artifacts/owned/n64_rdp_two_cycle_filter/`.
