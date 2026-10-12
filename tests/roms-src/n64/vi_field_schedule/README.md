# Original N64 VI field scheduling cartridge

MIT-licensed repository source; no SDK, guest firmware or retail bytes.
`build.py` emits an opt-in original IPL image. Its ten-instruction startup
configures VI; an uncached loop reads VI CURRENT into a GPR and stores it to
physical RAM `$00010000`. It embeds no expected register answers.

The native plugin harness starts at that guest-configured checkpoint and varies
VI inputs through native state. It writes actual register/state/RAM observations
to a file. The independent Python oracle uses line events and integer rational
clock arithmetic to check **5,160 NTSC and 5,160 PAL cases / 92,880 output
words**. The archived PAL variant differs only at cartridge country byte
`$3e` (`P`); both images are generated from this original source. The cases
cover:

- Every horizontal terminal count 0..4095, with partial starting line phases.
- Every vertical terminal count 0..1023, in a separate horizontal profile.
- 40 leap pattern, field parity and partial-field cases.
- Two production frame calls per case, guest CURRENT RAM, instruction attempts,
  field/line/phase/pattern, crystal remainder, frontend count and saved mode.

The harness also checks the largest stable field, blank startup, diagnostic
counter saturation, immediate debug yield/completion, S030 mode serialization,
62 reserved flag rejections, S029 budget import, mode-preserving reset/fresh native load and complete
ordinary/rollback state equality. The core's historical 100k homebrew budget is
preserved; this original native image exercises the new VI default.

An executed NTSC-country variant fails all 5,160 PAL clock expectations.
Other executed controls read WIDTH instead of CURRENT (1,062 rejected NTSC
records), or use the old CPU budget (all 5,160 NTSC records rejected). Nine
independent output faults and seven malformed/identity/order/footer/termination
controls must also fail. A separate region test loads all 12 PAL codes, eight
NTSC codes and two fallback codes in big-endian, byte-swapped and word-swapped
formats: 66 cartridge loads. The production state marker rejects cross-region
and old NTSC-clocked PAL checkpoints without changing NTSC state bytes.

```bash
python3 tests/roms-src/build.py n64/vi_field_schedule --original-only --check
python3 tests/roms-src/build.py n64/vi_field_schedule_pal --original-only --check
cmake --build build --target n64_field_schedule_capture
ctest --test-dir build -R '^n64_vi_field_schedule$' --output-on-failure
```

Files: `build/artifacts/owned/n64_vi_field_schedule/`. This is **scheduler
integration evidence**, not exhaustive horizontal × vertical combinations,
physical CPI/VI timing, VI filtering, PAL retail CIC boot, MPAL or full-title
playability. The PAL video crystal value and cartridge-country mapping follow
[ares N64 system setup](https://github.com/ares-emulator/ares/blob/master/ares/n64/system/system.cpp)
and [cartridge analysis](https://github.com/ares-emulator/ares/blob/master/mia/medium/nintendo-64.cpp).
