# Original N64 SP control register corpus

This MIT licensed assembler emits an original `V64HB001` cartridge with no
SDK, firmware or commercial bytes. Guest MIPS instructions write SP commands,
read hardware-facing registers, and stream observed values through the debug
register into a host file. The cartridge has no expected results or pass verdict.

All **196,608/196,608 cases** match **1,572,864 guest values**: all 4,096
writable status/interrupt seeds × twelve clear/set command pairs × four command
states. This includes both bits asserted together, which preserves the state.
Every row records status before/after, MI source before/after, two semaphore reads,
PC and CPU Cause. Reserved high command bits are set. Each sample writes a PC
with reserved bits, releases the semaphore using an arbitrary command value,
and enables the actual MI SP mask. The corpus does not enumerate every arbitrary
25-bit multi-pair command or RSP execution state. Component checks additionally
seed BROKE and test its clearing, every low-halfword PC/semaphore write value,
and interrupt delivery/acknowledgment/ERET.

The oracle uses independent arithmetic and bit transitions; it imports no core,
CPU or assembler field helpers. An executed PC-bit mutation fails all 192 reduced
cases. Eight captured-field mutations each fail one row. Seven invalid stream
controls produce ERROR. Identity, domain, widths, count, completion and trailer
validation reject incomplete observations. Raw files/reports are under
`build/artifacts/owned/n64_sp_controls/`; source/image hashes are in the manifest.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_sp_test -j
ctest --test-dir build -R '^n64_(sp_controls|sp_test)$' --output-on-failure
```

The original [RSP programmer guide](https://bukosek.si/hardware/collection/sgi-o2/n64-rsp-programmers-guide.pdf)
defines the CPU interface. The author's [hardware observations](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/registers.rs)
establish simultaneous command and semaphore behavior. Those external tests
were read, not copied or executed here. RSP scalar/vector instructions,
BREAK production, running-PC behavior, IO-full and physical clock validation
remain incomplete. This corpus proves its declared observations, not whole-RSP
accuracy or retail compatibility.

## RSP execution update

The current cartridge waits two CPU NOPs after status commands so single-step
RSP NOP execution settles HALT before sampling, then sets HALT before testing
PC write/read masking. All IMEM bytes are reset NOPs. The independent oracle
accounts for settled single-step HALT; it grades no physical pipeline latency.
The 196,608 rows and negative controls remain intact.
