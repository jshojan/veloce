# Original N64 complete RDP packet/END-extension cartridge

Original MIT MIPS assembly and native command words; no SDK, firmware or
commercial bytes. Build with `python3 tests/roms-src/build.py n64/rdp_packets
--original-only`; run `python3 tests/roms-src/n64/rdp_packets/run.py`.

## Native command framing domain

**200,704 cases / 1,404,928 actual captured 64-bit words**:
all 256 repeated-byte payloads × four opcode-byte aliases × RDRAM and wrapping
XBUS DMEM × every nonempty partial END split across all eight triangle packet
forms and two texture rectangles. Triangle lengths in 64-bit words are
4, 6, 12, 14, 12, 14, 20 and 22; rectangles are two words. This gives 98 split
profiles × four aliases × two sources × 256 payloads.

The guest builds the complete packet, submits only its prefix, waits for actual
DPC CURRENT to reach END, and captures CURRENT/STATUS/MI pending. It then
extends END, waits for CURRENT again, captures the same native registers and
an actual untouched RDRAM guard. Every coefficient word starts with the byte
for FullSync; none may be treated as a separate command or assert an IRQ.
There are no expected answers in the guest. The independent host length table
checks the actual files, exact domain, each field, completion/trailer and EOF.
The executed capture mutant fails all **72** short cases; seven field and seven
malformed/incomplete stream controls reject.

Complete packets are retained and explicitly blocked when their renderer is
unimplemented. This cartridge proves framing, ingestion and IRQ isolation for
the stated domain; it does not grade triangle pixels, native FIFO depth,
prefetch/raster timing or reserved-opcode electrical behavior. Settled counters,
STATUS, freeze/flush preservation and blocked-command restart are functional
profile conventions pending physical validation.

## Component and production states

The component checks 1,441,792 repeated-halfword coefficient words, 648 partial
header states across all 256 opcode bytes, and 10,240 RDRAM/XBUS streams with
110,592 individual END/word boundaries across every eight-byte DMEM alignment.
It checks full word retention, RAM preservation, no payload FullSync escape,
freeze/flush, incomplete fetches and atomic malformed-state rejection.

`VN64S024` appends a 200-byte canonical partial packet (start/count/needed and
22 words), totaling **8,478,944 bytes**. Unused words are zero; invalid addresses,
counts, expected lengths and unused content reject atomically. S023 and older
states restore a neutral packet while retaining their earlier represented data.
The production netplay ABI gate finds a real partial guest packet checkpoint,
restores it through the fast API and compares actual register output and full
states after replay. Component UBSAN passes with clean compilation.

Layouts follow the [SGI RDP Command Summary](https://hcs64.com/files/RDP_COMMANDS.pdf)
and the RDP author's [command length table](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp.c)
and [partial-list parser](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video.c).
These are primary documentation/implementation references, not physical-console
execution of this original cartridge.

Artifacts: `build/artifacts/owned/n64_rdp_packets/`. Shading, texture lookup,
coverage, blending, depth, complete RDP/boot/playability and timing remain
unfinished. N64 stays 45% delivery completion. CI/CD remains disabled.
