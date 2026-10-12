# Original N64 two-cycle fog observations

This MIT-owned cartridge issues real two-cycle RDP triangle commands. It writes
framebuffer pixels and DP/MI register values to a hexadecimal result file;
no expected output bytes, SDK, commercial ROM or microcode are embedded.

The full domain contains **262,144 cases × 13 guest 64-bit words = 3,407,872
observations**. Every 8-bit primitive RGB seed and primitive alpha pair runs
through two second-cycle blend profiles and both RDRAM and XBUS command paths.
Cycle zero forwards primitive color. Cycle one consumes COMBINED; the first
blender stage fogs with shade alpha, then the second stage blends with either
framebuffer memory or blend color. Thirteen words capture eight framebuffer
pairs, two guards, DP CURRENT/STATUS and MI interrupt state. The host oracle
independently calculates the triangle's quarter-pixel coverage, combiner
forwarding, two blend stages and register values.

The short 40-case control, separately executed wrong-capture cartridge,
13 field flips and seven malformed/domain/trailer controls check the grader.
This covers the stated bounded profile; other two-cycle sources, texture LOD,
physical console timing and full retail behavior remain unverified.

```sh
python3 tests/roms-src/build.py n64/rdp_two_cycle_fog --original-only --check
python3 tests/roms-src/n64/rdp_two_cycle_fog/run.py
ctest --test-dir build -R '^n64_rdp_two_cycle_fog$' --output-on-failure
```

Output files are under `build/artifacts/owned/n64_rdp_two_cycle_fog/`.
