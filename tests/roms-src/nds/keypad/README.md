# Original Nintendo DS digital keypad corpus

This MIT-licensed original cartridge captures real guest loads from KEYINPUT
on both CPUs and EXTKEYIN on ARM7. It uses its own instruction encoder and entry
drivers, no SDK, firmware, BIOS, Nintendo logo or commercial ROM. The
[original GBATEK keypad reference](https://doc.kodewerx.org/documents/gbatek.html#dskeypad)
and [BlocksDS input documentation](https://blocksds.skylyrac.net/tutorial/basic/input/)
describe active-low input and ARM7's X/Y access.

The full domain supplies all **4,096 twelve-button combinations** using the
frontend's `INPUT_FILE` schedule. All 4,096 ten-word snapshots matched with zero mismatches. A Gray sequence
changes one button each frame, including opposing directions held together.
ARM7 detects a changed combined keypad value, captures its registers and
signals ARM9 through shared RAM. ARM9 captures its registers and writes both
CPU observations before acknowledging ARM7. This exercises actual frontend
input, plugin frame latching, MMIO loads and shared-memory guest execution.
The ROM carries no expected answers or pass verdict.

| Snapshot word | Observation |
| --- | --- |
| 0–2 | ARM9 KEYINPUT halfword, low byte and high byte |
| 3–5 | ARM7 KEYINPUT halfword, low byte and high byte |
| 6–8 | ARM7 EXTKEYIN halfword, low byte and high byte |
| 9 | Shared-memory sample sequence counter |

The host independently converts each Gray sample to active-low physical bits.
KEYINPUT's low ten bits are graded; the other six register bits are unscored.
EXTKEYIN X/Y, debug, pen and hinge bits are graded; unknown bits 2/4/5 and 8–15
are unscored. The supplied profile fixes debug/pen released and lid open. The
model supplies set bits 2/4/5 and zero bits 8–15 without treating those unknown
values as verified hardware. All unknown bits remain present in raw captures
and the JSON report lists every unscored mask.

The reduced 16-snapshot original ROM must PASS. Its executed wrong-polarity
mutant must FAIL all sixteen. Nine defined-field mutations must each produce
one FAIL; five reserved-bit changes must PASS. Seven damaged streams must
return ERROR: missing/malformed row, wrong domain, false completion, missing or
unterminated trailer and trailing data. File identity, counts, syntax,
completion and sample counter are checked strictly.

Components exercise all 4,096 input masks with irrelevant ABI bits ignored,
both CPU access rights, halfword/byte lanes, ignored read-only writes, unsupported
transactions, reset and saved-input validation. The plugin supplies a twelve-
button layout using the shared A/B/X/Y/L/R/Start/Select/directional ABI controls.
Actual plugin tests verify every layout button, held/released input and exact
save/restore. `VNDS0002` preserves CPU/RAM offsets and appends the normalized
pressed-button latch. Exact legacy `VNDS0001` states load with keys released.

```bash
python3 tests/roms-src/build.py nds/keypad --original-only --check
cmake --build build --target veloce nds_plugin nds_machine_test -j
ctest --test-dir build -R '^nds_(machine_test|keypad)$' --output-on-failure
```

Artifacts remain in `build/artifacts/owned/nds_keypad/` and are uploaded by
owned-hardware CI. Generator/image SHA256 are recorded in the ROM manifest. Full image SHA256:
`358065bfa9edbba285e29a4c402b43cf91c3ec6a210506859173987beac43ba7`.
Output travels through emulator-only byte/word ports; the frontend owns the
register file and trailer. The test uses the supplied `VDSHB001` direct-load,
cache-free profile and bounded frame scheduler, without physical VBlank or
polling-time evidence. Touchscreen/SPI, lid/debug controls, KEYCNT/keypad IRQ,
other register widths, retail boot, physical controller use and DS netplay
remain unverified or unimplemented. Finite input coverage establishes no
whole-platform completion percentage.
