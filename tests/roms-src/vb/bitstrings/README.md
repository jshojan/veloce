# Original V810 arithmetic bit-string corpus

The standalone [builder](build.py) emits original reset-vector code and a marked
Virtual Boy cartridge without an SDK, commercial assets, expected results or a
guest pass verdict. The [host grader](run.py) reads actual shared-plugin/frontend
output and derives expectations using whole bit sequences, independently of the
core's word-buffer implementation. Instruction definitions follow the
[NEC V810 architecture manual](https://www.virtual-boy.com/downloads/978644/get/978650).

## Declared domain

**14,680,064/14,680,064 fourteen-word snapshots matched, zero mismatches**,
for ORBSU,
ANDBSU, XORBSU, MOVBSU, ORNBSU, ANDNBSU, XORNBSU and NOTBSU. Each group includes
all 16 incoming Z/S/OV/CY states:

| Group | Domain across all eight operations | Snapshots |
|---|---|---|
| Byte pairs | Every pair of repeated-byte source/destination words; offsets zero, length eight | 8,388,608 |
| Offsets and lengths | All 32 source × 32 destination offsets; lengths 0, 1, 7, 8, 15, 16, 31, 32, 33, 63, 64, 65; all four zero/all-one word pairs | 6,291,456 |

The guest seeds three source words and four destination words using distinct
complement/XOR patterns. It records four resulting destination words, three
unchanged source words, r26–r30, PSW and a leading destination guard. The host
checks preserved bits outside the selected range, cross-word transfers, working
register progress, zero remaining length, preserved PSW and guards. The source
and destination banks do not overlap. Zero-length cases preserve the seeded
memory and canonical working registers.

The ROM streams observations through the emulator-only debug address. No visual
output or guest verdict is scored. Whole-image CRC32 identity, exact group/row
counts, completion and frontend terminator metadata are mandatory. The reduced
positive control runs 88,064 cases. An executed MOVBSU-to-XORBSU mutation failed
only the MOV groups; seven damaged files returned ERROR. Reduced controls never
replace full-domain grading. Domain completeness and hand reference vectors are
checked before execution.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target veloce vb_plugin vb_machine_test -j
ctest --test-dir build -R '^vb_' -j2 --output-on-failure
```

Raw output/reports remain in `build/artifacts/owned/vb_bitstrings/` and are. Source/image SHA256 are recorded in the manifest.

## Execution, saved state and limits

Each core scheduling step processes at most 32 bits, ending at the first source
or destination word boundary. It retains a source word until consumed and a
destination word until flushed. The PC advances after the instruction completes;
r26–r30 track progress. A maximum 32-bit length cannot block one frontend step.
`VVBS0002` saves pending word buffers and their validity along with CPU/WRAM.
Legacy `VVBS0001` states are rejected; recreate them with the updated core.

Separate component checks compare 3,145,728 transfers against a per-bit byte
array oracle and preserve every writable PSW in 2,097,152 cases. They also check
zero-length bus silence, one opcode fetch, bounded long transfers, exact saved
continuation with retained source/unflushed destination buffers, six malformed
or legacy state variants, rejection atomicity and a restorable diagnostic stop.
These checks do not enlarge the original-ROM domain.

This measures the declared non-overlapping operand domain against architectural
instruction definitions, not physical hardware. Overlap visibility, word-buffer
bus order, invalid offset/address fields, bus faults, interrupt abort/resume and
pipeline/wait-state timing remain unverified. The current model masks initial
offsets to five bits and addresses to word boundaries; invalid fields are not
scored by this ROM. Bit-string searches, remaining CPU/NVC instructions, VIP/VSU,
controllers, retail games and most platform hardware remain missing or
unverified. Full Virtual Boy completion is unknown.
