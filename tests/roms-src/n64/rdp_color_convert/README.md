# Original N64 SetConvert and converted triangle observations

Original MIT MIPS/RDP source, built with the repository's assembler. No SDK,
firmware or commercial bytes. The guest issues SetConvert, loads real texture
source bytes, draws triangles and records actual framebuffer/guard RAM and
DP CURRENT/STATUS plus MI pending. Expected colors exist only in the host
[oracle.py](oracle.py), using logical source pixels and independent signed
coefficient/combiner arithmetic.

## Corpus

**1,318,912 cases / 17,145,856 actual 64-bit observations**:

- 1,048,576 color-conversion cases: each K0..K3 field independently covers all
  512 encodings against all 256 selected source-byte seeds, under both RDRAM
  and wrapping XBUS. Other coefficients remain at specified constants.
  UV pairs and luma derive from a repeated/complemented source pattern; this
  does not exhaust every independent U/V/Y triple or joint K0..K3 tuple.
- 262,144 K4/K5 cases: every pair of nine-bit coefficients drives the real
  cycle-one RGB equation `(TEXEL0-K4)*K5+TEXEL0`. K4 uses the native special
  extension and K5 ordinary signed nine-bit extension. These cases use one
  RGBA16 source family and RDRAM commands.
- 8,192 sampling cases: every 32-by-32 fractional pair, both command sources
  and selected YUV16/YUV32/RGBA16/IA16 tiles with fixed coefficients. Source
  patterns derive from coordinate bits; independent arbitrary corner colors
  are not exhaustive.
- The current TEXEL0 and a real FullSync feed each thirteen-word observation:
  sixteen framebuffer pixels, two guards and three DP/MI registers.

The fixture uses constant S/T coefficients, tile zero, a two-row source,
RGBA32 destination, coverage clamp, no TLUT/AA/blending/Z/perspective/LOD and
disabled dither. It exercises unfiltered color conversion and K4/K5 combiner
routes. Physical timing, complete graphics, two-cycle previous-texel feedback
and native retail playability remain unverified. Separate component framebuffer
goldens distinguish chroma/luma triangle selection, default zero weights,
programmed odd weights and the signed coefficient boundary.

## Run

```sh
python3 tests/roms-src/build.py n64/rdp_color_convert --original-only
cmake --build build --target n64_plugin n64_rdp_convert_test n64_rdp_packet_plugin_test
ctest --test-dir build -R '^n64_rdp_(color_convert|convert_test|conversion_plugin_replay_test)' --output-on-failure
```

Artifacts: `build/artifacts/owned/n64_rdp_color_convert/`. The full baseline
and 425-case control pass. A separately executed wrong-capture ROM must fail
in every row. All thirteen individual observation-field corruptions and seven
malformed/domain/trailer stream controls must be rejected. An absent result
or failed compilation cannot count as hardware evidence.

The production-plugin replay gate checkpoints guest-programmed conversion
state through the netplay ABI and requires exact subsequent guest register
output and complete-state equality after restore. It checks exact S025 capacity
and rejects malformed conversion/parser states atomically. The component gate
also verifies SetConvert aliases/ignored bits and S024 import without conversion
state. State/version details are recorded in the core's latest evidence section.
