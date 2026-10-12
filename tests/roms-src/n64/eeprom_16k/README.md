# Original N64 16 Kbit EEPROM cartridge

This repository-owned MIPS cartridge is assembled by `build.py` without
Nintendo firmware, SDK or retail bytes. The `E16K` header marker at `$3C`
selects the 2,048-byte cartridge EEPROM profile. The guest sends real SI/PIF
Joybus packets and writes observed EEPROM bytes and write status to the
register-file channel. The guest computes no pass verdict.

The cartridge reads all **256 blank eight-byte blocks**, then writes and reads
every block with **256 distinct payload patterns**. It rereads every block
after all writes to catch address aliasing. The **66,048 guest cases / 132,096
captured 64-bit words** span all 2,048 bytes. `run.py` independently grades the
file. It also checks production frontend persistence across fresh processes,
rejects malformed battery data, executes a wrong-read ROM that fails all
65,536 write/read cases, and rejects seven damaged register files.

The component test checks the 16 Kbit identity (`00 C0 00`), all 256 block
addresses and malformed packet lengths. The plugin checkpoint test checks
2,048-byte battery data, deterministic guest continuation, rollback, reset
and new-cartridge erasure. S033 states store all 2,048 bytes plus the selected media size; S032 imports the
first 512 bytes and erases the remainder. Busy timing and other save media
remain outside this profile. Recognized native 6102 cartridges default to
4 Kbit; an existing 2,048-byte save selects 16 Kbit before guest execution.
New native 16 Kbit cartridges still need cartridge metadata or a user setting.

```sh
python3 tests/roms-src/build.py n64/eeprom_16k --original-only --check
ctest --test-dir build -R '^n64_eeprom_16k' --output-on-failure
```

Artifacts stay under `build/artifacts/owned/n64_eeprom_16k/`; the test cleans
its temporary save. The Joybus identity and block layout match
[ares' cartridge implementation](https://github.com/ares-emulator/ares/blob/master/ares/n64/cartridge/joybus.cpp).
