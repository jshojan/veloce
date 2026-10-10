# Original N64 FPU control-register corpus

This MIT licensed assembler emits an original cartridge without SDK, firmware,
or commercial bytes. Guest CTC1/CFC1 instructions and a guest exception handler
capture actual control values, CPU exception codes, and all four BC1 branch
outcomes. They stream eight register values per case through the debug port into
a file. Expected values and grading exist only in the host runner.

**8,192/8,192 cases** match **65,536 register values**: every combination of six
cause bits, five enables, and four rounding modes. Flags, condition, flush mode
and reserved high bits are varied across that domain. The handler records FCR31
before clearing cause bits and returning through ERET. Each row also checks the
read-only FCR0 profile and ordinary/likely true/false branch delay-slot effects.
All architecturally required gaps are included; pipeline hazards are not scored.

The independent arithmetic oracle does not import emulator logic or assembler
field helpers. An executed rounding-mode mutation fails all 128 reduced rows.
Eight field mutations each fail one row; seven malformed streams produce ERROR.
Identity, count, widths, completion and trailer validation reject partial output.
Raw output and reports live in `build/artifacts/owned/n64_fpu_controls/`.
The image and generator hashes are recorded in `rom_manifest.json`.

The separate component harness checks all **1,048,576** defined combinations of
cause, enable, flags, rounding mode, condition and flush state; all **524,288**
branch offset/condition/kind combinations; all **1,024** disabled-CU1
format/control-index combinations; all GPR transfer destinations/sources,
read-only FCR0, delay-slot/BEV/nested exception context, reset, atomic rejection
of invalid state scalars, and legacy S009 conversion. This is exhaustive for
those declared finite fields, not the complete floating-point hardware corpus.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_fpu_controls_test -j
ctest --test-dir build -R '^n64_fpu_controls' --output-on-failure
```

Behavior follows the original NEC [VR4300 user manual](https://hack64.net/docs/VR43XX.pdf),
sections 7.2.3–7.2.5 and instruction entries BC1F/BC1FL/BC1T/BC1TL/CFC1/CTC1.
FCR0 revision zero is the selected profile. Reset-to-zero FCR31 is a functional
startup convention. Data registers/transfers, arithmetic, conversions, compares,
NaN/denormal behavior, physical timing and pipeline hazards remain pending.
Undefined control indices/encodings and remaining COP1 instructions currently
raise RI as an emulator limitation; this does not claim physical behavior for
undefined encodings. This cartridge has not been validated on a physical N64.
