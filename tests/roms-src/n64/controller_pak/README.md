# Original N64 Controller Pak cartridge

This repository-built MIPS ROM sends Joybus status, Pak write and Pak read
commands through SI DMA. On each of four ports it writes all 1,024 32-byte
blocks, immediately reads them, then rereads every block to catch aliasing.
The ROM records each complete 64-byte SI reply as eight guest register words
in a result file: **12,292 packets / 98,336 register words**. The host oracle
grades the entire file and the final 131,072 Pak bytes. An executed wrong-
register mutant and three malformed files must be rejected.

The cartridge contains no Nintendo boot or retail bytes. The harness enables
all four Paks through the core plugin configuration. Cross-session `.mpk`
persistence and S034 state rollback have separate production frontend gates.
This corpus covers functional addressed storage; game filesystem integration,
physical serial timing and other accessory types remain unverified.

```sh
python3 tests/roms-src/build.py n64/controller_pak --original-only --check
ctest --test-dir build -R '^n64_controller_pak$' --output-on-failure
```

Results: `build/artifacts/owned/n64_controller_pak/first.result`, `first.json`
and `first.paks`. Packet behavior follows the [ares N64 gamepad
implementation](https://github.com/ares-emulator/ares/blob/master/ares/n64/controller/gamepad/gamepad.cpp),
with accessory CRC checked using [libdragon's reference](https://github.com/DragonMinded/libdragon/blob/trunk/src/joybus/joybus_accessory.c).
