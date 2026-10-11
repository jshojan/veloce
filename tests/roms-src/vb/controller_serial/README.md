# Original Virtual Boy software controller corpus

Repository-owned MIT V810 source, without SDK, firmware or commercial bytes.
The 1,024-byte marked cartridge reproduces with SHA-256
`6fb8b708be26939b2488122eee12518b016cc75fd6c3283a726fd3d476c24e72`.
Register behavior is grounded in the author's
[hardware reference](https://files.virtual-boy.com/download/978651/stsvb.html)
(Game Pad Control/Data Registers).

## Guest program and actual file observations

The guest drives native SCR software latch/clock writes and reads SDLR/SDHR.
It performs a complete sixteen-edge transfer, attempts writes to the read-only
result registers, repeats the clock level, writes eight actual observations to
WRAM `0x05001000..0x0500101f`, then HALTs. The capture tool feeds ordinary
production plugin input and copies those actual WRAM words into a strict file.
The ROM contains no expected answers or correctness verdict.

The eight fields are low data, high data, SCR, low/high data after read-only
writes, SCR after a repeated clock level, zero and a fixed sentinel.
The independent host oracle calculates wire bits from the declared button
combination. It does not import CPU, controller or builder arithmetic.

| Executed domain | Extent |
|---|---:|
| Both D-pads, A/B, L/R, Start/Select combinations | All 16,384 |
| IN/OUT access widths | Byte, halfword, word |
| Selected base mirrors | `02000000`, `02000040`, `02ffffc0`, `82000000` |
| Actual guest snapshots | 196,608 |
| Actual 32-bit WRAM words | 1,572,864 |

All fourteen digital inputs are covered, including simultaneous opposite
D-pad directions. These are four selected mirrors, not every address alias.
The peripheral stores only a byte per register; wider reads zero the unused
bits in the supplied profile. Undefined upper bus bits are not claimed as
physical hardware results.

## Executed rejection and state evidence

A same-length wrong-capture instruction fails all sixteen short rows (one word
each). An omitted final clock fails sixteen short rows / 48 words. Eight
individual field mutations and four malformed streams reject. Capture binary,
ROM, builder, recipe, CPU/controller sources and grader hashes remain unchanged
through execution and are recorded in `controller.json`.

The component passes 311,552 button/clock/control observations. This includes
all button combinations, changing input after the latch, each partial clock's
state restoration, repeated clock levels, relatching, read-only preservation,
all 256 control-byte values and atomic rejection of unsupported hardware reads.
The supplied policy retains old data before the final edge; those undefined
partial data values are profile checks, excluded from native hardware scoring.
Machine checks preserve pending serial state and reject invalid state fields
before mutation. Selected actual plugin HALT continuations match whole states.

`VVBS0003` includes the pad's input, latched word, completed data, control and
clock count. Old `VVBS0002` states load with neutral pad state; older core
versions cannot consume the new state format.

## Frontend and remaining hardware

The native layout exposes fourteen controls. Left D-pad uses arrows; right
D-pad uses W/A/S/D, A/B use Z/X, L/R use Q/E, and Start/Select retain Enter/
Backspace. A modern gamepad uses its D-pad and thresholded right stick. Nine
virtual right-stick direction combinations pass the input manager/default plugin
contract; default bindings and remap persistence are checked.

Hardware automatic polling, key interrupt producers, physical timing and
power/battery behavior remain unfinished. Unsupported un-aborted hardware-read
requests fail atomically. CPU/NVC completion, VIP/VSU, native retail boot,
cartridge saves and netplay remain unfinished. This does not establish retail
playability or the full native hardware corpus.

```bash
python3 tests/roms-src/build.py vb/controller_serial --check
ctest --test-dir build -R '^vb_(controller_test|controller_serial)$|^input_hotplug_test$' --output-on-failure
```

Actual register files, executed controls and `controller.json` stay ignored in
`build/artifacts/owned/vb_controller/`. CI/CD remains disabled.
