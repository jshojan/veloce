# Original N64 SI/PIF digital controller corpus

The repository-owned Python assembler emits a marked `V64HB001` homebrew ROM
without SDK/boot assets. The actual frontend loads the production shared plugin,
feeds all 16,384 combinations of 14 digital buttons through `INPUT_FILE`, and
records guest-observed SI/PIF results through the debug file channel. The ROM
has no expected button values or pass verdict. It yields once per sample through
emulator-only `$BFF0000C`; this is test instrumentation, not hardware timing.

Each case records eight 64-bit values: the two words containing port 1 digital
input and port 2's absent response; port 1 identification; SI completion status;
MI pending; MI mask; then SI status and MI pending after acknowledgement.
The independent Python oracle constructs wire bits from the host input schedule.
All **16,384/16,384 cases** (131,072 values) matched. Analog bytes are zero,
reserved button bits are clear, controller identity is `05 00 02`, and absent
ports leave receive bytes unchanged with the no-response flag set.

An executed read-to-identify command mutation fails all 16 negative cases.
Seven damaged files (missing/malformed row, wrong domain, false pass, missing or
unterminated trailer, trailing data) produce ERROR. Additional frontend checks
reject malformed, overflowing, negative, missing-file and conflicting schedules;
legacy `INPUT` masks still work. Raw observations and JSON reports are preserved
in `build/artifacts/owned/n64_controller/`, including the full input schedule.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_machine_test -j
ctest --test-dir build -R '^n64_(machine_test|controller_serial)$' --output-on-failure
```

`INPUT_FILE` is a maximum 8 MiB ASCII comma-separated `frame:hexmask` schedule,
with nonnegative decimal frames and 32-bit hex masks (optional `0x` prefix).
Do not combine it with `INPUT`. Values persist until the next event; duplicate
frames use the last value. Existing button indices remain unchanged; N64 Z and
C-Up/Down/Left/Right append indices 20–24.

This is exhaustive only for digital combinations on one standard controller.
Settled transfers complete immediately; DMA busy/overlap/error timing, CPU
interrupt delivery, analog sticks, accessories, four-port input routing,
PIF boot, retail games, graphics/audio and physical hardware remain pending.
Packet grammar and error behavior have bounded component tests, not an exhaustive
original-ROM domain. The source generator and archived image hashes are tracked
in `tests/roms-src/rom_manifest.json`. Sources are MIT licensed.
