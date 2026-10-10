# Original N64 AI FIFO register corpus

This MIT licensed assembler emits an original marked cartridge with no SDK,
firmware or commercial bytes. Guest instructions write the audio-interface
registers and stream actual register observations into a host-owned file. There
are no expected results or pass verdicts in the cartridge.

All **2,048/2,048 cases** match **22,528 guest values**: raw lengths 0–255 ×
DMA enable clear/set × MI mask clear/set × a second buffer absent/present.
Each row captures length, selected status fields, MI source, all four write-only
length mirrors and CPU Cause. Reserved high bits are supplied to length/control
writes. COUNT/WC/BC and analog/serial timing are unscored. DMA is frozen while
printing; the prior FIFO drains before the next case. This domain is not the
entire independent length/address/rate/queue product. The component separately
checks every 18-bit length and aligned 24-bit address field.

The independent host oracle uses arithmetic rather than CPU or assembler field
helpers. An executed length mutation fails all 128 reduced cases. Eleven field
mutations each fail one row; seven malformed streams produce ERROR. Identity,
domain, count, widths, completion and trailer validation reject incomplete output.
Raw files/reports are under `build/artifacts/owned/n64_ai_fifo/`; source/image
hashes are in the ROM manifest.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_ai_test -j
ctest --test-dir build -R '^n64_(ai_fifo|ai_test)$' --output-on-failure
```

The functional model follows [AI documentation](https://n64brew.dev/wiki/Audio_Interface?action=raw)
and [libdragon register definitions](https://libdragon.dev/ref/group__lowlevel).
Physical IRQ/FIFO latency, status clock fields, serial-bit corruption, analog
behavior, dynamic rate phases, full RSP audio and retail compatibility remain
unverified. The original program tests the declared profile; it has not been
run on a physical console.
