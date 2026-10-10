# Original N64 FPU data-transfer corpus

This MIT licensed assembler emits an original cartridge without SDK, firmware or
commercial bytes. The guest executes MFC1/MTC1/DMFC1/DMTC1 and
LWC1/LDC1/SWC1/SDC1, then writes nine actual 64-bit captures per case to the debug
register stream. Expected values and grading live only in the host runner.

**131,072 cases / 1,179,648 values** match: every repeated 16-bit word payload in
both FR modes, with complementary high/low doubleword halves, signed word
readbacks, real RAM loads/stores and odd/even word pairing. Doubleword operations
use an even FPR in FR=0; FR=1 uses independent 64-bit registers. The ROM exercises
FPR 2/3; the component covers all 32 indices. This is not the full independent
32-/64-bit payload product, nor a complete FPU corpus.

An executed capture mutation fails all 512 reduced cases; nine field mutations
each fail a row, and seven malformed/incomplete streams are rejected as ERROR.
Strict identity, domain, row, completion and trailer checks prevent partial or
falsely framed output from passing. Reports and raw actual values are under
`build/artifacts/owned/n64_fpu_data/`; image and generator hashes are in the
committed original-ROM ledger.

The component checks **4,194,304 word register/payload cases**, **3,145,728 defined
doubleword register/payload cases**, all **262,144 signed-immediate/direction/width
cases**, every GPR including zero, every 64-bit data bit position on all FR=1
registers, all 2,047 nonzero reserved transfer field encodings, CU1 precedence,
32-/64-bit user/supervisor restrictions on doubleword moves, alignment/TLB/bus
faults, delay/nested/BEV context, reset and exact FPR state continuation.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_fpu_data_test -j
ctest --test-dir build -R '^n64_(fpu_data|fpu_data_test)$' --output-on-failure
```

Rules follow the NEC [VR4300 manual](https://hack64.net/docs/VR43XX.pdf),
sections 7.2 and 17.3, and the eight instruction entries. FR=1 word writes leave
upper bits undefined architecturally; this emulator preserves them as a convention.
Odd FR=0 doubleword forms are undefined and deterministically raise RI here.
Raw storage is retained across FR view changes as an emulator convention; physical
mode-transition behavior has not been graded. Transfer pipeline hazards and
physical timing remain unverified. FPR arithmetic/conversion/comparison and
reverse-endian transfers remain pending. No physical console has validated this
cartridge. N64 remains 40% on the delivery checklist; the CPU milestone is pending.
