# Original N64 independent edge and vertical LOD observations

This MIT-owned cartridge draws a two-cycle textured triangle from five distinct solid 16-bit texture tiles. The guest patches signed S/T gradients for screen X, screen Y and the triangle edge independently, then emits ten framebuffer words and three DP/MI register words to a hexadecimal file. No commercial ROM data or expected pixels are embedded.

The **165,760 cases / 2,154,880 observed words** exhaust every nonzero four-tuple from the bounded subtexel set `{0, 1, 31, 32, 63, 255}` (units of 1/32 texel), all 16 independent sign masks, two edge-gradient arrangements, RGBA16/IA16 and RDRAM/XBUS command delivery. In one arrangement d/dEdge equals d/dY; in the other the edge gradients are fixed at `(511, -511)` and deliberately differ from d/dY. The independent oracle derives LOD only from S/T changes along screen X/Y, then computes tile, fraction, combiner, blender, coverage, untouched guards and DP/MI register results. An executed changed-edge control must preserve all observations. Executed wrong-T, wrong-Y, wrong-sign, wrong-fraction, wrong-tile and wrong-capture ROMs, 13 field corruptions and seven malformed streams are checked.

This profile supports affine, nonperspective LOD with d/dEdge independent of d/dY and exact 1/32-texel derivative steps. The source textures are solid, so this cartridge isolates LOD selection from texture-coordinate variation; a separate non-solid-texel corpus is needed for edge interpolation itself. Perspective LOD, sub-unit gradients, detail/sharpen and full mipmap filtering remain unfinished. [Angrylion RDP implementation notes](https://emudev.org/2021/09/21/Angrylion_RDP_Comments) show that LOD reads d/dY separately from d/dEdge. Direct physical-hardware comparison remains pending.

```sh
python3 tests/roms-src/build.py n64/rdp_lod_edge_gradient --original-only --check
ctest --test-dir build -R '^n64_rdp_lod_edge_gradient$' --output-on-failure
```

Output stays in `build/artifacts/owned/n64_rdp_lod_edge_gradient/`.
