# Original 3DS netplay register probe

This MIT-licensed repository-authored `.v3ds` guest runs an independent HID
polling loop on both ARM11 contexts and ARM9. Each context publishes the
input sum, last input and iteration count in shared FCRAM at `20001000`,
`2000100c` and `20001018`. Three following words are zero guards. The test peer
reads these twelve words from the actual core and writes `.result` files;
full `.state` files are retained separately. No screen or guest verdict is scored.

Each program has nine setup instructions, then a seven-instruction loop:
load HID_PAD, add to a register, store sum, store last value, increment a count,
store count, branch. The supplied scheduler gives each context 100,000 attempts
per frame. A 100-frame sequence uses host virtual buttons `(frame*7+3)&4095`.
The independent host oracle in `tests/netplay_network_test.py` derives sums,
last values and counts from the load/store attempt positions, including loads
and stores crossing frame boundaries. It does not import the CPU or builder
for expected values. This is deterministic supplied-scheduler evidence, not
physical hardware timing or exhaustive input histories.

`ctr_netplay_plugin_test` runs a solo baseline, then two independent processes
with the production netplay plugin over IPv4 loopback TCP. Both final
**135,791,096-byte states** must equal the baseline byte-for-byte, and all twelve
published words must match the independent oracle. The host owns the only
controller; the joining viewer predicts and corrects host input. This mirrors
one emulator instance; 3DS wireless/link multiplayer is not implemented.

Controls include an executed guest replacing HID loads with zero, which must
change exactly six published words; twelve single-field mutations and four
invalid register streams must fail. The stream requires the 3DS/100-frame
header, role, rollback metadata, ordered exact register records, newline and
EOF. ROM mismatch and four fragmented invalid TCP frames reject; different
pre-session histories reset, and core replacement/external frame changes
pause and disconnect. These are local transport and replay checks, not
Internet, physical-device, cross-platform or GUI-rendering verification.

```sh
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target ctr_plugin netplay_plugin_peer netplay_default -j4
ctest --test-dir build -R '^ctr_netplay_plugin_test$' --output-on-failure
```

The test requires loopback socket permission and up to several GiB of RAM.
At window 8 the rollback ring holds nine full states (~1.14 GiB per peer),
plus the core and scratch buffers. Artifacts remain under
`build/artifacts/netplay-3ds/`; CTest timeout is 180 seconds. CI/CD is disabled.
The committed image reproduces from `build.py`; its source/image SHA256 are
in the shared ledger. See [core implementation](../../../../cores/3ds/README.md).
