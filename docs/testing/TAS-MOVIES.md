# TAS2 binary movie layout

All integers use little-endian encoding. Fields are written individually; C++
struct padding and bool object representations are not part of this format.

| Offset | Bytes | Field |
| --- | ---: | --- |
| 0 | 4 | ASCII `TAS2` |
| 4 | 256 | Title, NUL-terminated |
| 260 | 128 | Author, NUL-terminated |
| 388 | 1024 | Description, NUL-terminated |
| 1412 | 32 | Platform, NUL-terminated |
| 1444 | 256 | ROM name, NUL-terminated |
| 1700 | 4 | ROM CRC32 |
| 1704 | 8 | Number of frames |
| 1712 | 8 | Rerecord count |
| 1720 | 1 | Starts from state (0 or 1) |
| 1721 | 4 | State payload length |
| 1725 | variable | Opaque native core state bytes |
| after state | 29 × frame count | Frame records |

Each frame record has 8-byte sequential index, four 4-byte frontend controller
masks, one flag byte, and a signed 4-byte state slot. Flag bit 0 requests reset;
bit 1 requests a state save. Remaining bits must be zero. When a save is requested,
slots are 0–9. Unused slot values are preserved. Controller masks use Veloce's
`VirtualButton` ordering; NES hardware port bytes must be converted first.

A state-start movie requires a nonempty payload; a reset-start movie requires
an empty payload. Reads require exact length, sequential frame indices starting
at zero, bounded fields and terminated strings. Limits: 1,000,000 frames,
64 MiB state, 100 MiB file. These are format/implementation bounds, not evidence
of a hardware domain being exhausted. Native snapshots retain their core/build
compatibility requirements.

See [backend evidence and limitations](../../plugins/tas_default/README.md#verified-backend-evidence)
for independently packed byte tests, negative controls and frame boundary tests.
