# Original copy-cycle texture rectangle observations

Original MIT MIPS guest, assembler, cartridge and independent Python oracle.
No SDK, commercial bytes or guest-computed expected verdicts are included.

The guest writes an RGBA16 texture, transfers it through real DP LoadTile, then
executes E4/E5 copy rectangles. It captures **13 actual 64-bit words per case**:
four framebuffer words, four MI EBUS hidden-RAM words, two adjacent guards and
DP_CURRENT, DP_STATUS and MI_INTR. The host grades strict packed observations.

Full domain: **1,572,864 cases / 20,447,232 captured words** = all 65,536 coupled
seeds × 4/8/16-bit framebuffer × normal/flipped coordinates × opaque/alpha
comparison × RDRAM/wrapped-DMEM command source. The seed independently spans
raw 16-bit S, T, dS and dT through bijective partners; it does not enumerate their
whole Cartesian product. Wrapped DMEM starts at 0xff0 and transfers 88 bytes.
That source also enables alternate-row field selection, with seed-selected parity.
Texture masks wrap at four columns/two rows. Ignored combiner/blender/depth/dither
fields are deliberately populated. CI8 threshold is 127; random thresholds,
32-bit framebuffer crash behavior, general TMEM patterns and physical timing
are excluded. Additional shapes/banks/LOD are checked by component/reference tests.

The reduced **336-case** corpus covers boundary seeds. A separately executed
wrong-capture cartridge must fail every row, all **13 field mutations** must
fail their changed row, and **seven malformed streams** must be errors. ROM
identity, domain, trailer and termination are mandatory; missing output cannot pass.

```sh
python3 tests/roms-src/build.py n64/rdp_copy_rectangles --original-only --check
python3 tests/roms-src/n64/rdp_copy_rectangles/run.py
ctest --test-dir build -R '^n64_rdp_copy_(test|mutants|checkpoint_test)$' --output-on-failure
```

Artifacts: `build/artifacts/owned/n64_rdp_copy_rectangles/`. The checkpoint gate
restores production S028 state and compares actual guest output/full state after
replay. No external software reference is needed by this guest suite.
