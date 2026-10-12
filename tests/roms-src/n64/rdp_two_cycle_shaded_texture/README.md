# Original N64 shaded, textured two-cycle observations

This MIT-owned cartridge issues real RDP texture-image, tile and TMEM load
commands, then draws a shaded, textured two-cycle triangle. It writes actual
framebuffer pixels and DP/MI register values to a hexadecimal result file. No
commercial ROM bytes, SDK, microcode or expected framebuffer bytes are used.

The full corpus covers **262,144 guest cases × 13 64-bit words = 3,407,872
observations**: every 16-bit source texel under RGBA16 and IA16, each through
RDRAM and XBUS command delivery. Cycle zero combines TEXEL0 with constant
half-scale shade RGB; cycle one reads COMBINED. A varying shade alpha blends
the result with a distinct fog color in the first blender stage. The second
stage blends against framebuffer memory according to texel alpha. Shade alpha
is derived from the high source byte, so texel and shade inputs are correlated.

The independent host oracle decodes the texture source, computes combiner and
both blender stages, walks the quarter-pixel triangle coverage grid, and grades
eight framebuffer pairs, two guards and DP/MI registers. A 24-case control,
executed wrong-capture ROM, 13 field corruptions and seven malformed stream
controls check the grader.

The finite domain uses point sampling, one tile, fixed geometry, no depth or
LOD, and constant shade RGB. Independent shade color/alpha patterns, filtering,
depth and general two-cycle hardware behavior remain unverified.

```sh
python3 tests/roms-src/build.py n64/rdp_two_cycle_shaded_texture --original-only --check
python3 tests/roms-src/n64/rdp_two_cycle_shaded_texture/run.py
ctest --test-dir build -R '^n64_rdp_two_cycle_shaded_texture$' --output-on-failure
```

Output files are under `build/artifacts/owned/n64_rdp_two_cycle_shaded_texture/`.
