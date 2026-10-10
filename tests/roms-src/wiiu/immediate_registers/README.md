# Original Wii U immediate-register corpus

This MIT-licensed, repository-authored `.vwiiu` ROM executes original PowerPC
ADDI/ADDIS programs on three CPU contexts through the separately loadable Wii U
plugin. Source and binary are committed and tracked by the SHA256 ledger.
No external ROM, proprietary firmware or SDK is needed.

## Finite input domain

**25,165,824 captures = 3 contexts × 2 instructions × 4 source patterns ×
2 RA fields × 8 incoming XER combinations × 65,536 immediates.** Source values
are 0, 1, `7fffffff` and `80000000`. RT is fixed to r11; RA is 0 (zero base)
or 3 (ordinary source). All XER.SO/OV/CA combinations carry low bits `7f`.
Four selected initial CR words vary by group. Alias/other register/source
patterns are separate component checks; the ROM does not enumerate all 32-bit
sources or every register field.

Each operation emits nine captured words: result, CR, XER, r3 source, actual
encoding, initial CR, initial XER, sentinel `12345678`, and context tag. A host
integer oracle independently derives signed-immediate addition, high-half
addition and flag preservation. It imports the builder only to reproduce
images and execute controls, not for expected stimulus/result iteration.

Shared MEM2 orders the three output blocks. Each block ends with `BLOCK
complete`; only the global final completion uses `END`. The stream requires
exact `core=Wii_U`/ROM CRC/channel identity, suite/count/width, context order,
every nine-word row, completion, ungraded trailer and EOF. No guest pass count,
stdout verdict or image is scored. The transport ports are synthetic test
conventions, not physical Wii U MMIO.

## Controls and use

Full run: **25,165,824/25,165,824 captures matched**, zero mismatches.

The reduced domain uses eight boundary immediates and all other fields,
3,072 rows total. Executed wrong-result, wrong-ADDI/ADDIS-width and wrong-CR
controls must fail exactly **3,072 / 1,536 / 2,304** rows. Nine field mutations
each fail one row; seven missing/malformed/domain/completion/trailer/EOF cases
produce errors. These controls verify the grader observes actual guest fields.

```sh
python3 tests/roms-src/build.py --original-only --check
python3 tests/roms-src/wiiu/immediate_registers/run.py --controls-only
ctest --test-dir build -R '^wiiu_immediate_registers$' --output-on-failure
```

Artifacts stay in `build/artifacts/owned/wiiu_immediates/`: original/control
images, raw streams (~1.85 GiB full), and JSON grades. CTest timeout is 1,800
seconds. CI/CD remains disabled. This proves the named finite instruction
domain in the supplied profile; full CPU, physical timing, firmware, GPU/audio,
controller hardware and complete platform accuracy remain unverified.

Definitions: [IBM ADDI](https://www.ibm.com/docs/en/ssw_aix_71/assembler/idalangref_cal_inst.html)
and [IBM ADDIS](https://www.ibm.com/docs/en/aix/7.2.0?topic=set-addis-cau-add-immediate-shifted-instruction).
See [core implementation and limits](../../../../cores/wiiu/README.md).
