# Original GameCube comparison and condition-register corpus

Repository-owned MIT-licensed assembler and original marked image. Guest code
executes each instruction and captures registers into `ppcdebug` output files;
the independent host oracle grades every captured bit. No SDK, firmware,
retail assets, expected-answer tables or guest pass verdicts are included.

## Verified domain

**42,108,928/42,108,928 eight-word snapshots matched.**

| Family | Cases | Input domain |
|---|---:|---|
| CMP/CMPL | 131,072 | Two forms, sixteen operand patterns squared, all eight destination CR fields, all eight incoming SO/OV/CA states, four initial CR patterns |
| CMPI/CMPLI | 33,554,432 | Two forms, every 16-bit immediate, sixteen operand patterns, all eight CR fields, both SO states; OV/CA set, initial CR selected by field/SO |
| Eight CR logical operations | 8,388,608 | All 32 destination/source-A/source-B field encodings, four CR patterns, all eight incoming XER flag states |
| MCRF | 2,048 | All eight destination/source fields, four CR patterns, all eight XER flag states |
| MCRXR | 32,768 | All eight fields, all eight XER flag states, every seven-bit byte count, four CR patterns |

Every row contains CR, XER, both preserved source GPRs, the executed instruction
encoding, initial CR/XER seeds and an untouched GPR sentinel. The host uses
signed/unsigned integer order, nibble arithmetic, separate logical truth-table
lookups, field extraction and XER clear rules. It never imports the builder's
input-group iterator or embeds answers in the image. Row count, ROM CRC,
channel/suite/domain identity, completion and ungraded trailer are mandatory.

The ROM contains stimulus groups and an original instruction loop. Immediate
and CR logical encodings advance at runtime; each is stored into the execution
slot before being fetched. The supplied flat-memory profile makes this visible
immediately. Physical instruction caches, synchronization, pipeline timing and
Gekko/Broadway differences are not verified. Images are roughly 4.5 MiB and each
full output is about 2.8 GiB; the manual gate may take several minutes.

Fixed CR patterns do not produce every truth input for every source pair.
Component checks additionally force all defined truth-table inputs with every
source/destination encoding and alias, enumerate every compare immediate,
check every GPR field including GPR0, and exercise every MCRF source/destination
nibble. Across both profiles they match 101,941,248 comparison/condition
snapshots. Plugin fixtures capture known guest CR/XER bytes and compare exact
state replay, including immediately before MCRXR clears the XER flags.

## Failure controls and reproduction

The reduced 864-case original image must pass. Executed corrupted CR capture
must fail all 864, forced signed immediate interpretation must fail 68, and
restoring cleared XER flags must fail eight. Eight individual output-field
mutations fail exactly one row each; seven malformed/truncated/false-verdict
streams return ERROR. JSON reports and raw output remain locally in
`build/artifacts/owned/gc_conditions/`; CI/CD remains disabled.

```sh
python3 tests/roms-src/build.py gc/condition_registers --check
cmake --build build --target veloce gc_plugin ppc_machine_test -j4
ctest --test-dir build -R '^(gc_condition_registers|ppc_machine_test)$' --output-on-failure
```

Image and generator SHA256 are recorded in the original-ROM ledger. This
covers the declared finite domains; 32-bit operand pairs and all CR words are
not exhaustive. See [the implemented profile and remaining hardware](../../../../cores/ppc/README.md).
