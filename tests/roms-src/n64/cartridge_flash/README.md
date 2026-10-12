# Original N64 cartridge FlashRAM corpus

This original MIPS cartridge selects FlashRAM silicon ID mode, then programs
and reads every one of its 1,024 pages through guest PI commands and DMA. The
guest records the ID and 20 register words per page. An independent file oracle
checks **1,024 guest page cases / 20,480 register words**, plus all **131,072
saved bytes**. An executed wrong-register cartridge and three damaged result
files must fail. The archived ROM contains no Nintendo boot or retail bytes.

The component gate covers the new and old Macronix IDs, old-model array
address mapping, programming, all eight 16 KiB sector erases, chip erase,
reset retention, S037 rollback during a pending program operation and malformed
state rejection. The frontend gate checks `.sav` reload across sessions,
malformed-file rejection and frontend state rollback. Known native cartridge
IDs select FlashRAM automatically; an existing 128 KiB save can identify an
unknown native board.

This is a functional command and settled transfer profile. Physical PI DMA
timing, busy and stale-status intervals, odd or unaligned transfers, retail
game save APIs and full-title playability remain unverified.

```sh
python3 tests/roms-src/build.py n64/cartridge_flash --original-only --check
ctest --test-dir build -R '^n64_(cartridge_flash|flash_)' --output-on-failure
```

Artifacts are under `build/artifacts/owned/n64_cartridge_flash/` and
`build/artifacts/owned/n64_flash_frontend/`. The command profile follows
[Mupen64Plus FlashRAM](https://github.com/mupen64plus/mupen64plus-core/blob/master/src/device/cart/flashram.c)
and [ares FlashRAM](https://github.com/ares-emulator/ares/blob/master/ares/n64/cartridge/flash.cpp);
media IDs follow the [ares N64 cartridge table](https://github.com/ares-emulator/ares/blob/master/mia/medium/nintendo-64.cpp).
