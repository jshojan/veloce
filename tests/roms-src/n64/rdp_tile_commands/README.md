# Original N64 RDP tile command progression

MIT source and reproducible numeric MIPS assembly, with no SDK, firmware or
commercial bytes. Build: `python3 tests/roms-src/build.py n64/rdp_tile_commands --original-only`.
Run: `python3 tests/roms-src/n64/rdp_tile_commands/run.py`.

The finite domain is **131,072 cases / 917,504 actual 64-bit words**: every
16-bit repeated payload through both RDRAM and XBUS DMEM. Lists issue
SET_TEXTURE_IMAGE, SET_TILE and SET_TILE_SIZE; tile indices rotate over all eight.
DMEM lists cross its 4 KiB boundary. Guest registers capture CURRENT, STATUS and
MI pending before and after a separately extended FullSync, plus an untouched
RAM guard. The guest computes no verdict. Host grading requires exact identity,
domain, rows and completion, rejects an executed capture mutant in all 16 reduced
cases, mutates each of seven fields and rejects seven malformed streams.

**Internal texture state has no hardware readback in this ROM.** Its register
captures verify command consumption, real FullSync IRQ and the RAM guard.
`n64_rdp_texture_test` independently checks retention and decoding: **10,107,008
pairs of descriptor fields**, every encoded field value at all eight tile indices,
**1,048,576 DRAM/XBUS state streams**, all input bits, 131,072 isolated coordinate-field cases, raw 10.2 bounds and image
width/address, frozen command replay, S020 conversion preserving divider state,
reserved-state rejection, reset and explicit blocking of unimplemented loads.
The domains do not enumerate every full 56-bit payload or 26-bit image address.

S021 appends 17 words: descriptor/bounds for each tile and the texture image.
Unused command bits are discarded as a deterministic convention; physical
reserved-bit behavior is ungraded. TMEM transfers, coordinate processing,
filtering, sampling, palette loads, textured/triangle rasterization and physical
RDP timing remain pending. This original has not run on a physical console.
Artifacts: `build/artifacts/owned/n64_rdp_tile_commands/`.

Field source: [SGI RDP Command Summary, tables 3, 6 and 9](https://hcs64.com/files/RDP_COMMANDS.pdf).
