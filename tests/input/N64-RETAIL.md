# Local N64 retail input probes

These original text schedules drive a user-supplied ROM through the production
plugin and a separate CPU/device trace. Names describe the investigation target;
none of the scripts awards a boot or playability verdict. Commercial ROMs,
framebuffers, trace data and screenshots remain local under `build/artifacts/`.

| Script | Purpose |
|---|---|
| `n64-title-start.txt` | Title Start pulses |
| `n64-title-menu.txt` | Later Start/A pulses; observed Peach intro |
| `n64-gameplay-start.txt` | Extend Start/A pulses through quantum 26,100; three-billion-attempt diagnostic |
| `n64-gameplay-extended.txt` | Later A pulses through quantum 55,100; six-billion-attempt diagnostic |

Each noncomment row is a zero-based decimal scheduling quantum and hexadecimal
port-zero core input word. The mask remains held until the next transition.
Rows must be strictly increasing and less than 100,000. This is the frontend
core input ABI. Valid untagged N64 digital masks retain neutral axes; tagged signed-stick words are defined in `include/emu/n64_input.hpp`. Legacy UI bits 14/15 are reserved as the codec tag.

```sh
cmake --build build --target retail_core_probe n64_boot_trace n64_plugin
python3 tests/n64_retail_probe.py /path/to/private/sm64.z64 \
  --frames 60000 --input-script tests/input/n64-gameplay-extended.txt \
  --capture build/artifacts/retail/n64-gameplay-extended.ppm \
  --output build/artifacts/retail/n64-gameplay-extended.json
```

`--frames` accepts 1–100,000 scheduling quanta; each corresponds to 100,000
nominal CPU attempts in the separate trace. They are not physical game frames
or a wall-clock duration. The default is 120. Each subprocess has a 900-second
timeout; a timeout or error does not establish a guest hardware blocker.

With `--capture`, the probe saves the last valid framebuffer, including a
uniform framebuffer, and reports its **one-based** completed `capture_quantum`.
For runs longer than 10,000 quanta it also writes each 10,000-quantum checkpoint
to `<capture>.q<quantum>.ppm`. The JSON lists checkpoint quanta, paths and SHA256,
as well as final-capture, ROM, binary and input-script fingerprints. A nonuniform
frame observed earlier remains a separate smoke signal; it no longer determines
which frame is saved. A checkpoint cannot be claimed from a failed framebuffer
read. All captures stay private.

The helper's original-ROM regression uses the uniform framebuffer from
`integer_registers.z64` over 10,001 calls: capture succeeds at quantum 10,001,
checkpoint 10,000 is present, and both PPMs match exactly. This checks capture
semantics, not hardware video accuracy. The normal original register-output ROM
oracles remain separate from these retail diagnostics.


## Checkpoint continuation and signed input

`--load-state` continues a private native checkpoint, and `--save-state` writes
the final one locally. `--frames` is the number of additional scheduling quanta.
The input schedule, capture labels and `starting_quantum` remain **absolute**:
a 60,000 checkpoint plus 4,000 calls ends at quantum 64,000. The loader checks
native ROM identity and rejects a wrong image or damaged state. Use distinct
input/output state paths. `--capture-every` selects a 1..100,000 interval;
its default remains 10,000. The direct C++ helper accepts these option pairs
after `CORE ROM COUNT CAPTURE SCRIPT`, with `-` placeholders for omitted capture
and input files. Its total start plus continuation must not exceed 100,000.

The Python runner fingerprints inputs and executables before execution and
rejects changes during a run. Reports include checkpoint hashes and size.
The separate native trace restores the same starting checkpoint and retains
absolute input timing, reporting initial instructions and actual attempted
steps separately from its final instruction counter.

| Script | Tested continuation |
|---|---|
| `n64-courtyard-dialog.txt` | 60,000 -> 64,000; dismiss intro dialogue, stand in courtyard |
| `n64-courtyard-neutral.txt` | Neutral comparison from 64,000 |
| `n64-courtyard-walk.txt` | Y=+80 from 64,000; 1,000-quantum walking control |
| `n64-courtyard-jump.txt` | A from 64,000; 150-quantum ascending jump |
| `n64-courtyard-attack.txt` | B from 64,000; 100-quantum punch |
| `n64-castle-align.txt` | X=+80 from 70,000; investigated bridge approach; entrance unverified |

```sh
# Boot once and save the private courtyard checkpoint.
python3 tests/n64_retail_probe.py /path/to/private/sm64.z64 \
  --frames 60000 --input-script tests/input/n64-gameplay-extended.txt \
  --capture build/artifacts/retail/analog/courtyard.ppm \
  --save-state build/artifacts/retail/analog/courtyard.state \
  --output build/artifacts/retail/analog/courtyard-report.json
# Dismiss the dialogue and save a shared input-test checkpoint.
python3 tests/n64_retail_probe.py /path/to/private/sm64.z64 \
  --frames 4000 --load-state build/artifacts/retail/analog/courtyard.state \
  --save-state build/artifacts/retail/analog/ready.state \
  --input-script tests/input/n64-courtyard-dialog.txt --capture-every 500 \
  --capture build/artifacts/retail/analog/ready.ppm \
  --output build/artifacts/retail/analog/ready.json
# Run walking and its neutral control separately from that same checkpoint.
python3 tests/n64_retail_probe.py /path/to/private/sm64.z64 \
  --frames 1000 --load-state build/artifacts/retail/analog/ready.state \
  --save-state build/artifacts/retail/analog/walk.state \
  --input-script tests/input/n64-courtyard-walk.txt --capture-every 250 \
  --capture build/artifacts/retail/analog/walk.ppm \
  --output build/artifacts/retail/analog/walk.json
# Repeat with neutral, jump-control, jump, attack-control, attack filenames.
# Durations/scripts: neutral=1000/neutral; jump-control=150/neutral;
# jump=150/jump; attack-control=100/neutral; attack=100/attack.
python3 tests/n64_retail_controls.py /path/to/private/sm64.z64 \
  --artifacts build/artifacts/retail/analog
```

The original file grader supports the exact US ROM fingerprint documented in
its source. It checks same checkpoint/duration/core, trace completion, input and
state fingerprints, ordered RAM/cache watches, standing neutral controls,
walking displacement, ascending jump and punch actions. It emits
`controls-evidence.json` including raw words, positions, action values and source
links. Substituting the actual neutral continuations for walking/jumping/punching
must fail their semantic checks, even when state identities and durations match.
These runs verify bounded courtyard controls; full-title playability remains
unverified.

`n64_state_inspect ROM STATE PHYSICAL_START_HEX WORD_COUNT` restores a checkpoint
on a disposable native machine and reports **physical RAM and the CPU primary
cache view**. It supports aligned ranges up to 512 words within native RDRAM.
It leaves the input file unchanged. Use the CPU view for cached guest variables.
The pinned [Mario field definitions](https://github.com/SM64-TAS-ABC/STROOP/blob/6771197ad0f50718d3d4a13387e01d1e6d9c0d1a/STROOP/Structs/Configurations/MarioConfig.cs)
and [action names](https://github.com/SM64-TAS-ABC/STROOP/blob/6771197ad0f50718d3d4a13387e01d1e6d9c0d1a/STROOP/Config/MarioActions.xml)
provide the US structure at `8033B170`, action offset `0C`, and float XYZ offsets
`3C/40/44`. No external guest or emulator implementation is copied.

```sh
build/bin/n64_state_inspect /path/to/private/sm64.z64 \
  build/artifacts/retail/analog/walk.state 33b170 32
ctest --test-dir build -R '^(retail_checkpoint_test|n64_tas_checkpoint_test|tas_frontend_test)$' --output-on-failure
```

The checkpoint regression uses an original SI cartridge: 100+200 resumed calls
match 300 uninterrupted calls byte for byte in full state and framebuffer;
absolute capture boundaries, wrong-ROM/truncated/bound rejection and unchanged
inspection files are checked. A separate original-ROM movie gate executes 180
frames through **Application**, Default Input and Default TAS, compares recording
and playback states plus six seek boundaries, and checks actual SI RAM after
an executed neutral-input movie.

The same frontend gate can run a private checkpoint with
`tests/retail_tas_checkpoint_test.py --checkpoint STATE --frames 180 --word 5000c000`;
its other required arguments are shown by `--help` and the CMake original gate.
Use a new private artifact directory, then pass it as `--movie-artifacts` to the
courtyard grader. ROMs, states, movies containing private RAM, framebuffers and
all retail observation files remain under ignored `build/artifacts/` paths.

## Native VI scheduling (2026-10-11)

New native sessions now use VI fields; old S029 checkpoints retain their
100,000-attempt schedule. The raw `retail_core_probe` deliberately defaults to
that old diagnostic budget, even with a new core. To migrate a private checkpoint
explicitly, append `--scheduling video-field`; saved S030 states retain the mode.
Its existing `starting_quantum`, `requested_quanta` and capture-label keys count
frontend calls, which are VI fields in this mode. `executed_attempts` reports the
actual variable CPU budget. The older `n64_retail_probe.py` paired-trace wrapper
continues to probe instruction quanta.

`tests/n64_retail_vi.py` instead checks native VI controls using a standing SM64
US checkpoint, physical RAM plus CPU cache views, matching neutral continuations
and raw tracing for the exact observed attempts with a constant input. It checks
one new field per frontend call. Optional `--movie-artifacts` grades output from
`retail_tas_checkpoint_test.py` using the saved VI-mode `ready.state`; the movie
remains a private local file. `--check-existing` verifies existing observations
against their input/executable fingerprints and repeats the paired raw traces.
The current native movie gate verifies 121 fields and six seeks; these results
support courtyard controls and deterministic replay, not whole-title playability.

## Castle lobby continuation

The [private castle-entry grader](../n64_retail_castle_entry.py) resumes the
64,000-quantum standing checkpoint created above. It drives Mario to Lakitu's
dialogue with an absolute signed-stick schedule, changes to VI fields, advances
the text with spaced A presses, pulls the castle door and verifies the lobby
terrain/camera change through guest RAM. Neutral A and neutral stick controls
run from the same checkpoints. All commercial ROM data, states and frame
captures remain in ignored local artifacts.

```sh
python3 tests/n64_retail_castle_entry.py /path/to/private/sm64-us.z64 \
  --checkpoint build/artifacts/retail/analog/ready.state
```

This verifies castle lobby entry for the pinned US image. It does not grade
main-level progression or complete game compatibility.
