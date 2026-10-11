# Original one-cycle texture rectangles

Repository-owned MIT MIPS III cartridge and assembler; no SDK, retail bytes or
expected guest verdict. Real DP packets load a 4×2 RGBA16 texture and execute E4
and E5 through RDRAM or wrapped XBUS/DMEM. A real SP helper reads hidden metadata
through MI EBUS. The guest emits framebuffer bytes, hidden bits, two guards and
DP CURRENT/STATUS plus MI pending state.

The independent Python oracle grades **1,048,576 cases / 13,631,488 actual
64-bit words**: 65,536 raw seeds × RGBA16/RGBA32 × E4/E5 × point/three-point
filter × RDRAM/XBUS. Each seed generates eight texture words and coupled signed
S/T and derivative fields. Every raw 16-bit coordinate/derivative field occurs;
this is a selected coupled domain, not every joint combination. XBUS also enables
alpha comparison at threshold 127 and alternating field parity. The two sources
therefore cover different alpha/field profiles.

A 224-case boundary cartridge must pass. A separately executed cartridge corrupts
actual framebuffer captures and must fail every row. Thirteen individual field
mutations and seven malformed/truncated/trailing streams must be rejected.
Identity, case count, row width, completion line, terminator and EOF are strict.

```sh
python3 tests/roms-src/build.py n64/rdp_texture_rectangles --original-only --check
python3 tests/roms-src/n64/rdp_texture_rectangles/run.py
ctest --test-dir build -R '^n64_rdp_rectangle_(test|mutants|checkpoint_test)$' --output-on-failure
```

Artifacts: `build/artifacts/owned/n64_rdp_texture_rectangles/`.
The production plugin checkpoint gate checks serialized replay including hidden
metadata. Component tests and the optional pinned renderer comparison separately
cover pixel observations; complete graphics and playable retail remain pending.
