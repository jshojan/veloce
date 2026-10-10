# Original N64 rsp_vector_transfers corpus

MIT licensed original assembler and program; no SDK, firmware or commercial bytes.
Every 16-bit payload at every byte element of vector 5, with an upper-word sentinel. An actual RSP program executes MTC2/MFC2 (including scalar zero), CTC2/CFC2 for VCO/VCC/VCE and stores vector/flag/scalar observations. All observations are captured by CPU code.

**1,048,576 cases / 10,485,760 captured 64-bit words** are graded by an independent integer byte-mapping oracle. Guest code emits observations without computing a verdict. An executed capture mutant must fail every reduced case; all 10 captured fields have individual mutation checks, and seven malformed/incomplete streams must return ERROR. Raw streams and JSON reports live in `build/artifacts/owned/n64_rsp_vector_transfers/`; the ledger records image/generator hashes.

```bash
python3 tests/roms-src/build.py --original-only --check
ctest --test-dir build -R '^n64_rsp_vector' --output-on-failure
```

Rules follow the SGI [RSP programmer guide](https://bukosek.si/hardware/collection/sgi-o2/n64-rsp-programmers-guide.pdf), and the hardware-test author's [loads](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vector_loads.rs), [stores](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vector_stores.rs), [scalar transfers](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_mfc2_mtc2.rs) and [flag aliases](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_cfc2_ctc2.rs).

These original ROMs have not run on a physical console. This is a finite functional profile, not exhaustive independent bytes/operands or physical timing. LFV element 1's first packed low bit follows the hardware-test author's published mapping; the published sequential-byte test cannot distinguish competing mappings there, so non-linear-input physical behavior receives no accuracy credit. Physical load interlocks/dual issue, vector arithmetic, accumulators, reciprocal state, complete graphics/audio and retail playability remain pending. Unknown arithmetic blocks at the actual PC without a fabricated BREAK/IRQ. N64 remains 40% on the delivery checklist.
