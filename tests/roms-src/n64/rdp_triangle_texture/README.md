# Original N64 affine texture triangle observations

Original MIT MIPS/RDP commands built with the repository's assembler. No SDK,
firmware or commercial bytes. The guest loads texture source bytes, draws
triangles and captures actual framebuffer RAM, guards, DP CURRENT/STATUS and
MI pending into a register-output file. Expected pixels exist only in the host
[oracle.py](oracle.py).

## Corpus

**786,432 cases / 10,223,616 actual 64-bit observations**:

- 393,216 source cases: every 16-bit word and its complement under RGBA16,
  RGBA32 and IA16, using both RDRAM and wrapping XBUS commands. The source has
  two repeated words per row and a complemented second row; arbitrary RGBA32
  colors and independent four-corner colors are not exhaustive.
- 393,216 sampling cases: all 32-by-32 fractional coordinate pairs, all eight
  format codes and four size codes (including aliases), point/triangular/
  mid-texel filter modes, both triangle directions and both command sources.
  These cases use one selected two-row texture pattern.
- The real texture-image/tile/load/bounds commands feed TMEM before rendering.
  YUV tests retain the native unconverted signed chroma interpretation; they
  do not claim conversion coefficients or converted YUV support.
- One-cycle TEXEL0 passthrough uses cycle one of the combiner, deliberately
  different from cycle zero. Each draw ends with a real FullSync.
- Sixteen framebuffer pixels, two untouched guards and three DP/MI registers
  form each thirteen-word row. The host separately decodes source pixels,
  uses barycentric filter weights and walks an explicit quarter-row point grid.

The guest uses constant S/T coefficients, tile zero, a two-row source, no TLUT,
RGBA32 destination, coverage clamp, no AA/blending/Z/perspective/LOD, disabled
dither and fixed small geometry. Full hardware accuracy, physical timing and
retail playability are not established by this bounded corpus. TLUT rendering
and texture combiner arithmetic have separate component framebuffer goldens.

## Run

```sh
python3 tests/roms-src/build.py n64/rdp_triangle_texture --original-only
cmake --build build --target n64_plugin n64_rdp_triangle_texture_test n64_rdp_packet_plugin_test
ctest --test-dir build -R '^n64_rdp_(triangle_texture|texture_plugin_replay_test)' --output-on-failure
```

Artifacts: `build/artifacts/owned/n64_rdp_triangle_texture/`. The full baseline
passes independently of a short control. A separately executed wrong-capture
ROM changes one captured framebuffer bit in every row and must fail. Thirteen
individual observation-field corruptions and seven malformed/domain/trailer
stream controls must also be rejected. Compilation failures or absent output
cannot count as hardware evidence.

The production-plugin replay gate checkpoints guest-loaded texture/TMEM state
through the netplay ABI, then requires exact subsequent register output and
whole-state equality after restore. It also checks state capacity and atomic
invalid-state rejection. The existing partial-command checkpoint gate remains
strict about capturing a genuinely partial command.
