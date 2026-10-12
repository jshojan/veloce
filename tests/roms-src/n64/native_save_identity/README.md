# Original N64 native save-media identity cartridge

`build.py` creates a 4,100-byte native-profile image from repository-owned
MIPS instructions and data. It solves a four-byte CRC suffix so the N64 core
recognizes the synthetic IPL3 as its supported 6102 profile; it copies no
Nintendo firmware, SDK or retail bytes. The image is archived in this repo.

The IPL3 stub formats a real cartridge-status Joybus packet in RDRAM, runs SI
write/read DMA, and emits the guest-observed EEPROM identity byte through the
test channel. The production plugin test changes only the three-byte cartridge
ID at header `$3B`-`$3D`. It executes the stub for each of the **23 documented
16 Kbit IDs**, four 4 Kbit fallback controls, and both byte-swapped ROM forms.
Every guest response must be `C0` for 16 Kbit or `80` for 4 Kbit. It also
checks the frontend battery length and rejects a 512-byte save for 16 Kbit IDs.
The test grades 30 guest SI responses; the full 2,048-byte read/write corpus
remains in the separate [16 Kbit EEPROM cartridge](../eeprom_16k/README.md).

The ID list follows [ares' Nintendo 64 media table](https://github.com/ares-emulator/ares/blob/master/mia/medium/nintendo-64.cpp).
Unknown native IDs still use the 4 Kbit fallback unless a pre-existing
2,048-byte save selects 16 Kbit. SRAM, FlashRAM, controller accessories and
physical timing remain incomplete.

```sh
python3 tests/roms-src/build.py n64/native_save_identity --original-only --check
ctest --test-dir build -R '^n64_native_save_identity_test$' --output-on-failure
```
