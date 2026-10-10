# Original N64 IPL3 handoff corpus

The MIT licensed Python assembler emits original MIPS code in `ipl3_handoff.z64`,
marked `V64IP001`. The loader copies only bytes `$40..$FFF` to SP DMEM and enters
`$A4000040`, with RDRAM zero. No Nintendo IPL/PIF or commercial bytes are included.
This tests an explicit HLE handoff, not authentication or native boot correctness.

The actual frontend/production plugin writes **12/12 matching 64-bit captures**:
s3=0, s4=1, s5=0, s6=63, s7=1, SP=`FFFFFFFFA4001FF0`,
RA=`FFFFFFFFA4001550`, Status=`34000000`, and PI domain-1 fields 64/18/7/3.
The guest records actual registers without embedding expected outputs or verdicts.
The independent host oracle grades a strict identity/domain/width/completion stream.

An executed seed mutation fails the sole case; twelve field mutations each fail,
and seven invalid streams produce ERROR. Raw observations and reports are in
`build/artifacts/owned/n64_ipl3_handoff/`; source/image hashes are in the manifest.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_pi_test -j
ctest --test-dir build -R '^n64_(pi_test|ipl3_handoff)$' --output-on-failure
```

The handoff covers selected NTSC CIC-6102 parameters only. PIF/CIC execution,
complete hardware reset state, RDRAM initialization, native boot, graphics/audio,
other regions/CIC variants and physical timing remain pending. The local Super
Mario 64 target loads but faults at RI initialization after six IPL3 instructions.
