# RDP combiner arithmetic

`RdpCombiner::rgb_equation`, `alpha_equation` and `clamp` implement the integer
arithmetic used by the RDP `(A - B) * C + D` datapath. This is an internal
arithmetic kernel for the pending draw pipeline; guest triangles remain blocked.
Command state and S022 are unchanged. ABI v2/plugin 0.26.0 remain unchanged.

Operands are encoded nine-bit values. A/B/D extend `0x180..0x1ff` to
`-128..-1`, while `0x100..0x17f` stay positive. C uses ordinary signed nine-bit
extension. The fixed-point numerator includes a 128 rounding bias. RGB retains
the **full 17-bit residue**, including fractional bits needed by later keying
stages; alpha exposes the upper nine bits. The special clamp maps negative
encodings to zero and positive 256..383 to 255. No host floating point, signed
left shift or signed overflow is used.

The component test uses signed enumeration and Euclidean remainder arithmetic,
with **262,144 A/B pairs**, **268,173,312 difference/C/D cases**, **512 clamp
values**, and literal rounding/wrap boundaries. The finite reduction is explicit:

1. A/B each decode to every integer in -128..383. All 512 × 512 pairs are checked
   at C=1/D=0, covering their special extension and all differences -511..511.
2. The equation depends on A and B only through that difference. One actual
   encoded representative pair for every difference is tested against all 512 C
   factors and 512 D addends: 1,023 × 512 × 512 = 268,173,312 cases.
3. Every encoded clamp value is checked against signed-value saturation.

This proves the stated scalar arithmetic domain under the documented reduction;
it is not an exhaustive RDP hardware corpus. An [optional, repeatable comparison](../../tests/reference/n64-combiner/README.md)
against pinned Angrylion RDP Plus also checks every reduced RGB/alpha case and
clamp value using its actual routines. Undefined-behavior sanitizer checks pass.
The normal CMake test has no dependency on that reference or network access.

Run `ctest --test-dir build -R '^n64_rdp_combiner_math_test$' --output-on-failure`.
The separate original `rdp_combiner_commands` ROM continues to grade real command
register captures; it does not read back this arithmetic kernel. Guest draw
readback will be required to verify its eventual integration.

Remaining: operand source routing and the complete one-/two-cycle pixel pipeline,
keying, coverage/alpha coupling, dither, texture sampling, rasterization, blending,
depth and hardware timing. **Native Mario boot/playability remain unverified and
N64 remains 40% (8/20 delivery milestones).** CI/CD remains disabled.
