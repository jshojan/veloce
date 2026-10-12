# Original N64 textured two-cycle observations

This MIT-owned cartridge loads 16-bit texture sources through real RDP
texture-image, tile and load commands. It draws a two-cycle triangle and writes
framebuffer and DP/MI register observations to a hexadecimal result file. No
SDK, commercial bytes or expected pixel data are embedded in the ROM.

The corpus covers **262,144 cases × 13 guest 64-bit words = 3,407,872
observations**: every 16-bit source texel under RGBA16 and IA16, delivered
through both RDRAM and XBUS command paths. Cycle zero forwards TEXEL0 to
COMBINED, cycle one consumes that result, and the second blender stage blends
with framebuffer memory using the texel alpha. The triangle has zero shade
alpha, so the first fog stage forwards the combined RGB. This specifically
checks real TMEM loading, TEXEL0 feedback and framebuffer blending together.

The host oracle independently decodes source pixels and calculates the
triangle's quarter-pixel coverage and blender output. Each row also captures
two untouched guards, DP CURRENT/STATUS and MI interrupt state. A 24-case
control, separately executed wrong-capture ROM, 13 field corruptions and seven
malformed stream controls check the grader.

The finite domain uses point sampling, one tile, fixed geometry, no depth or
LOD, and zero shade alpha. Other texture formats, filtering, nonzero fog and
complete two-cycle hardware behavior need separate evidence.

```sh
python3 tests/roms-src/build.py n64/rdp_two_cycle_texture --original-only --check
python3 tests/roms-src/n64/rdp_two_cycle_texture/run.py
ctest --test-dir build -R '^n64_rdp_two_cycle_texture$' --output-on-failure
```

Output files are under `build/artifacts/owned/n64_rdp_two_cycle_texture/`.
