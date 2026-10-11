# Original cached SI analog controller observations

Original MIT MIPS cartridge, assembler and independent Python integer oracle.
The guest formats one four-controller packet once, then reads SI DMA twice per
case with a real frontend input transition between reads. The guest emits actual
RDRAM packet contents, SI_STATUS and MI_INTR; it computes no expected verdict.

**65,536 cases / 1,048,576 captured 64-bit words** cover every X/Y byte pair
(-128..127 on both axes), followed by both byte complements. Digital buttons vary
with the low fourteen case bits, with every combination covered four times.
This is not the Cartesian product of all digital masks and all axis pairs.
Port zero is active; three neutral ports, packet/control guards and both IRQ
acknowledgements are graded in each case. Component observations additionally
cover every axis pair independently on each of four ports, with two cached reads:
**524,288 analog observations**, alongside all existing digital/command tests.

The input word is independently assembled in the host oracle as compact digital
bits 0..13, tag `11` in bits 14..15, X byte in bits 16..23 and Y in bits 24..31.
Untagged valid N64 VirtualButton masks still mean a neutral stick. UI bits 14/15
are not N64 buttons and must not be supplied in legacy N64 input words.
The test uses two strict chunks (7 and 9 words) per case to fit the output sink.

The 16-case control passes; an executed wrong-capture cartridge fails all 16
cases; sixteen single-field faults fail individually; seven malformed streams
are errors. Production mutations additionally reject neutral-X, swapped axes,
Y-port aliasing, and retained-layout/input faults. Neither ROM nor oracle imports
production emulator arithmetic or embeds copyrighted firmware or retail content.

```sh
python3 tests/roms-src/build.py n64/controller_analog_reads --original-only --check
python3 tests/roms-src/n64/controller_analog_reads/run.py
ctest --test-dir build -R '^n64_(controller_analog_reads|pif_.*)$' --output-on-failure
```

Artifacts: `build/artifacts/owned/n64_controller_analog_reads/`.
The controller-read structure in [libdragon's Joybus documentation](https://libdragon.dev/ref/joybus__commands_8h_source.html)
corroborates the signed eight-bit X/Y fields. The original corpus grades wire
bytes, not optical counters, physical stick calibration, accessory/reset-origin
behavior, detached devices, EEPROM/CIC, serial timing or retail playability.
