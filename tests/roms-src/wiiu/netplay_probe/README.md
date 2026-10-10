# Original Wii U netplay and sparse high-RAM probe

This MIT-licensed, repository-authored `.vwiiu` guest polls the original virtual
input latch on all three PowerPC contexts. Each publishes input sum, last input
and iteration count in MEM1 at `1000`, `100c` and `1018`. Each also stores the
low input bit to its own word near the end of MEM2 (`8fffffe0/4/8`), reloads it,
and publishes that actual readback at `1024/8/c`. The test peer reads all twelve
words from the actual core and writes `.result` files; canonical complete
`.state` files are retained separately. No image or guest verdict is scored.

The program has ten setup attempts and an eleven-attempt loop: input load,
add, sum/last stores, count increment/store, low-bit mask, far store, far load,
readback store, branch. The supplied quantum is 100,000 attempts per context
per frame. A 100-frame sequence uses host buttons `(frame*7+3)&4095`. Independent
host arithmetic derives each sum, last value, count and readback from the
load/store positions, including operations crossing frame boundaries. Expected
values do not use guest/CPU helpers. This is a supplied-scheduler replay test,
not hardware timing or exhaustive input-history coverage.

Alternating low input bits makes the high MEM2 page nonzero or clears it.
Initial snapshots have three code pages (13,292 bytes); the first nonzero input
also adds MEM1 observations and a far page (21,492 bytes). A cleared far page
is omitted canonically; the final state is **17,392 bytes**. Missing pages
represent zero across the full mapped RAM. The logical memory profile remains
32 MiB MEM1 plus 2 GiB MEM2; the dense-state upper bound is 2,183,168,992 bytes.

`wiiu_netplay_plugin_test` runs a solo baseline and two independent processes
with the production netplay plugin over IPv4 loopback TCP. Both full canonical
states must match the baseline byte-for-byte, and all twelve guest words must
match the independent oracle. Host owns one original digital-input latch; the
joining viewer predicts/replays host input. This mirrors one emulator instance.
Native GamePad/VPAD/KPAD, wireless multiplayer and retail boot are pending.

An executed guest replacing input loads with zero must change exactly six
published words. Twelve single-field mutations and four invalid register
streams must fail. The stream requires exact Wii U/100-frame identity, role,
metadata, ordered register records, final newline and EOF. ROM mismatch and
four fragmented invalid TCP frames reject. Different pre-session histories
reset; core replacement and external frame changes disconnect and pause.

```sh
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target wiiu_plugin netplay_plugin_peer netplay_default -j4
ctest --test-dir build -R '^wiiu_netplay_plugin_test$' --output-on-failure
```

Artifacts remain in `build/artifacts/netplay-wiiu/`: state/register/log files,
control observations and `network.json`. The test needs loopback socket
permission. CTest timeout is 180 seconds; CI/CD remains disabled. Physical
devices/two machines, Internet, cross-platform peers and sustained performance
remain unverified. See [core implementation](../../../../cores/wiiu/README.md).
