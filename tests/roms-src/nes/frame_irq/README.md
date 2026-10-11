# Original NTSC frame IRQ register captures

This original 32 KiB NROM contains no pass verdict or expected status values.
It reads `$4015` into actual A registers, stores the two observed bytes in
zero-page RAM, and prints those bytes through `$401E`. `$401F=1` finishes with
unknown status; the host independently grades the observations.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce nes_plugin
ctest --test-dir build -R '^nes_frame_irq$' --output-on-failure
```

Current result: **80 cases / 160 actual status reads**, zero mismatches; the
capture repeats byte for byte. Each mode/inhibit combination and both CPU
write parities run ten selected delays around the first and second IRQ
windows. Two reads are separated by seven CPU cycles; the first acknowledges
the flag, so the second detects reassertion inside the three-cycle window.
The host computes event times with integers, independently of emulator source.

References: [NESdev frame counter](https://www.nesdev.org/wiki/APU_Frame_Counter)
and [the documented three-cycle reassertion behavior](https://forums.nesdev.org/viewtopic.php?t=24422).
The NTSC sequence period is 29,830 CPU cycles; IRQ assertions occupy sequence
cycles 29,828–29,830. Writes reset after three/four cycles according to parity.
The ROM instruction timing and current core's four startup bus reads establish
its write parity. Physical power-on phase, PAL/Dendy timing, all possible write
positions, length/envelope/sweep/audio/DMC domains are outside this bounded
profile and remain unfinished. This is not a whole-APU accuracy percentage.

The runner executes a separate original mutant that flips a captured status
bit with EOR; the independent oracle must fail it. Missing rows are measurement
errors; changed capture values are failures. Reports, raw captures, mutant
images and control results remain under `build/artifacts/owned/nes_frame_irq/`.
The archived `frame_irq.nes` is Veloce-authored MIT source output; no commercial
ROM bytes or foreign emulator code are included.
