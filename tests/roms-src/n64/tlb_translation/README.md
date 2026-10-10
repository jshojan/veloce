# Original N64 TLB translation corpus

This MIT licensed assembler emits a repository-owned cartridge without SDK,
firmware or commercial bytes. It installs a real paired-page translation,
executes TLBWI/TLBP/TLBR and guest loads/stores, and captures actual CPU/CP0
registers. Guest exception handlers recover through ERET. Eleven words per case
are streamed to a file; expected values and grading live only in the host runner.

**114,688/114,688 cases** match **1,261,568 register values**: seven page sizes ×
256 stored ASIDs × 32 patterns × two page halves. Patterns cover valid/dirty,
each independent global bit and matching/mismatching query ASIDs. Each row
captures loaded value, load/store exception codes and BadVAddr, physical backing
value after the store, probe outcome, both EntryLo values, PageMask and EntryHi.
Index bit 5 is supplied and ignored for the selected entry. Both global bits
must be set to ignore ASID; TLBR exposes their combined value in both halves.
The 16 MiB case intentionally uses the same aligned physical base for both
halves because the board has 8 MiB of RAM. Physical half selection across the
full frame domain is checked separately by the component.

The independent arithmetic oracle imports the builder only to emit cartridges.
An executed ASID mutation fails all 256 reduced cases. Eleven field mutations
each fail one row; seven malformed/incomplete streams are rejected as ERROR.
Identity, dimensions, completion, trailer and trailing-byte checks reject partial
or falsely framed output. Raw output/reports live in
`build/artifacts/owned/n64_tlb_translation/`; image and generator hashes are in
the committed original-ROM ledger.

The component separately checks **3,670,016** ASID/global/page-half combinations,
**2,097,152** physical-frame selections (all 20-bit PFNs on both halves),
**79,188** page offsets/boundaries, **229,376** independent even/odd
permission/global/CCA cases, and all **4,096** PageMask field encodings.
Only seven masks have defined hardware behavior. It also checks 32/64-bit
segments, region/privilege holes, xkphys and cache attributes, ERL bypass,
all indexed bit-5 aliases, Wired/Random replacement ranges, CP0 usability,
DMTC0/DMFC0 including 32-bit user/supervisor restrictions, CPU load/store/fetch faults, Context/XContext, nested/delay/BEV
vectors, mapped instruction execution, noncoherent cache aliases and save states.
This covers the declared finite domains, not their full independent product or
the full VR4300 hardware corpus.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_tlb_test -j
ctest --test-dir build -R '^n64_(tlb_translation|tlb_test)$' --output-on-failure
```

Rules follow the original NEC [VR4300 manual](https://hack64.net/docs/VR43XX.pdf),
chapters 5–6 and TLBR/TLBWI/TLBWR/TLBP entries. Reset installs unique invalid
entries as a functional convention; hardware power-on tags are unspecified.
Random decrements once per interpreter attempt; pipeline stalls/cycle timing are
unverified. Duplicate hits/shutdown/TS behavior remains unsupported; duplicate
translation hits raise RI and duplicate probes report failure as deterministic
emulator conventions. Undefined page masks are rejected before TLB mutation.
The N64 bus profile rejects unsupported physical accesses with IBE/DBE; this
is not a claim about every physical board/open-bus response. Pipeline CP0/TLB
hazards are unscored and gaps are inserted in the ROM. No physical-console run
has validated this cartridge yet.
