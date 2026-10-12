# Original N64 cartridge RTC Joybus corpus

This original native-profile MIPS cartridge uses the synthetic 6102 CRC
builder and the `NAF` cartridge ID. It contains no Nintendo IPL, SDK or retail
bytes. The guest sends RTC status, writes block 2, reads block 2, and writes
the **13 returned Joybus bytes** to a register file through the debug channel.
The independent file oracle grades those bytes and rejects an executed
wrong-register cartridge and a damaged file.

The component gate also checks block 0 stop and write-lock controls, block 2
BCD validation, 2024 and 2400 leap-day rollover, the 2100 non-leap boundary,
one-day calendar advance, reset retention, S038 rollback, S037 import and
malformed-state rejection. The frontend gate
checks a 128 KiB FlashRAM plus 40-byte RTC `.sav` file across sessions,
one-day offline advance, malformed saves and legacy raw 128 KiB FlashRAM import.

The emulated clock starts at **2000-01-01 UTC** for a fresh cartridge and
advances at the nominal 93.75 MHz CPU clock rate. A persisted clock advances
by elapsed host seconds when it is reloaded. This is a functional profile;
physical oscillator drift, serial timing, 64DD RTC and full retail title
behavior remain unverified.

```sh
python3 tests/roms-src/build.py n64/cartridge_rtc --original-only --check
ctest --test-dir build -R '^n64_(cartridge_rtc|cartridge_rtc_test|rtc_frontend_test)$' --output-on-failure
```

The command and block layout follows [ares cartridge Joybus](https://github.com/ares-emulator/ares/blob/master/ares/n64/cartridge/joybus.cpp)
and the [libdragon RTC documentation](https://libdragon.dev/ref/group__rtc.html).
The native `NAF` media assignment follows the [ares cartridge table](https://github.com/ares-emulator/ares/blob/master/mia/medium/nintendo-64.cpp).
