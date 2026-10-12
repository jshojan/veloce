# Original N64 magnifying LOD tile observations

This MIT-owned cartridge loads two different 16-bit texels into consecutive
RDP tiles. It draws two-cycle triangles with constant S/T/W coordinates, once
with texture LOD disabled and once with LOD enabled. With LOD disabled,
cycle-one TEXEL1 comes from the following tile; with LOD enabled and no
detail/sharpen mode, the magnification tile table selects the primitive tile
for both cycles. The guest writes framebuffer and DP/MI register words to a
hexadecimal file. It embeds no expected framebuffer values or commercial data.

The finite corpus covers **524,288 cases × 13 guest 64-bit words = 6,815,744
observations**: all 16-bit source patterns in RGBA16 and IA16, both LOD mode
settings and RDRAM/XBUS command delivery. The second tile contains the bitwise
complement of the first, so wrong selection changes color or alpha. The host
oracle independently decodes the selected source, combines the two cycles,
blends against framebuffer memory and checks quarter-pixel coverage, two
untouched guards and DP CURRENT/STATUS plus MI interrupt state. A 48-case
control, executed wrong-capture, wrong-second-tile and wrong-LOD-mode ROMs,
13 field corruptions and seven malformed streams check the grader.

This is the **constant-coordinate magnification** case only. Nonzero texture
gradients, minification, LOD fraction, detail/sharpen, mipmap chains,
perspective LOD and physical timing need separate implementation and evidence.
The tile mapping comes from Nintendo's
[LOD tile-selection tables](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro13/13-07.html).

```sh
python3 tests/roms-src/build.py n64/rdp_lod_magnify --original-only --check
python3 tests/roms-src/n64/rdp_lod_magnify/run.py
ctest --test-dir build -R '^n64_rdp_lod_magnify$' --output-on-failure
```

Output files are under `build/artifacts/owned/n64_rdp_lod_magnify/`.
