# Original N64 Rumble Pak cartridge

This repository-built MIPS ROM sends status, accessory writes and reads through
guest SI DMA on all four controller ports. It probes every one of the 2,048
aligned 32-byte accessory addresses during motor off, on and off phases. The
host checks all **24,596 complete 64-byte SI packets / 196,768 guest register
words**, including address CRC, payload CRC, status insertion latches, address
regions and motor response. An executed wrong-register cartridge and three
damaged result files must fail the independent host oracle.

The cartridge contains no Nintendo boot or retail bytes. The harness enables
Rumble Paks through plugin configuration and checks the S035 frontend snapshot.
Separate component gates check malformed commands, state rollback and older
state imports. An SDL virtual gamepad gate checks feedback routing, stopping
and reassignment. These are functional checks; physical accessory electrical
timing and feedback on a real controller have not been measured.

```sh
python3 tests/roms-src/build.py n64/rumble_pak --original-only --check
ctest --test-dir build -R '^n64_rumble_' --output-on-failure
```

Results are under `build/artifacts/owned/n64_rumble_pak/`. Joybus response
regions follow the [ares N64 gamepad implementation](https://github.com/ares-emulator/ares/blob/master/ares/n64/controller/gamepad/gamepad.cpp);
the output pulse uses [SDL GameControllerRumble](https://wiki.libsdl.org/SDL2/SDL_GameControllerRumble).
