# Original Wii GameCube-controller serial corpus

MIT repository-owned PowerPC stimulus; no Nintendo SDK, IPL or retail assets.
The archived `controller_serial.vwi` is built by `build.py` and recorded with
its generator/image SHA256 in `tests/roms-src/rom_manifest.json`.

Measured 2026-10-10: **4,096/4,096 digital snapshots matched** (65,536 register
values), or **100% of the declared twelve-button combination domain**. Each
frontend quantum supplies one combination through the actual core plugin. The
ROM submits command `40 03 00` through communication RAM at `$CD006400` and reads
the response, completion/mask fields, TCINT acknowledgement, a separate idle-ID
transaction, three absent channels, latched errors and their acknowledgement.
The last command byte cycles only the stop setting used here (zero). Every
captured word is graded by independent host arithmetic; the guest has no
expected response bytes or pass verdict.

The supplied response has neutral main/C sticks (128), native wire button
positions and the always-set origin marker. Digital L/R also drive their
corresponding trigger endpoint to 255. This is a digital-to-analog mapping;
continuous analog input and physical controller calibration are not measured.
The original profile returns idle ID/status `09 00 00`; this is an emulator
convention and not a claim that every real controller reports that status.
Mode 3 is the supported direct-read format. No controller polling is performed.

Each row contains sixteen words in order: direct response high/low, SICOMCSR,
SISR, acknowledged SICOMCSR; idle identity word/SICOMCSR/SISR; absent-port
response word/SISR for ports 1, 2 and 3; acknowledged SISR and SICOMCSR. ID
response byte four and absent receive bytes stay unchanged. Transfers settle
immediately. The frame-padding loop uses CTR so the next sample begins exactly
at the next 100,000-attempt quantum, including the initial header's attempts.
This quantum does not model a physical video field or Joybus wire time.

An executed command mutation fails all sixteen short-domain rows. Sixteen
single-field mutations fail individually; seven malformed/missing/trailing
streams are rejected. Full raw observations, input schedules, emulator logs
and JSON reports are in `build/artifacts/owned/wii_controller/`.

```bash
cmake --build build --target veloce wii_plugin ppc_serial_test
ctest --test-dir build -R '^(wii_controller_serial|ppc_serial_test)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

`ppc_serial_test` additionally covers all four selected channels, sixteen
connection masks and every digital combination (262,144 snapshots), plus
all input/output lengths 1–128 for identify/reset/direct read on all ports
(196,608 snapshots). Receive boundaries, W1C, state validation, old-state
compatibility, invalid bus accesses and both loaded plugins are checked.
These are functional profile checks, not exhaustive native hardware coverage.
Polling buffers/locking, VI scheduling, wire/busy/overlap timing, CPU interrupt
delivery, other controller modes, origin/recalibration, physical rumble,
accessories, WaveBird, analog axes, four-port frontend routing, Wii Remote and
retail startup remain pending. Netplay is not yet implemented for this core.
