# Original N64 texture-memory command progression

MIT numeric MIPS assembly; no SDK, firmware or commercial bytes.
Build: `python3 tests/roms-src/build.py n64/rdp_tmem_commands --original-only`.
Run: `python3 tests/roms-src/n64/rdp_tmem_commands/run.py`.

**393,216 cases / 2,752,512 actual 64-bit register/RAM words** cover every repeated
16-bit tile-state payload through both RDRAM and XBUS DMEM, for LOAD_BLOCK,
LOAD_TILE and LOAD_TLUT. The CPU loops over all three load profiles without
tripling the archived table. Texture source is a 16-bit image at a RAM guard.
Tile indices rotate across eight; source state includes every encoded format and
width value. Counts are four texels/entries, origins are zero; block DxT sweeps
all 12-bit values collectively. This is not every possible load geometry.

Guest registers capture CURRENT/STATUS/MI before and after a separately extended
FullSync and an untouched RAM guard. DMEM command lists wrap at 4 KiB. The host
requires exact domain/identity/rows/completion/trailer, grades every word and
rejects an executed capture mutant in all 48 reduced cases, seven individual
field mutations and seven malformed streams. Full run: **33.63 seconds**, zero
mismatches and all controls rejected. Artifacts:
`build/artifacts/owned/n64_rdp_tmem_commands/`.

**TMEM has no direct CPU readback here.** This ROM verifies command consumption,
IRQ and the RAM guard. Byte contents are separately verified by
`n64_rdp_tmem_test`: **665,884 cases**, an independent inverse-bank/scatter oracle,
all destination words, source alignments and descriptor format/size combinations,
all encoded DxT values, block counts, coordinate fields, isolated byte-value
patterns, fractional/multirow tiles, palette profiles and literal interleave,
RGBA32 split-bank and palette checks. DP/XBUS END extension, counters, IRQ,
frozen save replay, S021 conversion preserving descriptors/divider and invalid
state lengths are checked. The finite domain is not all Cartesian products.

Transfers are functional settled operations; physical timing, partial crash
effects, sampling/filtering, palette lookup, textured/triangle rasterization
and native retail compatibility remain unfinished. No physical-console run of
this original is claimed. Four-bit source images and multirow palette loads
remain explicitly blocked crash profiles; legal four-bit textures are uploaded
using a larger source size. Field source:
[SGI RDP Command Summary, tables 7, 8 and 10](https://hcs64.com/files/RDP_COMMANDS.pdf).
