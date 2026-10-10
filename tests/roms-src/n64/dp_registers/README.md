# Original N64 dp_registers corpus

This MIT licensed generator assembles original programs without SDK, firmware or commercial bytes. The CPU loads an original RSP program, freezes the DP, and supplies four address patterns for every 16-bit value. The RSP executes actual MTC0/MFC0 and stores START/CURRENT/STATUS observations in DMEM, including ignored repeated START and frozen END activation. CPU code captures the same shared registers.

**262,144 rows / 2,621,440 captured 64-bit words** match an independent Python integer oracle. The guest emits observations only. The executed capture mutant fails all 32 reduced cases; each of 10 field mutations fails individually; seven malformed/incomplete streams return ERROR. Results live in `build/artifacts/owned/n64_dp_registers/`; image/generator hashes are recorded in the ROM ledger.

```bash
python3 tests/roms-src/build.py --original-only --check
ctest --test-dir build -R '^n64_dp' --output-on-failure
```

Rules follow the SGI [RSP DP register guide](https://hcs64.com/files/RSPCOP0.pdf), [RDP command summary](https://ultra64.ca/files/documentation/silicon-graphics/SGI_RDP_Command_Summary.pdf), and the hardware-test author's [DP latch tests](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rdp/mod.rs). These original cartridges have not been run on a physical console. Counters and command issue are nominal clocks; FIFO/prefetch/physical timing, simultaneous clear/set behavior, read-only bus writes, hidden coverage bits, fractional rectangles, interlace scissor, textures, triangles, blending and depth remain ungraded or unsupported. The fill engine accepts only the documented integer RGBA16/32 fill-cycle subset. Unsupported commands retain CURRENT and record diagnostics without fabricated completion IRQ. N64 remains 40% on the delivery checklist.
