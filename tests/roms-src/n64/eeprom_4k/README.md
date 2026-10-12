# Original N64 4 Kbit EEPROM cartridge

This repository-owned MIPS cartridge executes on the N64 core's explicit
homebrew entry profile. Its instructions and data table are assembled from
`build.py`; it contains no Nintendo firmware, SDK or retail ROM bytes. The
`E4K1` header marker at `$3C` selects the 4 Kbit EEPROM profile. The
guest formats real SI/PIF command packets and writes only observed read bytes
and write status to the register-file channel. It computes no pass verdict.

The cartridge first reads all **64 blank eight-byte blocks**, then writes and
immediately reads every block with **256 distinct payload patterns**. It
finally rereads every block to detect address aliasing after all writes.
These **16,512 cases / 33,024 guest 64-bit words** are independently graded by
`run.py`. The table spans the full 512-byte device; it does not cover physical
serial timing, SRAM, FlashRAM or controller accessories.

The runner also checks production frontend persistence across two processes,
rejects a malformed one-byte save, executes a wrong-read cartridge that fails
all 16,384 write/read cases, and rejects seven damaged register files. A
component test covers EEPROM identification, all valid block addresses,
out-of-range addresses, malformed command lengths, and reset behavior.
Native state tests cover S033 replay and atomic S032 legacy import.

```sh
python3 tests/roms-src/build.py n64/eeprom_4k --original-only --check
cmake --build build --target veloce n64_plugin n64_eeprom_joybus_test n64_pif_state_test
ctest --test-dir build -R '^n64_(eeprom_4k|eeprom_joybus_test|pif_state_test)$' --output-on-failure
```

Outputs stay under `build/artifacts/owned/n64_eeprom_4k/`. The cartridge's
temporary battery save is removed after the run. The 4 Kbit block layout and
Joybus command values follow [libdragon's EEPROM documentation](https://github.com/DragonMinded/libdragon/blob/trunk/include/eeprom.h)
and [ares' cartridge Joybus implementation](https://github.com/ares-emulator/ares/blob/master/ares/n64/cartridge/joybus.cpp).
