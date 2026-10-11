# Original N64 one-cycle shade framebuffer observations

Original MIT MIPS/RDP commands, using the repository's DP assembler and original
fill-suite geometry producer. No SDK, firmware or commercial ROM bytes.
The guest reads actual framebuffer RAM, guards, CURRENT, STATUS and MI pending;
expected pixels appear only in [oracle.py](oracle.py).

## Corpus

**792,576 cases / 10,303,488 actual 64-bit observations**:

- 786,432 color cases: every 16-bit RG pair and its complemented BA pair,
  three combiner programs, both triangle directions and RDRAM/wrapping XBUS.
  This does not exhaust arbitrary 32-bit RGBA colors or all gradients.
- 6,144 geometry/program cases: eight edge/scissor profiles, both directions,
  eight combiner programs, three field selections, both command sources,
  all four coverage destinations and both framebuffer-read settings.
- Shade, primitive and environment passthrough, shade times primitive alpha,
  primitive/environment interpolation, ONE, ZERO and primitive-LOD arithmetic.
- Nonzero RGBA X/edge/Y gradients; fractional/negative top coordinates,
  fractional middle changes, partial clipping, thin/empty/crossed triangles.
- A deliberately different cycle-zero program verifies that one-cycle uses
  cycle one. Each packet ends with a real FullSync.

Each case captures the sixteen-pixel RGBA32 buffer as eight words, two guard
words and three DP/MI registers. The host owns a separate coordinate/gradient
specification, walks scalar quarter-rows, samples the native point grid, and
calculates interpolated RGB and coverage independently of the C++ module and
guest command packing. Framebuffer width remains a stride.

## Run

```sh
python3 tests/roms-src/build.py n64/rdp_triangle_shade --original-only
python3 tests/roms-src/n64/rdp_triangle_shade/run.py
ctest --test-dir build -R '^n64_rdp_triangle_shade$' --output-on-failure
```

The full corpus and all 6,216 short baseline cases pass. The executed capture
mutant fails all 6,216 short cases; thirteen field controls each detect one
mismatch, and all seven malformed-stream controls reject.

The runner requires the full corpus and a 6,216-case short baseline, an executed
capture mutant that must fail every short case, thirteen field controls and
seven malformed-stream controls. It validates identity/domain/count/width,
completion, trailer and EOF. Artifacts:
`build/artifacts/owned/n64_rdp_triangle_shade/`.
The archive/generator hashes are in [the ROM ledger](../../rom_manifest.json).

## Remaining hardware scope

This grades the functional one-cycle RGBA32 path with AA/blending, dither,
alpha compare, keying, texture fetch and depth accesses disabled. With AA off,
the upper-left coverage sample gates writes; RGB and native coverage occupy
the framebuffer word. Alpha equation routing has separate component evidence;
it is not stored as framebuffer alpha. Previous-pixel feedback, texture/LOD,
other framebuffer formats, hidden RDRAM bits, Z testing and physical timing
remain unfinished. Unsupported profiles reject before writes. Mario's textured,
depth-tested profile remains blocked; this original corpus does not prove
retail boot or playability.
