# Original N64 one-cycle blending observations

This MIT-owned cartridge issues real RDP commands, loads framebuffer and DP/MI
registers, and writes packed hexadecimal observations to a result file. It
contains original code; no SDK, commercial ROM, microcode or expected output
bytes are included.

## Domain and evidence

**655,360 cases × 13 actual 64-bit words = 8,519,680 observations**:

- **524,288**: every 8-bit primitive RGB seed/alpha pair under four selected
  forced/automatic-AA/fog/memory-alpha/coverage/threshold profiles, both RDRAM
  and wrapping XBUS command delivery.
- **131,072**: all 256 cycle-zero blender selector combinations, 256 correlated
  source/alpha patterns, both command sources, with selected coverage destinations.

A constant-shade 4×4 triangle targets RGBA32 memory initialized to `0x5a`.
Primitive RGB channels, primitive alpha and shade alpha are programmed by the
guest; fog/blend colors are distinct fixed values. Native AA coverage writes,
full-alpha bypass, quantized factors, memory-alpha shifters, normalized quotient
and force-blend output wrapping are observable. Thirteen words are eight
framebuffer pairs, two guards, fetched CURRENT, STATUS and MI interrupt state.
FullSync sets the interrupt; end addresses are `0x7001a0` and `0x1090`.

The host independently models source selection, a quotient state machine,
alpha/coverage decisions and an explicit quarter-pixel point grid. It never
imports the production blender or command packing for expected values.
The 1,616-case control passes; a separately executed wrong-capture ROM fails
every row. All thirteen field flips and seven malformed/domain/trailer stream
controls are rejected.

## Repeat

```sh
python3 tests/roms-src/build.py n64/rdp_one_cycle_blend --original-only
python3 tests/roms-src/n64/rdp_one_cycle_blend/run.py
ctest --test-dir build -R '^n64_rdp_(blender|one_cycle_blend)' --output-on-failure
```

Artifacts: `build/artifacts/owned/n64_rdp_one_cycle_blend/` and
`build/artifacts/component/n64_blender/`. The numerical implementation follows
the [pinned primary RDP model](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/blender.c).
All 32,768 quotient entries were also compared byte for byte with its separately
compiled lookup initializer. This is software-model evidence, not physical
console measurement.

Component gates cover all **134,217,728** first/second-color and five-bit-factor
combinations under forced and normalized equations, **2,424,832** alpha/coverage
and threshold decisions, and **1,310,720** selector/delta/latch observations.
Ten compiled production faults are rejected. **1,048,576** command captures cover
selected 16-bit color/complement patterns, both color commands, all opcode aliases,
ignored upper command bits and both delivery paths. Warning-as-error/UBSAN pass.
Additional framebuffer goldens cover retained memory RGB with image reads
disabled and point/filtered perspective texture compositing.

S026 appends 24 bytes for fog color, blend color and retained memory RGB, totaling
8,478,976 bytes. Reserved bits reject atomically; S025 imports retain every old
payload byte and initialize these fields to zero. Production netplay checkpoint
and subsequent actual output/full-state replay pass with populated blender state.

Joint RGB/alpha/color/state/texture/geometry combinations are not exhaustive.
The guest uses fixed fog/blend colors and background coverage. Image-read-disabled
retention and color-on-coverage behavior have component evidence. Depth, two-cycle
blending, dithering, hidden memory and physical timing remain unfinished. Mario
boot/playability remain unverified.
