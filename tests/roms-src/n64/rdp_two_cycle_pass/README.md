# Original N64 two-cycle PASS blender observations

This MIT-owned MIPS cartridge issues real RDP commands and writes framebuffer
and DP/MI register values to a hexadecimal result file. It contains no SDK,
commercial ROM bytes, microcode or expected output bytes.

The corpus covers **262,144 guest cases × 13 64-bit words = 3,407,872
observations**: every 8-bit primitive RGB seed and primitive alpha pair, two
second-stage blend profiles, and RDRAM/XBUS command delivery. CC0 forwards
primitive color; CC1 scales it by constant shade RGB (100, 200, 50). The first
blender stage uses `G_RM_PASS`, so BL1 must receive that CC1 color rather than
the distinct CC0 color. BL1 blends against framebuffer memory using primitive
alpha or against a programmed blend color using shade alpha.

The host oracle independently computes CC1, the second blend stage,
quarter-pixel triangle coverage and the 13 captured words: eight framebuffer
pairs, two untouched guards, DP CURRENT/STATUS and MI interrupt state. A
40-case control, executed wrong-first-stage and wrong-capture ROMs, 13 field
corruptions and seven malformed/domain/trailer controls check the grader.

This is a finite mode/geometry domain. Other first-cycle blender sources,
filtering, depth, timing and full two-cycle hardware accuracy remain pending.
The PASS mode is described in the
[Nintendo RDP programming manual](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro12/12-07.html).

```sh
python3 tests/roms-src/build.py n64/rdp_two_cycle_pass --original-only --check
python3 tests/roms-src/n64/rdp_two_cycle_pass/run.py
ctest --test-dir build -R '^n64_rdp_two_cycle_pass$' --output-on-failure
```

Output files are under `build/artifacts/owned/n64_rdp_two_cycle_pass/`.
