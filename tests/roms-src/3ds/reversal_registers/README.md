# Original 3DS ARM11 reversal register corpus

This MIT-licensed, repository-authored ROM is reproducible from `build.py`.
It runs through the independently loadable 3DS core; ARM9 participates in
completion while both ARM11 contexts execute the instruction corpus.
No firmware, SDK, proprietary game or external ROM is required.

## Finite domain and observations

**25,165,824 captures = 2 contexts × 2 instruction states × 3 operations ×
32 NZCVQ combinations × 65,536 low-half values.** Operations are REV, REV16
and REVSH; input is `low + (65535-low)*65536`. ARM uses condition AL and fixed
Rd=4/Rm=0; Thumb uses the same low registers. Aliases, other ARM conditions,
GE and other full-width patterns have separate component evidence; the ROM
does not enumerate every 32-bit value or every register field.

Each executed operation emits seven actual guest register words:
result, captured CPSR, source, tested encoding, initial CPSR, and sentinels
`12345678` and `89abcdef`. ARM captures CPSR immediately; Thumb returns via BX
before ARM MRS, so T is normalized to zero. NZCVQ preservation is graded;
direct component checks cover Thumb T preservation. The two contexts use
shared RAM to order their blocks; this is not an SMP/cache timing test.

The host `run.py` independently computes byte permutations and signed
halfword conversion. It parses every field and requires exact core/ROM CRC,
channel, suite, ordered block/count/row width, completion, ungraded trailer
and EOF. The raw stream uses `REG r=` followed by seven eight-digit hex words.
Block completion uses `BLOCK complete`, since `END` is reserved for final
transport termination. No guest pass count, stdout verdict or image is graded.

## Controls and reproduction

The reduced 3,072-row domain uses eight low values and every flag/form/context.
Executed wrong-result, wrong-half-width, wrong-sign-extension and wrong-flag
ROMs fail exactly **3,072 / 1,024 / 1,024 / 3,072** rows. Seven single-field
mutations each fail one row; seven malformed/truncated/false-completion or
trailing streams produce errors. Full output matched **25,165,824/25,165,824**
with zero mismatches. Images and source SHA256 are in the shared ROM ledger.

```sh
python3 tests/roms-src/build.py --original-only --check
python3 tests/roms-src/3ds/reversal_registers/run.py --controls-only
ctest --test-dir build -R '^ctr_reversal_registers$' --output-on-failure
```

Artifacts are in `build/artifacts/owned/3ds_reversal/`: original/control images,
raw register streams (about 1.48 GiB full), and JSON grades. CTest timeout is
1,800 seconds. CI/CD is disabled. This proves the specified instruction input
domain in the supplied profile; firmware, GPU/audio, peripherals, physical
memory/timing and full hardware completion are unverified.

Definitions: [ARM DDI0100I, ARM and Thumb REV/REV16/REVSH](https://www.scs.stanford.edu/~zyedidia/docs/arm/armv6.pdf).
See [core implementation and limits](../../../../cores/3ds/README.md).
