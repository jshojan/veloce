# Original N64 rsp scalar immediates corpus

This MIT licensed assembler generates original CPU and RSP programs, with no SDK,
firmware or commercial bytes. The CPU loads IMEM, supplies operands in DMEM,
starts the RSP and waits for real BREAK. The RSP writes actual GPRs to DMEM; CPU
code streams those observations through the emulator debug register. Neither
guest program computes a verdict.

**1,572,864 rows / 11,010,048 captured 32-bit words** cover:

All 65,536 immediates for ADDI/ADDIU/SLTI/SLTIU/ANDI/ORI/XORI/LUI on zero, 0x7fffffff and 0x80000000 source patterns. SLTIU sign-extends its immediate before unsigned comparison.

Each row captures destination, both unchanged sources, zero register, SP PC,
SP status and MI pending. An independent mathematical Python oracle grades all
seven words. An executed capture mutant fails all 192 reduced cases;
seven field mutations fail individually; seven damaged/incomplete streams return
ERROR. Raw observations and JSON reports live in
`build/artifacts/owned/n64_rsp_scalar_immediates/`. Image/generator hashes are in the ledger.

The component adds all 16-bit immediates on eight patterns, every scalar ALU
register triple, every DMEM byte address on 256 byte patterns for nine load/store
forms, all PC slots for selected branch/BREAK/links, shared SP controls, semaphore
and DMA, CPU interrupt acknowledgment and S014 state replay/rejection/legacy conversion.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_rsp_scalar_test -j
ctest --test-dir build -R '^n64_rsp_scalar' --output-on-failure
```

Rules follow the SGI [RSP programmer guide](https://bukosek.si/hardware/collection/sgi-o2/n64-rsp-programmers-guide.pdf)
and the hardware-test author's [scalar cases](https://github.com/lemmy-64/n64-systemtest/tree/main/src/tests/rsp).
No physical console has validated these original cartridges. The declared finite
domains do not exhaust independent full-width operands. Scheduling uses two RSP
steps per three nominal CPU attempts; physical pipeline/dual issue/interlocks,
DMA bus timing, debugger timing and undefined encodings remain ungraded. Vector
arithmetic, complete RDP and game graphics/audio remain pending. Shared DP controls and a bounded fill/sync profile are now implemented; see the [current core evidence](../../../../cores/n64/README.md#display-processor-interface-and-fill-engine).
Unsupported operations block explicitly without a fake BREAK/completion IRQ.
N64 stays 40% on the delivery checklist.
