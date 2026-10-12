# Original N64 absent-accessory Joybus cartridge

This repository-built MIPS ROM formats and executes Joybus accessory read
(`02`) and write (`03`) commands through SI DMA on all four controller ports.
For each port it uses all 256 address and payload seeds: **2,048 guest cases**.
The ROM writes the **entire 64-byte returned SI packet** as eight register words
per case to a result file. A separate host oracle grades **16,384 actual guest
register words**. The cartridge carries no Nintendo boot or retail bytes.

The current functional profile has no Controller Pak inserted. A valid read
returns 32 zero bytes and their inverted CRC; a valid write returns the
payload's inverted CRC. The component gate checks the same packet shape and
CRC over all ports and seeds, malformed lengths and the status byte. An
executed wrong-register ROM and three damaged result files are rejected.
The [inserted Controller Pak cartridge](../controller_pak/README.md) now grades
addressed storage on all four ports. Physical SI timing remains unfinished.

```sh
python3 tests/roms-src/build.py n64/accessory_absent --original-only --check
ctest --test-dir build -R '^n64_(accessory_absent|pif_joybus_test)$' --output-on-failure
```

Results: `build/artifacts/owned/n64_accessory_absent/first.result` and
`first.json`. The packet semantics follow [ares' N64 gamepad
implementation](https://github.com/ares-emulator/ares/blob/master/ares/n64/controller/gamepad/gamepad.cpp)
and the CRC uses [libdragon's accessory reference](https://github.com/DragonMinded/libdragon/blob/trunk/src/joybus/joybus_accessory.c).
