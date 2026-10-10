# Nintendo implementation and hardware test coverage

The goal includes Nintendo core plugins, repository-owned ROMs covering the
hardware, machine-readable register observations, netplay, seamless controllers,
and documentation. The full goal is incomplete. Existing test-subset scores
are not measurements of overall hardware or product completion.

## Implemented cores and original ROMs

| Core family | Plugin | Original hardware evidence | Full core completion |
|---|---|---|---|
| NES/Famicom | `nes_plugin` | ADC/SBC zero-page arithmetic: 524,288/524,288 A/P pairs verified (100% of both operands, C and D states) | Not measured; other CPU instructions/addressing/timing and peripheral corpora remain |
| GB/CGB | `gb_plugin` | DMG WRAM/ADD probe: 2/2 checks; byte ALU E-register corpus: 8,388,608/8,388,608 A/F pairs matched (100% of eight operations, every operand pair, and all 16 incoming flag states) | Not measured; other CPU operands/instructions, timing/PPU/APU/MBC/CGB corpus remains |
| SNES | `snes_plugin` | Settled multiplication: 65,536/65,536 operand pairs verified (100%); division: 3,328 selected boundary cases verified | Not measured; timing/intermediate math results and remaining hardware corpora pending |
| GBA | `gba_plugin` | ARM MOVS register shifts: 65,536/65,536 result/CPSR pairs verified; all low-byte amounts and C/V inputs on 16 operands | Not measured; remaining CPU modes/instructions, timing and peripheral corpora pending |

Run `python3 tests/roms-src/build.py --original-only --check` to reproduce the
original ROM images and verify their source and image SHA256 ledger. Run
`ctest --test-dir build -L owned --output-on-failure` to execute them. Raw
register files and reports remain in `build/artifacts/owned/`, and CI uploads
them as `owned-hardware-registers`.

The NES, GB ALU, SNES, and GBA corpora are graded by host integer oracles that read
every observed register result. The small GB WRAM probe emits its own `CHECK`
lines alongside observed values. The GB ALU ROM uses the existing SB/SC start
observation hook without waiting for link delivery; physical serial timing and
retail boot are unverified.
No original suite scores a screenshot or stdout verdict. External ROM
suites still use their documented legacy detection paths; they have not all
been migrated to original ROMs or register-file scoring.

## Coverage still required

Every core needs explicit, reproducible domains and independent reference
expectations for:

- CPU instructions, addressing modes, flags, stack and bus behavior; exhaustive
  operand enumeration where finite domains are practical, plus specified
  boundaries for wide arithmetic and every supported architectural mode.
- Interrupt entry/exit, halt/stop/reset, priority, and each timing phase that can
  change an externally observed result.
- Memory maps, banks, mirrors, open bus, cartridge/save hardware, and every
  documented read/write register behavior.
- DMA transfers, overlap/priority, CPU stalls, audio transfers, and completion
  interrupts across start phases and lengths.
- Video register behavior, scanline/dot timing, memory access restrictions,
  sprites, blending, and rendering output as raw observed data in files.
- Audio clocks, envelopes, counters, FIFO/DSP behavior, and observed samples.
- Timers, serial/link peripherals, controller protocols, and platform-specific
  coprocessors or accessories.

ROM generators, expected-value oracles, committed binaries, coverage metadata,
and deliberate failure controls are required for each corpus. A new platform
must implement its hardware as a loadable core plugin and execute its own ROMs;
an empty plugin or declared platform name does not count as implementation.

Additional requested platform families without a core in this worktree include
Virtual Boy, Nintendo 64, Nintendo DS/DSi, GameCube/Wii, Nintendo 3DS, Wii U,
and Switch. Famicom Disk System and other Nintendo accessories still need
separate validation. There is no verified completion percentage for these
missing implementations.

## Modern feature evidence still required

The SDL virtual-controller test verifies two identical pads, duplicate events,
custom mappings, capture mode, disconnect/reconnect, saved identity restoration,
selection after removal, ambiguous devices, and axis endpoints. Physical devices,
Bluetooth, cross-platform drivers, and multiplayer routing still need testing.

The frontend has netplay interfaces/UI and the NES core exposes `INetplayCapable`.
The [two-player rollback module](../plugins/netplay_default/README.md) implements
bounded history, prediction, late-input restore/replay, stalls, and confirmation
validation. Two delayed actual NES sessions converge byte-for-byte on a 100-frame
baseline running an original controller ROM. Guest RAM observations and states
are retained in files; buffered/streaming replay audio is exercised in the test.
The IPv4 TCP transport also passes a two-process loopback test: both complete
NES states match the baseline after late-input replay, ROM mismatch is rejected,
and four fragmented malformed-message cases fail cleanly. Network reports,
guest registers, logs, and states are saved under `build/artifacts/network/`.
This establishes the tested local transport/replay scenarios, not whole-core
accuracy or Internet play.

The default plugin now uses both modules for direct IPv4 play. The frontend
polls while paused, lets the plugin own each live frame, and gates replay audio.
An additional two-process production-plugin test verifies complete state
equality, mismatch/malformed-message rejection, replay audio, and pause/disconnect
on a replaced core or externally advanced frame. Its register observations and
states are retained under `build/artifacts/netplay-plugin/`. Reset, ROM/core
replacement, and savestate loads disconnect before mutating the live core.
Rendered GUI operation, reconnects, desync detection/recovery, Windows, Internet
conditions, and physical two-machine play remain unverified. Relay, chat,
session codes, spectators, and input delay scheduling are not implemented.
Other core families need complete deterministic state and appropriate
multiplayer/link behavior before their netplay can be claimed as verified.

The corrected NES/GB/GBA CRC32 identities can change the identifier of a ROM
previously loaded with the mistyped table. Old savestate filenames and embedded
ROM identifiers may therefore stop matching; recreate those states. The core
state serialization format itself has not changed in this increment.
