# Original N64 RDP combiner command progression

MIT source and reproducible numeric MIPS assembly; no SDK, firmware or commercial
bytes. Build with `python3 tests/roms-src/build.py n64/rdp_combiner_commands --original-only`.
Run with `python3 tests/roms-src/n64/rdp_combiner_commands/run.py`.

The finite domain is **131,072 cases**: every 16-bit repeated payload, from both
RDRAM and XBUS DMEM. Lists issue SET_COMBINE, SET_PRIM_COLOR and SET_ENV_COLOR.
DMEM lists cross its 4 KiB boundary. The guest captures seven actual 64-bit
register/RAM words per case: CURRENT, STATUS and MI pending before FullSync,
the same three after a separately extended FullSync, and an untouched RAM guard.
All **917,504 words** are independently graded. State commands must progress
without asserting DP IRQ; only the later FullSync asserts it. A repeated payload
covers every selector and color bit, but is not the full 56-bit payload domain.

**This ROM has no hardware readback of internal combiner state.** It grades
command consumption, IRQ and guard behavior. The separate
`n64_rdp_combiner_test` component checks internal retention, 43,264 selector
pairs, 524,288 RAM/DMEM streams, LOD fields, unsupported draw diagnostics,
frozen save replay, legacy conversion and atomic invalid-state rejection.
Neither gate grades color-combiner arithmetic, textured/triangle rasterization,
physical DMA timing or retail compatibility. Those features remain pending.
Unused color/primitive command bits are discarded as a deterministic convention;
physical reserved-bit behavior is ungraded.

The driver executes a mutated capture ROM that must fail every reduced case,
mutates all seven captured fields, and rejects seven malformed or incomplete
streams. ROM CRC, suite, exact case domain, all rows, completion and emulator
trailer are required. Results are under `build/artifacts/owned/n64_rdp_combiner_commands/`.

Field layout source: [SGI RDP Command Summary, tables 19, 21 and 22](https://hcs64.com/files/RDP_COMMANDS.pdf).
