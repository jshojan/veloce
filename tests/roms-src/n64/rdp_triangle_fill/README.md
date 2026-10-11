# Original N64 fill-cycle triangle observations

Repository-owned MIT MIPS and numeric RDP commands; no SDK, firmware or
commercial ROM bytes. The guest reads actual RDRAM framebuffer words, guards,
CURRENT, STATUS and MI pending through the register-output channel. Expected
pixels exist only in [oracle.py](oracle.py).

## Corpus

**795,648 cases / 10,343,424 actual 64-bit observations**:

- 786,432 color cases: all 65,536 halfword patterns, CI8/RGBA16/RGBA32,
  RDRAM/wrapping XBUS and both triangle directions. The fill word combines the
  pattern with its complement; this does not exhaust all 32-bit fill words.
- 9,216 geometry cases: eight edge/scissor profiles, both directions, eight
  triangle types with four opcode-byte aliases, three field selections, both
  sources and three pixel sizes, with fill pattern `0x1357`.
- Geometry includes fractional/negative top coordinates, fractional middle-edge
  changes, partial scissoring, a thin span, an empty triangle and crossed edges.
- Every attribute payload word starts with FullSync's byte. Real FullSync occurs
  after the complete triangle. Both buffer guards and actual interrupt/state
  registers are captured.

Each case reports eight words covering the 64-byte framebuffer region, two
guard words and three registers. The host oracle walks quarter-scanlines using
scalar signed arithmetic, quantizes/clips bounds and produces byte-addressed
fill expectations independently of the C++ renderer and guest command packer.
Field selection and lower-edge transitions are included. Bit 55 set places the
major edge on the left in this raster path; the old command summary's direction
wording is checked against the command producer and raster implementation.

## Verification

The full corpus passes. The **9,288-case** short control passes; an executed
guest capture mutant fails all 9,288 cases. Thirteen single-field mutations
fail once each; seven malformed-stream controls reject. Identity, domain,
row width/count, completion, terminator/trailer and EOF are required.

```sh
python3 tests/roms-src/build.py n64/rdp_triangle_fill --original-only
python3 tests/roms-src/n64/rdp_triangle_fill/run.py
ctest --test-dir build -R '^n64_rdp_triangle_' --output-on-failure
```

Artifacts: `build/artifacts/owned/n64_rdp_triangle_fill/`. The archive and
generator hashes are in [the ROM ledger](../../rom_manifest.json).

## Scope still pending

This grades fill-cycle byte writes and the declared packet/geometry/color
domains. One/two-cycle shading, texture sampling, blending, depth processing,
hidden RDRAM coverage bits, crash partial writes, native raster/FIFO timing and
retail playability remain pending. It does not establish exhaustive native RDP
accuracy. Bounds and unsafe fill profiles reject before writes under the
existing functional convention; physical partial-write behavior is ungraded.

Sources: [SGI RDP command formats, tables 11–15](https://hcs64.com/files/RDP_COMMANDS.pdf),
[libdragon's command producer](https://github.com/DragonMinded/libdragon/blob/trunk/src/rdpq/rdpq_tri.c),
[the RDP author's raster implementation](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/rasterizer.c).
