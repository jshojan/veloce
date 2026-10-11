# ARM11 exclusive memory original ROM

`build.py` produces the repository-owned `exclusive_memory.v3ds` image from
three ARM programs. Both ARM11 contexts execute byte, halfword, word and
doubleword LDREX/STREX pairs, CLREX, peer writes and repeated stores. ARM9
acts as the third completion context. Guest code writes ten actual register
and RAM words per case through the 3DS debug result channel.

`run.py` independently computes every expected word from the case number,
width and scenario. The full domain contains **5,242,880 snapshots and
52,428,800 actual words**: two ARM11 contexts, four widths, five scenarios,
65,536 low operand values and two NZCV surrounds. It checks exact group,
row and trailer framing. The reduced original image grades 1,280 snapshots;
two executed image faults and 17 file faults must be rejected. The executed
faults change store status and omit CLREX. The latter must affect only the
CLREX scenario.

Run with:

```sh
python3 tests/roms-src/build.py 3ds/exclusive_memory
python3 tests/roms-src/3ds/exclusive_memory/run.py
```

This verifies the supplied original program and coherent shared-RAM profile.
Physical ARM11 cache/MMU configuration, cache maintenance, exception behavior,
timing and native 3DS software remain unverified. The private Super Mario 64
cartridge is unrelated to this image and is never included in the archive.
