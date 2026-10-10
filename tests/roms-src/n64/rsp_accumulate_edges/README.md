# Original N64 rsp_accumulate_edges corpus

MIT licensed original assembler/program; no SDK, firmware or commercial bytes.
Nine accumulate/round functions × 8,192 packed groups of eight raw halfwords × four partner and accumulator-edge patterns. All raw halfwords occur; selectors cycle with group/pattern and VS parity varies for round. This does not exhaust every operand at every selector.

**294,912 cases / 7,372,800 captured 64-bit words.** The CPU runs actual RSP
instructions and records destination, all accumulator slices, sources, flags,
SP PC/status and MI. The edge suite also captures the seeded accumulator and
scalar zero. Guest code computes no verdict. Independent unbounded integer
arithmetic grades every field. Records use ordered chunks of at most twelve
words. An executed capture mutant must fail every reduced case; all 25
fields have individual mutation checks and seven malformed streams must ERROR.
Artifacts: `build/artifacts/owned/n64_rsp_accumulate_edges/`. Reproducible image and generator
hashes are committed in the ROM ledger.

```bash
python3 tests/roms-src/build.py n64/rsp_accumulate_edges --original-only --check
ctest --test-dir build -R '^n64_rsp_' --output-on-failure
```

Rules and published numeric hardware observations are linked from the
[N64 arithmetic evidence](../../../../cores/n64/README.md#rsp-vector-multiply-and-accumulate).
These cartridges have not run on a physical console. Full-width operand pairs,
physical timing/interlocks, remaining vector arithmetic, reciprocal/flags,
complete RDP/audio and retail boot/playability stay pending. N64 remains 40%
on the fixed delivery checklist.
