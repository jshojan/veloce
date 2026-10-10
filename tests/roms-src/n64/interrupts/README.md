# Original N64 interrupt register corpus

This repository-owned ROM exercises CP0 interrupt delivery through the production
N64 plugin and frontend, with no SDK or boot assets. It installs an original
exception handler at `$80000180` and records eight observed values per case:
handler-entry flag, interrupted-instruction marker, Cause, Status, EPC, SI status,
MI pending, and MI mask. The handler captures exception state before returning
with ERET. The ROM includes no expected result or pass verdict. Debug output
ends with an unknown status; the independent Python oracle grades the file.

The full domain has **65,536 cases**, all matched, with 524,288 values:

- Four software interrupt combinations (IP0/IP1).
- SI pending off/on, and MI SI mask off/on (IP2 is the masked device level).
- Settled Count/Compare timer pending off/on (IP7).
- All 256 Status interrupt masks.
- All eight IE/EXL/ERL combinations.

For an eligible interrupt, the probe instruction is suppressed, EPC points to
that instruction, ExcCode is zero, EXL is set, and the handler records the pending
requests. Otherwise, the probe executes, the handler flag remains zero and the
EPC sentinel remains intact. SI pending remains visible in MI even when MI masks
it from CPU IP2. The oracle constructs the gate equation independently; it shares
only the input domain and static instruction address with the generator.

The executed negative ROM writes Cause in place of Status and fails 63 of 64
cases. Seven damaged files (missing/malformed row, wrong domain, false pass,
missing or unterminated trailer, trailing data) return ERROR. The runner retains
raw files and reports in `build/artifacts/owned/n64_interrupts/`. Generator and
image hashes are in `tests/roms-src/rom_manifest.json`. Sources are MIT licensed.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_machine_test -j
ctest --test-dir build -R '^n64_(machine_test|interrupts)$' --output-on-failure
```

Separate CPU component tests cover all 256 pending combinations × 256 masks ×
eight control states × both BD contexts × both BEV vectors: 2,097,152 cases.
External IP3–IP6 are injected at the CPU boundary; their N64 hardware producers
are unimplemented. Component checks also cover read-only Cause fields,
Count wrap/equality/latching, Compare acknowledgement, EPC/ErrorEPC writes,
ERET level priority, persistent SI re-entry and acknowledgement, and restoration
of timer deadlines and pending SI state.

This is exhaustive for the declared logical gates, not the physical N64
interrupt/timing system. External interrupt sources beyond SI, CP0 instruction
hazards/privilege checks, pipeline latency, asynchronous DMA, Count write/equality
hazards, NMI/reset/cache-error paths, physical hardware and retail boot remain
unverified. Count advances every two CPU step attempts (including interrupt
entry) as an explicit approximation, not a verified PClock model. Timer setup
settles before sampling; this ROM does not score exact timer arrival cycles.
