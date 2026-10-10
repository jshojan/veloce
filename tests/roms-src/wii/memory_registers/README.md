# Original Wii integer memory register corpus

`memory_registers.vwi` contains repository-owned stimuli, generated memory
instructions, loops and register capture. It has no expected answers or guest
pass verdict. Each supplied profile matches **12,586,368/12,586,368 ten-word
observations** through the actual plugin and headless frontend.

## Declared finite domain

| Family | Domain | Rows |
| --- | --- | ---: |
| 14 displacement forms | All aligned signed 16-bit displacements × 8 data patterns × 3 RAM aliases | 12,582,912 |
| 18 indexed forms | 8 full-width index patterns × 8 data patterns × 3 RAM aliases | 3,456 |
| Total | Independently graded observations | 12,586,368 |

Displacement forms are LWZ/LWZU, LBZ/LBZU, STW/STWU, STB/STBU,
LHZ/LHZU, LHA/LHAU and STH/STHU. Indexed forms are LWZX/LWZUX,
LBZX/LBZUX, STWX/STWUX, STBX/STBUX, LHZX/LHZUX, LHAX/LHAUX,
STHX/STHUX and LWBRX/LHBRX/STWBRX/STHBRX.

Data patterns are `00000000,00000001,7fff8000,8000ffff,ffffffff,89abcdef,
5555aaaa,12345678`. Initial memory is the source rotated left eight bits,
XOR `89abcdef`, making both signs of halfword loads observable. RAM aliases
are physical `0`, cached `80000000` and uncached `c0000000`. The effective
target is alias + `00100000`, held fixed while the base is target minus
signed displacement/index modulo 2³². Indexed offsets are `0,-4,4,-32768,
32764,-2147483648,-width,2147483644` (decimal). Word/halfword displacement
low alignment bits are not crossed; every byte displacement is covered.

RT=r11, RA=r3 and RB=r10 are fixed in the original ROM. CR seeds
`00000000,ffffffff,a5a5a5a5,12345678` use `(form-index + data-index +
alias-index + index-pattern-index) % 4`. Incoming SO/OV/CA use that sum
modulo eight, with XER byte count 127. Seeds are selected per group, not
crossed with every input. Guard words immediately before and after the
four-byte target word are `13579bdf` and `2468ace0`.

## Captures and independent host oracle

Each `REG r=` row captures ten words:

1. Final RT (loaded result or preserved store source).
2. Final RA (old base or updated effective address).
3. Displacement/index register r10.
4. CR.
5. XER.
6. Target memory word after execution.
7. Guard word before target.
8. Guard word after target.
9. Original instruction encoding.
10. Initial source value.

The Python host independently enumerates its domain and uses byte arrays,
explicit endian conversion, arithmetic sign extension and modular addressing.
It imports no CPU code or builder stimulus iterator; the builder import is
for image reproduction and executed controls. Every captured bit is graded,
including untouched bytes within partial stores. The stream must have exact
core/ROM CRC/channel identity, suite/domain, row width/count, completion
markers, ungraded status=-1 trailer and EOF.

The positive reduced image has 1,968 rows. Executed wrong-result,
zero-instead-of-sign-extension and missing-base-update mutants fail exactly
1,968, 120 and 768 rows. Ten independent capture-field mutations each fail
one row; seven malformed, truncated, wrong-domain or false-verdict streams
return ERROR. Raw `.result` files (~1.02 GiB each) and JSON reports stay in
`build/artifacts/owned/wii_memory/`.

## Component and replay coverage

Across both profiles **27,560,000** memory-transfer component snapshots
match: **26,356,500 valid transfers** and **1,203,500 invalid-form/access
diagnostics**. The separate C++ oracle operates on bytes, checks complete GPR
arrays, CR/XER/LR/CTR, PC, instruction count and 16 neighboring memory bytes.
It covers the original displacement domain, every RT/RA/RB field on selected
operands, destination/base/index/source aliases, RA=0 special addressing,
ordinary RB=0 reads and full-width index arithmetic. Update loads with RA=0
or RA=RT and update stores with RA=0 diagnose without register/RAM mutation.
Every indexed reserved Rc encoding is also rejected. Mapped unaligned and
unmapped access diagnostics are profile checks, not measured exception behavior.

A known original guest combines STWUX, LHAU, LWBRX and STHBRX and checks
loaded values, updated base and exact memory bytes through both plugins.
Full-frame replay and a machine snapshot immediately before STWUX preserve
RAM and base updates exactly. The full component gate passes undefined-
behavior sanitization. The plugin API's debug byte accessor is limited to a
16-bit address; the replay fixture uses that window, while CPU execution and
serialized state cover the supplied 24 MiB RAM.

```sh
cmake --build build --target veloce gc_plugin wii_plugin ppc_machine_test -j4
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_memory_registers)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
python3 tests/roms-src/wii/memory_registers/run.py --controls-only
```

CI/CD remains disabled; these gates run locally.

## Limits and reference

This finite domain exhausts aligned 16-bit displacements on selected data
patterns, not all 32-bit data/index/base/CR/XER combinations. Original fixed
register-field evidence is separate from component alias coverage. Flat RAM
aliases, generated instructions without cache synchronization and `ppcdebug`
addresses are supplied test conventions. Physical cache/MMU behavior,
alignment/bus exceptions, timing, Gekko/Broadway quirks, retail boot and most
console hardware remain pending; see [core limits](../../../../cores/ppc/README.md).

Instruction encodings, update restrictions and endian rules follow the
[NXP PowerPC instruction set manual](https://www.nxp.com.cn/docs/en/reference-manual/MPC82XINSET.pdf).
That reference does not establish measured console behavior or timing.
