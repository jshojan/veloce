# Original AArch64 paired-transfer register corpus

Original MIT source and uncompressed NSO0, with no firmware, SDK, keys,
retail data or external emulator source. The assembler reuses only original
`ccmp_registers/build.py` and `arithmetic_registers/build.py`.

## Implemented behavior

Arm's published instruction definitions describe
[LDP](https://www.scs.stanford.edu/~zyedidia/arm64/ldp_gen.html),
[STP](https://www.scs.stanford.edu/~zyedidia/arm64/stp_gen.html),
[LDPSW](https://www.scs.stanford.edu/~zyedidia/arm64/ldpsw.html),
[LDNP](https://www.scs.stanford.edu/~zyedidia/arm64/ldnp_gen.html) and
[STNP](https://www.scs.stanford.edu/~zyedidia/arm64/stnp_gen.html).

- LDP/STP transfer two 32- or 64-bit integer words with signed offset,
  pre-index or post-index addressing. LDNP/STNP supply the no-writeback
  non-temporal form; cache residency/timing is not modeled.
- LDPSW transfers two 32-bit words and sign-extends each to 64 bits, with
  signed offset/pre-index/post-index forms. There is no non-temporal LDPSW.
- All 128 immediate fields are signed, scaled by 4 or 8. Post-index accesses
  the old base; pre-index accesses the updated base. Writeback occurs only
  after both successful accesses. W loads zero the upper half; W stores use
  the low half. Register 31 is SP for the base and zero/discard for transfers.
- NZCV is preserved. Sources are captured before writes. No-writeback base/
  destination aliases are supported; duplicate store sources are supported.
- Reserved integer sizes and SIMD/FP pairs reject. Writeback overlap and
  duplicate load destinations stop diagnostically, choosing an allowed
  constrained-unpredictable outcome, without claiming hardware exception entry.

The supplied process profile checks 16-byte SP alignment (diagnostic 6), allows
unaligned general-base accesses, and uses existing little-endian permissions.
Full SCTLR/EL/MMU/alignment controls are unfinished. Pair accesses use two
ordered bus calls. A failed second store leaves its successful first store in
RAM; failed loads leave both destinations unchanged. Base/PC stay unchanged on
fault. This is an explicit functional fault convention, not precise data-abort,
pair atomicity, cache ordering, tag checking or measured hardware behavior.

## Actual original-guest domain

The original image independently grades **2,490,368 snapshots / 24,903,680 actual
64-bit captured words**:

- 19 variants: four address classes for 32/64-bit load/store, plus three LDPSW.
- All 128 encoded displacements, including both endpoints and zero.
- X8 and SP bases; clear/all-set NZCV surrounds.
- All 256 repeated-byte seeds, paired with their bitwise complements.

Rows contain actual result/source registers X0/X1, post-operation base, both
memory words read back through ordinary scalar LDRs, MRS NZCV, unchanged input
seed/complement, requested encoding, and initial base. Loads use scalar STRs
to seed RAM; stores start from reversed memory values, so a skipped store cannot
pass. Every test resets the base and flags; captures execute after the pair.
The image has no expected answers or guest correctness verdict. Synthetic
`vnxdebug` STR ports transport actual register values to a file.

This finite domain covers addressing and all repeated-byte transfers, not every
independent pair of 64-bit values, register encoding, memory mapping or CPU
configuration. Native firmware/HOS/retail boot, processes, GPU/audio and native
controllers remain unimplemented. Switch stays **25%** of the delivery checklist.

## Independent oracle and controls

```sh
ctest --test-dir build -R '^switch_pair_transfer_' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

The host imports only the builder. It independently enumerates encodings and
addresses, constructs input bytes, truncates through byte selection and applies
signed conversion. It grades all ten fields and requires the exact ROM CRC,
channel, domain, completion, ungraded trailer and EOF. Emulator/core, CPU/bus,
builder/oracle, original assembler, recipe and archive hashes must remain fixed.

The short **1,520-case** image tests four byte values and five encoded offsets
for every variant/base/flag combination. Executed result/flag/offset mutations
reject every row; replacing LDPSW with unsigned loads rejects all **240 signed
load rows**; skipped stores reject all **640 store rows**. Ten single-field faults
and seven malformed/incomplete/false-completion streams reject.

The archive reproduces: **1,382,904 bytes**, SHA256
`88a10258c0419d8efc592e7603ad9877f5016e12058406744574ab5969fdd115`.

The component checks **2,294,067 observations**: all 128 offsets × repeated-byte
seeds × variants × general/SP bases; all valid register fields/aliases;
unaligned general-base boundaries; exhaustive selected reserved/overlap fields;
ordered first/second read/write faults and SP alignment. Expected memory is built
byte by byte and compared including surrounding untouched bytes. The prior
interpreter fails the actual new paired-load result gate.

Production-plugin replay compares actual output and complete **4,194,648-byte**
states after checkpoint/restore. The machine gate additionally verifies SP
alignment diagnostics round-trip, invalid diagnostic 7 rejects atomically,
and partial-store/read-fault states replay exactly. State layout remains
VNXS0001; readers predating diagnostic 6 cannot restore that new terminal state.
CI/CD remains disabled; no private ROM assets are included.

Executed integration: the full Release build and all **25 Switch gates** pass,
including the prior original instruction corpora. The new 2,490,368-row file
has zero mismatches and unchanged capture provenance. All **146 original
archives** reproduce; completion rendering and whitespace checks pass.
