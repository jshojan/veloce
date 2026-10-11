# Original N64 perspective texture observations

This MIT-owned cartridge sends real RDP command lists, reads framebuffer and
DP/MI registers with guest loads, and writes packed hexadecimal observations
to the test sink. It contains original code and an original 8×8 RGBA16 grid;
no commercial ROM, SDK, microcode or expected framebuffer bytes are included.

## Domain

**1,572,864 cases × 13 actual 64-bit words = 20,447,232 observations**:

- **524,288**: every raw 16-bit W, four selected constant S/T pairs, both RDRAM
  and wrapping XBUS command delivery.
- **1,048,576**: every raw 16-bit S with complementary T, eight selected W
  values, both command sources.

These exercise reciprocal normalization, the native segment-six endpoint,
positive/negative and zero W, division overflow/underflow and coordinate
saturation before masked tile sampling. Each case uses a constant-coordinate
4×4 triangle, tile zero, point sampling, an RGBA16 source and RGBA32 output.
Words are eight framebuffer pairs, two guard words, fetched CURRENT, STATUS
and MI interrupt state. End addresses are `0x7001a8` and `0x1098` respectively;
XBUS command fetch crosses the 4 KiB DMEM boundary. Real FullSync produces the
interrupt observation.

The host oracle derives reciprocal interpolation with integer arithmetic,
models the logical grid and uses an explicit quarter-pixel coverage grid.
It does not import command packing, production divider/fetch/filter routines,
or any expected output from the ROM. A 288-case control must pass; a separately
executed wrong-capture cartridge must fail every row. Thirteen field flips and
seven malformed/domain/completion/trailer controls must also be rejected.

## Run

```sh
python3 tests/roms-src/build.py n64/rdp_texture_perspective --original-only
python3 tests/roms-src/n64/rdp_texture_perspective/run.py
ctest --test-dir build -R '^n64_rdp_(texture_divider|perspective)' --output-on-failure
```

Artifacts: `build/artifacts/owned/n64_rdp_texture_perspective/`. The registered
scalar divider gate checks all **2³² scalar S/W pairs** using independent
piecewise endpoint data and a signed interval overflow oracle; T is the
complementary lane. Every 19-bit saturation encoding and affine identity are
also checked. Its mutation runner uses a shorter boundary corpus to reject
nine compiled production faults. Component framebuffer goldens additionally
use nonzero tile seven, X/Y W gradients, point/filtered sampling and preserve
atomic rejection of unsupported modes. A production S025 checkpoint requires
perspective mode and actual nonzero TMEM; subsequent guest output and full
state must replay byte for byte through the netplay interface.

The native numerical behavior is compared against the
[pinned RDP implementation](https://github.com/ata4/angrylion-rdp-plus/blob/192ad5de5b0f50bfa55f9f97208feac4bc87f885/src/core/n64video/rdp/tcoord.c).
This is software-model evidence; physical console measurements remain absent.

This is not an exhaustive joint S/T/W, texture, tile, gradient, geometry or
pipeline corpus. LOD/mipmapping, two-cycle feedback, AA, blending, depth,
hidden RDRAM bits and cycle-accurate rendering remain unfinished. Mario boot
and playability are not established by this test.
