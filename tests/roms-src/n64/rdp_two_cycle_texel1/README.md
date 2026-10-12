# Original N64 two-tile TEXEL1 observations

This MIT-owned cartridge loads two distinct 16-bit texels through actual RDP
texture-image, tile and load commands. Cycle zero forwards TEXEL0 from the
primitive tile. Cycle one computes `(COMBINED - TEXEL1) × PRIM_ALPHA + TEXEL1`
using the following tile, then the second blender stage combines the result
with framebuffer memory. The guest writes framebuffer and DP/MI register words
to a hexadecimal result file; it contains no expected pixels or commercial data.

The finite corpus covers **262,144 cases × 13 guest 64-bit words = 3,407,872
observations**: every 16-bit source texel in RGBA16 and IA16, with both RDRAM
and XBUS command delivery. The following tile holds the bitwise complement of
the first tile's texel, so accidentally resampling TEXEL0 changes the result.
An independent host oracle decodes both texels, computes the two combiner
cycles, quarter-pixel coverage and memory blending, and checks two untouched
guards plus DP CURRENT/STATUS and MI interrupt state. A 24-case control,
executed wrong-capture and wrong-TEXEL1 ROMs, 13 field corruptions and seven
malformed streams check the grader.

The profile uses point sampling, fixed primitive alpha, no LOD or depth and
zero first-stage fog. Filtering, LOD selection, other formats and complete
two-cycle hardware behavior need separate evidence. The tile-plus-one rule
with LOD disabled is documented in Nintendo's
[texture overview](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro13/13-07.html).

```sh
python3 tests/roms-src/build.py n64/rdp_two_cycle_texel1 --original-only --check
python3 tests/roms-src/n64/rdp_two_cycle_texel1/run.py
ctest --test-dir build -R '^n64_rdp_two_cycle_texel1$' --output-on-failure
```

Output files are under `build/artifacts/owned/n64_rdp_two_cycle_texel1/`.
