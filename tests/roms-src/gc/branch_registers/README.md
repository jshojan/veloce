# Original GameCube conditional branch register corpus

Original MIT-licensed Python assembler, marked test image and independent host
oracle; no SDK, firmware, retail assets, embedded answers or guest verdicts.
The [supplied PowerPC profile](../../../../cores/ppc/README.md) executes every
branch and writes seven observed words through `ppcdebug` to a file.

## Verified domain

**74,752/74,752 seven-word snapshots matched**, comprising:

- 34,816 BC cases and 34,816 BCLR cases: seventeen canonical BO encodings ×
  all 32 BI fields × four CR seeds × eight CTR boundaries × both LK states.
- 5,120 BCCTR cases: five canonical BO encodings with no CTR decrement × all
  32 BI fields × four CR seeds × all four target low-bit positions × both LK.

BC uses a forward relative displacement of eight bytes. BCLR targets include
all four low-bit positions on the same boundary domain. The seven captured
words are taken-path marker, LR, CTR, CR, XER, an actual guest PC anchor and an
untouched GPR sentinel. The host independently classifies BO conditions and
checks decrement-before-test, modulo carry-around, bit indexing, all captured
preserved bits and link updates on taken/untaken paths. It finds PC-observation
instructions in the actual image to establish addresses; no builder metadata
or expected-answer table is consumed.

Component tests separately grade 4,227,072 branch snapshots across both
profiles, including every BD field with AA/LK states and wrapping targets,
and all canonical BO/BI, CR/CTR boundaries and indirect low-bit alignments.
They check 256,000 unsupported/invalid XL forms without claiming hardware
exceptions, ignored BO z-bit compatibility separately, and exact replay of
an original 100,001-iteration counted loop across plugin frame boundaries.

## Harness controls

A reduced 256-case original ROM must pass. Executed result corruption must
fail 256 cases; reversed CR conditions must fail 144, and stripped LK bits
must fail 128. Every one of seven captured field mutations must fail exactly
one case. Seven missing/malformed/truncated/false-verdict streams must return
ERROR. Raw `.result` files and JSON reports remain in
`build/artifacts/owned/gc_branches/`. The image and generator hashes are
tracked in the original-ROM reproducibility ledger.

This is a finite input domain and instruction-level evidence. Noncanonical BO
fields, newer BH forms, physical branch prediction/timing, Gekko/Broadway
quirks, retail startup and other console hardware remain unverified. Direct
negative/absolute displacements are covered by components, not this ROM.

```sh
python3 tests/roms-src/build.py gc/branch_registers --check
python3 tests/roms-src/gc/branch_registers/run.py --emulator build/bin/veloce
```
