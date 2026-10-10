# Original N64 VI framebuffer corpus

This MIT licensed assembler emits an original cartridge without SDK, firmware or
commercial bytes. The guest fills RDRAM with two pixel ramps, configures VI,
waits for actual VI/MI interrupt state, captures register values, acknowledges
CURRENT and yields each framebuffer through the production plugin ABI.

The register oracle matches **2 rows / 24 values**: nine configuration readbacks,
MI source, CPU external interrupt pending and post-ack MI state for each format.
An executed WIDTH mutation fails both rows; twelve field and seven framing
controls are rejected. The guest produces no expected-value verdict.

The separate ABI capture writes actual pixel bytes to a file. The independent
host pixel oracle matches **131,072 pixels**: every RGBA16 codeword, including
coverage-bit choices, and 65,536 RGBA32 ramp pixels covering every red/green pair
with a blue permutation. Coverage does not become host transparency; alpha is
255. This is not the full RGBA32 color product. An executed pixel mutation fails
all 131,072 pixels; four channel mutations and six malformed streams are rejected.
The capture also verifies that observing video does not alter guest save state.

Register reports live in `build/artifacts/owned/n64_vi_framebuffer/`; ABI pixel
reports live in `build/artifacts/owned/n64_vi_video/`. Generator and ROM hashes
are in the committed original-ROM ledger.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_video_capture n64_vi_test -j
ctest --test-dir build -R '^n64_(vi_framebuffer|vi_video|vi_test)$' --output-on-failure
```

This grades unfiltered host-observed scanout and functional interrupt scheduling.
Antialiasing, divot/dither/gamma filters, staged line-buffer tests, physical DMA
pipeline timing, mid-frame register changes, extreme-scale errata, RSP/RDP and
retail gameplay remain pending. The 100,000-attempt frontend quantum is not a
hardware VI field. TYPE 1 and out-of-RAM source pixels use documented emulator
blank/black conventions. No physical console has validated this cartridge.
Sources: [VI registers](https://n64brew.dev/wiki/Video_Interface),
[Nintendo VI manual](https://jrra.zone/n64/doc/pro-man/pro30/30-01.htm),
[hardware timing reference](https://github.com/meauxdal/N64-Refresh-Rate-Reference/blob/main/N64_Timing_Reference.md).
