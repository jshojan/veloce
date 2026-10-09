# Game Boy register probe

This is original Veloce test code, licensed under the repository's MIT license.
`build.py` emits the checked-in 32 KiB `register_probe.gb` using SM83 opcodes;
Python 3 is the only build dependency. Rebuild with:

```bash
python3 tests/roms-src/gb/register_probe/build.py
```

The ROM checks two independent observations: a byte written to and read from
WRAM `$C000`, and the result of `$0F + $01`. It sends the observed values via
`SB` (`$FF01`) and `SC` (`$FF02`) using the Game Boy's internal serial clock.
The plugin forwards each `SC=$81` transmit request to the application-owned result file.
`REG` lines contain the observations; `CHECK` and `END` lines carry verdicts.

```bash
python3 tests/roms-src/gb/register_probe/test_probe.py build/bin/veloce
```

The runtime test compares the ROM to a fresh source build and requires PASS.
It then changes the ROM's WRAM stimulus from `$5A` to `$59` and requires FAIL
with `REG wram=59`, proving that the output follows emulated hardware state.
It does not exercise other CPU instructions, interrupt timing, PPU, APU, MBC,
or CGB behavior.
