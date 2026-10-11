# Original N64 rsp_multiply_pairs corpus

MIT licensed original assembler/program; no SDK, firmware or commercial bytes.
Seven load-multiply functions × all 16 element selectors × 256 repeated-byte VS halfwords × 32 packed groups of eight repeated-byte VT halfwords. Identity selection covers every byte pair; broadcasts cover the selected packed lanes, not every pair at every selector.

**917,504 cases / 16,515,072 captured 64-bit words.** The CPU runs actual RSP
instructions and records destination, all accumulator slices, sources, flags,
SP PC/status and MI. The edge suite also captures the seeded accumulator and
scalar zero. Guest code computes no verdict. Independent unbounded integer
arithmetic grades every field. Records use ordered chunks of at most twelve
words. An executed capture mutant must fail every reduced case; all 18
fields have individual mutation checks and seven malformed streams must ERROR.
Artifacts: `build/artifacts/owned/n64_rsp_multiply_pairs/`. Reproducible image and generator
hashes are committed in the ROM ledger.

```bash
python3 tests/roms-src/build.py n64/rsp_multiply_pairs --original-only --check
ctest --test-dir build -R '^n64_rsp_' --output-on-failure
```

Rules and published numeric hardware observations are linked from the
[N64 arithmetic evidence](../../../../cores/n64/README.md#rsp-vector-multiply-and-accumulate).
These cartridges have not run on a physical console. Full-width operand pairs,
physical timing/interlocks, remaining vector arithmetic, reciprocal/flags,
complete RDP/audio and retail boot/playability stay pending. The historical increment credited 40%; the current fixed-checklist score is
45%, as summarized below.

## Corrected mixed-product grading

VMUDM/VMADM use signed VS × unsigned VT; VMUDN/VMADN use unsigned VS × signed
VT. The previous oracle reversed these roles. The corrected full archived
cartridge emits the same observations and is regraded with the corrected rule.
Reduced controls now include both operand signs (1,792 cases). All four
compiled signedness faults are rejected by executed component observations.
The [optional pinned comparison](../../../reference/n64-rsp-mixed/README.md)
checks register and 48-bit accumulator output independently of our arithmetic.

N64 remains **45% = 9/20 delivery milestones**. The private Mario probe now
shows a recognizable logo; gameplay and full hardware accuracy remain unverified.
