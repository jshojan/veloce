# Original DSi dual-CPU RAM register corpus

`ram_registers.vdsi` contains original ARM9/ARM7 programs with stimuli,
synchronization, loads/stores and register capture. It contains no expected
answers or guest verdict. The production DSi plugin executes through the
headless frontend, and the independent host grades the complete file.
Full result: **387,072/387,072 fourteen-word observations matched**.

## Finite domain

3 memory-limit modes (0,1,2) × 4 banks × 3 store aliases × 3 read aliases ×
2 writer CPUs × 7 width/lane pairs × 256 data patterns = 387,072 rows.
Bank targets are `02010000 + bank*00400000`; aliases are `02`, `0c` and `0d`.
Width/lane pairs are byte lanes 0–3, halfword lanes 0/2 and word lane 0.
The byte/complement pattern is `[b,255-b,b,255-b]` in increasing address order,
for every b=0..255. Each case starts in mode 2 and initializes four independent
bank words to input seeds `13579bdf,2468ace0,55aa00ff,a55aff00`, then selects
the tested mode and performs a store by either ARM9 or ARM7. Both CPUs read
all four banks through the selected read alias. ARM9 waits for ARM7's response
before writing the row; ordering is protocol evidence, not physical timing.

Fourteen captured words are the four ARM9 bank reads, four ARM7 bank reads,
store address, source pattern, width, writer selector (0=ARM9/1=ARM7), requested
SCFG value and case ordinal. Mode 0/1 maps canonical banks to the first 4 MiB,
with zero/ignored-store extended aliases; mode 2 exposes distinct banks and
all mirrors. The host separately enumerates cases and applies byte-array
replacement and mapping arithmetic. It imports no emulator code or builder
stimulus iterator. Builder import is for reproduction/executed controls.

The reduced image uses b=0..3, totaling 6,048 rows. Executed collapsed-bank,
byte-instead-of-wider-store and forced-16-MiB-mode mutants fail exactly
**1,512/1,038/4,032** rows. Fourteen capture-field mutations each fail one row.
Seven invalid/malformed/truncated/wrong-domain/false-verdict streams return
ERROR. The parser requires exact core/CRC/channel identity, suite/domain,
row width/count, completion markers, ungraded status=-1 trailer and EOF.
Raw output (~44 MiB) and JSON reports stay under `build/artifacts/owned/dsi_ram/`.

```sh
python3 tests/roms-src/build.py dsi/ram_registers
python3 tests/roms-src/dsi/ram_registers/run.py
python3 tests/roms-src/dsi/ram_registers/run.py --controls-only
```

The corpus covers settled mapping at four bank locations on its full declared
data/lane/mode/CPU/alias domain. It does not exhaust all addresses, 32-bit data,
CPU instruction encodings, physical clocks, cache/bus effects or exceptions.
The separate component gate covers every supplied RAM byte with one chosen
pattern. Mode 3 open bus, actual chip-control locations, full SCFG/MBK and
most DSi hardware remain pending; see [core limits](../../../../cores/dsi/README.md).
Test image markers and `dsdebug` addresses are repository conventions.
CI/CD stays disabled; these gates run locally.
