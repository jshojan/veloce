# Original DS netplay polling probe

This MIT-licensed original cartridge continuously reads both CPUs' KEYINPUT and
ARM7 EXTKEYIN into registers, accumulates the observed values, and publishes
eight words in shared main RAM. It uses its own instruction encoder and entry
code, without an SDK, firmware, BIOS, Nintendo logo or commercial ROM. It is
built independently from the exhaustive [keypad corpus](../keypad/README.md).

| Main RAM address | Guest observation |
| --- | --- |
| `$02001000` | ARM9 accumulated KEYINPUT reads |
| `$02001004` | ARM9 last KEYINPUT read |
| `$02001008` | ARM9 published poll count |
| `$0200100C` | Unchanged zero guard |
| `$02001010` | ARM7 accumulated KEYINPUT reads |
| `$02001014` | ARM7 last KEYINPUT read |
| `$02001018` | ARM7 accumulated EXTKEYIN reads |
| `$0200101C` | ARM7 last EXTKEYIN read |

`nds_netplay_plugin_test` loads the actual DS and production netplay shared
plugins in two independent processes over loopback TCP. The host owns the one
DS keypad; the joining peer is a viewer that predicts and corrects host input.
Both run the same DS instance. **This does not emulate a DS wireless link or two
separate handhelds playing a multiplayer game.**

The 100-frame test passes: both 4,292,968-byte CPU/RAM/input states equal a third
baseline process byte-for-byte. Eight published guest words on all three runs
also match an independent host arithmetic oracle. It derives each sum from the
load's input frame and exact static loop position, including loads/stores
crossing frame boundaries. The probe carries no expected answers. The host
captures these RAM words in `.result` files; output is not a visual verdict.

Different prior histories are removed by fresh-core preparation. The test checks
handshake/start/pause/confirmation, viewer rollback, fixed host controller
ownership, ROM mismatch rejection and existing fragmented-invalid-message
controls. Core replacement/external frame changes disconnect and pause. A
separate component gate checks short/null/malformed state rejection and ordinary
versus rollback state equivalence. Viewer input cannot alter the DS controller;
keypad coverage remains the separately graded finite hardware domain.

```bash
python3 tests/roms-src/build.py nds/netplay_probe --original-only --check
cmake --build build --target netplay_plugin_peer netplay_rollback_test nds_machine_test -j
ctest --test-dir build -R '^(nds_netplay_plugin_test|nds_machine_test)$' --output-on-failure
```

Loopback socket permission is required. Raw registers, logs, full states and the
JSON report remain in `build/artifacts/netplay-nds/`. The manifest records generator/image SHA256. The current DS core produces
no audio, so this test verifies state rollback without claiming a DS audio replay
check. It uses the supplied direct-load/cache-free profile and bounded scheduler;
physical bus/CPU polling timing, rendered frontend/video, physical controller
use, Internet/cross-platform peers and DS wireless multiplayer remain unverified
or unimplemented. Whole-platform completion is unknown.
