# Original V810 address-trap register corpus

The standalone [builder](build.py) emits an original marked Virtual Boy ROM,
without an SDK, commercial assets, expected result tables or a guest pass verdict.
The [host grader](run.py) independently grades register files from frontend/shared
plugin execution. Behavior follows the
[NEC V810 architecture manual](https://www.virtual-boy.com/downloads/978644/get/978650),
including ADTRE's fixed-zero low bit, AE enable, PC equality, `$FFC0` exception
code, saved faulting PC, normal/nested entry and RETI.

## Declared domain

The full ROM matched **4,982,784/4,982,784 fourteen-word snapshots**,
with **zero mismatches**:

- Six raw ADTRE inputs (`PC`, `PC+1`, `PC+2`, `PC-1`, a WRAM storage mirror,
  and a high-bit board alias), each with all **131,072 writable PSW combinations
  with NP clear**. Condition/sticky floating flags, all interrupt levels, ID,
  AE and both EP states are included.
- All 32 single-bit flips of PC, with all 16 condition-flag states, AE and EP.
- All 65,536 low-half offsets added to PC, with those same 64 PSW states.

There are **131,168 matching enabled cases**. The others verify disabled or
nonmatching behavior. The domains intentionally overlap; each declared case
is retained and graded. This is a finite architectural domain, not all possible
32-bit address/PSW combinations or a whole-platform completion percentage.

The measured instruction at `$05003000` is LDSR to clear AE, followed by a JMP
to observation code. All setup, ADTRE writes, instruction execution and exception
handling happen in the guest. The instruction allows arbitrary nonmatching
ADTRE values, including the next instruction's address, to finish without a
later trap obscuring the measured event. An enabled match traps before the LDSR
executes. The handler captures original saved PC/PSW and entry PSW, clears AE
in the selected saved PSW, and executes RETI to retry the same instruction.
The vector stubs preserve flags and identify normal versus nested entry.

Each row contains EIPC, EIPSW, FEPC, FEPSW, ECR, post-return PSW, ADTRE,
vector marker, original saved PC, entry PSW, original saved PSW, and unchanged
sentinel GPRs r1/r2/r30. ECR is read-only and persists; the host tracks both halves
across the complete case sequence. Normal and nested bank sentinels distinguish
which context changed. All 14 words are graded. No images or stdout verdicts
are scored.

The reduced positive control keeps all single-bit address flips and 256 low-half
offsets. Executed address-XOR and saved-context capture mutations failed as required;
seven damaged files returned ERROR. Identity includes actual whole-ROM CRC32;
group/row counts, completion and frontend terminator trailer must be exact.
The reduced control never substitutes for the full archived domain.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target veloce vb_plugin vb_machine_test -j
ctest --test-dir build -R '^vb_' --output-on-failure
```

Raw files and reports are retained in `build/artifacts/owned/vb_address_traps/`. The manifest records the ROM and standalone
builder SHA256.

## Limits

Component checks separately exercise **12,582,912 cases**: every writable PSW,
six address relationships at eight full-PC boundaries, including NP-set fatal
cases. They verify zero instruction fetches on a trap, unchanged GPRs, both
saved banks, fatal source/PSW/PC bus writes and halt, and nonmatching MOV execution.
Armed-trap and handler save-state continuation are also checked. These checks
do not enlarge the original-ROM domain.

Physical hardware, pipeline/bus timing and simultaneous external interrupt
priority remain unverified. Unmapped board storage still stops diagnostically;
matching address traps take precedence before attempting an instruction fetch.
Other remaining CPU/board features, VIP/VSU, controllers and retail games are
listed in the [core's implementation limits](../../../../cores/vb/README.md).
