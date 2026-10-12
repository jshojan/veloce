# Original N64 affine LOD tile-level observations

This MIT-owned cartridge loads five different solid 16-bit texel rows into
TMEM and defines five tile descriptors, one for each row. A two-cycle triangle
uses an exact signed affine S gradient. The guest writes framebuffer and DP/MI
register words to a hexadecimal result file. It contains source texels but no
expected framebuffer pixels or commercial data.

The corpus covers **8,192 cases × 13 guest 64-bit words = 106,496
observations**: every integer S gradient from 0 through 255 texels per pixel,
both signs, maximum LOD levels 1 and 3, LOD on/off, RGBA16 and IA16 decoding,
and RDRAM/XBUS command delivery. With LOD on, the oracle derives the level
from the signed coordinate difference. A negative delta loses one 1/32-texel
unit in the RDP absolute-value path. Magnification selects the primitive tile
twice; at or beyond the maximum level, both cycles use that coarsest tile.
Otherwise cycle one uses the next tile. With LOD off, the cycles select the
primitive tile and primitive tile+1.
The independent oracle decodes the selected texels, computes combiner and
blender output, quarter-pixel coverage, untouched guards and DP/MI registers.
An executed wrong-tile, wrong-LOD and wrong-maximum-level ROM, an executed
wrong-capture ROM, 13 field corruptions and seven malformed streams check it.

This is the affine, horizontal, exact integer-texel gradient domain. Tile
textures are solid and fixed. This corpus does not grade fractional gradients,
T/edge/Y gradients, perspective, LOD fractions in the combiner, detail/sharpen
or full mipmap filtering. Nintendo's
[tile-selection tables](https://ultra64.ca/files/documentation/online-manuals/man-v5-1/pro-man/pro13/13-07.htm)
define the level clamp and cycle tile offsets used here.

```sh
python3 tests/roms-src/build.py n64/rdp_lod_affine_levels --original-only --check
python3 tests/roms-src/n64/rdp_lod_affine_levels/run.py
ctest --test-dir build -R '^n64_rdp_lod_affine_levels$' --output-on-failure
```

Output files are under `build/artifacts/owned/n64_rdp_lod_affine_levels/`.
