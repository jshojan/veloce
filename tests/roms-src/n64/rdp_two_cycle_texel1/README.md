# Original N64 two-tile TEXEL1 observations

This MIT-owned cartridge loads two distinct 16-bit texels through actual RDP
texture-image, tile and load commands. Cycle one interpolates COMBINED against
TEXEL0 or TEXEL1 with one of three coefficients: TEXEL0 alpha, TEXEL1 alpha or
fixed primitive alpha. The second blender stage combines the result with
framebuffer memory. The guest writes framebuffer and DP/MI register words to a
hexadecimal result file; it contains no expected pixels or commercial data.

The finite corpus covers **786,432 cases × 13 guest 64-bit words = 10,223,616
observations**: every 16-bit source texel in RGBA16 and IA16, three distinct
combiner selectors and both RDRAM and XBUS command delivery. The following tile
holds the bitwise complement of the first tile's texel. For the TEXEL0-alpha
variant, cycle zero forwards fixed primitive RGB and cycle one uses TEXEL0 RGB
as its base; the other variants forward TEXEL0 then use TEXEL1 RGB as their base.
The two tile alphas are therefore complementary in this corpus; independent
tile pairs still need separate evidence.
An independent host oracle decodes both texels, computes the two combiner
cycles, quarter-pixel coverage and memory blending, and checks two untouched
guards plus DP CURRENT/STATUS and MI interrupt state. A 72-case control,
executed wrong-capture, wrong-TEXEL1 and wrong-alpha-selector ROMs, 13 field
corruptions and seven malformed streams check the grader.

The profile uses point sampling, fixed primitive color, no LOD or depth and
zero first-stage fog. Filtering, LOD selection, other formats and complete
two-cycle hardware behavior need separate evidence. The tile-plus-one rule
with LOD disabled is documented in Nintendo's
[texture overview](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro13/13-07.html).
The [combiner manual](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro12/12-06.html)
lists both texels and their alpha values as selector inputs.

```sh
python3 tests/roms-src/build.py n64/rdp_two_cycle_texel1 --original-only --check
python3 tests/roms-src/n64/rdp_two_cycle_texel1/run.py
ctest --test-dir build -R '^n64_rdp_two_cycle_texel1$' --output-on-failure
```

Output files are under `build/artifacts/owned/n64_rdp_two_cycle_texel1/`.
