# Original N64 RDRAM device corpus

The MIT licensed Python assembler emits original MIPS code in `rdram_devices.z64`,
marked `V64HB001`, without SDK, firmware or commercial bytes. The frontend loads
the production plugin and records actual guest reads in a host file. Expected
outputs and pass verdicts exist only in the independent host oracle.

All **14,336/14,336 cases** match **86,016 guest values**:

- Four devices × six storage registers × every repeated byte (6,144 cases).
- Four devices × all 2,048 writable delay-field combinations (8,192 cases).

The ROM activates RI, uses a repeated broadcast to establish write delay,
and discovers four devices with distinct IDs. Every case records the selected
register, the same register on another device, device type, manufacturer,
broadcast read and MI mode after repeat consumption. The host computes expected
bytes and delay fields independently, without CPU or assembler field helpers.
This does not enumerate all independent 32-bit register operands.

An executed value-bit mutation fails all 128 reduced cases. Six captured-field
mutations each fail one row; seven invalid streams produce ERROR. Strict identity,
domain, width, count, completion and trailer checks reject incomplete output.
Raw observations and reports stay in `build/artifacts/owned/n64_rdram_devices/`.
The ROM manifest tracks generator and archived image hashes.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_rdram_test -j
ctest --test-dir build -R '^n64_rdram_(test|devices)$' --output-on-failure
```

The functional register topology is grounded in the authored
[RDRAM register model](https://github.com/ares-emulator/ares/blob/master/ares/n64/rdram/rdram.cpp),
[register bus](https://github.com/ares-emulator/ares/blob/master/ares/n64/rdram/io.cpp)
and [MI bus controls](https://github.com/ares-emulator/ares/blob/master/ares/n64/mi/bus.hpp).
The window follows the archived [RCP register map](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm).
No upstream implementation code is included.

Component evidence adds all 65,536 ID encodings on each device, all delay fields
across eight previous write delays and 129 repeat lengths, every selector/register
alias, 256 mode patterns, disabled-device discovery, broadcast row storage,
MI upper-read suppression/one-shot repeat, atomic rejected MMIO/states,
exact replay, legacy conversion and loaded-plugin execution.

Native RAM remapping, analog current calibration/data degradation, RDRAM refresh,
repeat writes to ordinary RAM, ninth-bit EBus memory, cached transaction behavior,
physical bus timing and retail boot remain pending. The present model stores
mode/current settings and reflects their encoded readback; it does not validate
analog calibration. Super Mario 64 reaches an unsupported CACHE instruction;
video/audio and playability remain unverified.
