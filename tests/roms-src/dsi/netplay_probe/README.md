# Original DSi netplay register probe

`netplay_probe.vdsi` is repository-owned dual-CPU ARM polling code. It samples
digital keypad registers and stores sums/current values plus a poll count in
main RAM at `02001000`. It has no expected answers or guest verdict. This
probe uses the same declared instruction/input schedule as the
[DS probe](../../nds/netplay_probe/README.md), with DSi unit code, profile marker,
loader and full 16 MiB state identity.

The production TCP rollback gate runs two independent peer processes for
100 live frames. Their complete **16,875,896-byte** states match the solo
baseline; eight persisted guest register words match an independent host
oracle. Fragmented invalid packets and ROM mismatch reject. Sessions start
from reset despite prior histories and disconnect on core replacement.
Host owns one keypad and joining peer views/predicts/replays; DSi wireless
multiplayer is not emulated. Physical controllers and Internet/relay/Windows
sessions remain unverified.

```sh
ctest --test-dir build -R '^dsi_netplay_plugin_test$' --output-on-failure
```

Artifacts are under `build/artifacts/netplay-dsi/`. Local loopback sockets
must be allowed. CI/CD stays disabled. [Core implementation and limits](../../../../cores/dsi/README.md).
