# Original N64 native FlashRAM identity cartridge

This 4,100-byte repository-built native-profile cartridge contains original
MIPS code and a synthetic 6102 CRC suffix. It contains no Nintendo IPL, SDK
or retail bytes. The guest sends an E1 FlashRAM silicon-ID command, performs
PI DMA into RDRAM, and writes the two observed register values to the test
channel. The host runs the same stub with all **20 known FlashRAM cartridge
IDs** and both swapped ROM formats. Its independent file oracle grades **22
guest cases / 44 register words** from `registers.txt`, and rejects an executed
wrong-register cartridge and a damaged result file.

The gate also verifies that each native cartridge selects erased 128 KiB
FlashRAM. The `NAF` RTC board additionally stores a 40-byte clock trailer in
its battery file. Eight IDs expect the older Macronix silicon ID; the other twelve
and the swapped controls expect the newer ID. Full page programming, erase and
save rollback are covered by the separate [FlashRAM cartridge](../cartridge_flash/README.md).

```sh
python3 tests/roms-src/build.py n64/native_flash_identity --original-only --check
ctest --test-dir build -R '^n64_native_flash_identity_test$' --output-on-failure
```

The ID selection follows the [ares N64 cartridge table](https://github.com/ares-emulator/ares/blob/master/mia/medium/nintendo-64.cpp),
and the silicon ID command follows [Mupen64Plus FlashRAM](https://github.com/mupen64plus/mupen64plus-core/blob/master/src/device/cart/flashram.c).
Physical PI timing and retail title save behavior remain unverified.
