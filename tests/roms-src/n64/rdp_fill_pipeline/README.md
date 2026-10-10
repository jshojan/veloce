# Original N64 RDP fill pipeline corpus

MIT numeric MIPS assembly and RDP commands, without SDK, firmware or commercial
bytes. Build with `python3 tests/roms-src/build.py n64/rdp_fill_pipeline --original-only`.
Run with `python3 tests/roms-src/n64/rdp_fill_pipeline/run.py`.

The finite domain contains **294,912 input patterns**: all **12,288 safe low
16-bit mode encodings**, three color sizes (CI8, RGBA16 and RGBA32), RDRAM and
wrapping XBUS DMEM lists, and four geometric profiles. Each list sets a depth
image, redirects the color image to it, clears it to packed maximum depth,
then redirects the color image to a separate framebuffer and fills actual RAM.
FullSync supplies the IRQ. Color/depth storage, three guards, CURRENT, STATUS
and MI pending are captured in two ordered rows per pattern. Each row holds
11 actual 64-bit words, within the result channel's 255-byte line limit.

Thus **589,824 rows / 6,488,064 words** are independently graded. Profiles cover
whole rectangles, the fill span's inclusive truncated X scissor endpoint,
fractional rectangle/scissor bounds, and odd row stride with interlaced
scissoring. The low mode domain excludes image reads, depth compares and
stepped depth updates; these hazardous profiles are separately checked as
explicit atomic blockers by the component. Texture/combiner/blender/dither
selectors bypassed by fill remain present in the submitted mode words.

The component `n64_rdp_fill_test` independently enumerates 1,572,864 fractional
rectangle/scissor/field cases, 196,608 mode cases, 196,608 raw fill-code cases,
all 1,024 strides and all 67,108,864 depth-image addresses. It checks RAM bounds,
address-dependent pattern phase, selected field rows, guard preservation,
actual depth clears, IRQ, frozen replay and rejected/legacy state conversion.
The address sweep proves command retention, not depth comparisons or a
hardware readback of the depth-image register.

The driver requires the exact ROM CRC, suite, domain, every row, completion,
terminator and emulator trailer. Its executed capture mutant must fail all
reduced rows. All 22 captured fields across the two row kinds are mutated,
and seven malformed/incomplete streams must return ERROR. Results live under
`build/artifacts/owned/n64_rdp_fill_pipeline/`.

## Evidence and limits

Command layouts and the depth-clear method follow the [SGI RDP Command
Summary](https://hcs64.com/files/RDP_COMMANDS.pdf) and Nintendo's
[fill-cycle documentation](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro12/12-01.html).
The quarter-span and hazard rules are inferred from the RDP author's primary
[rectangle/edge-walker implementation](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/rasterizer.c).
The oracle independently enumerates quarter scanlines rather than importing
production code. This cartridge has **not** run on a physical console.

The manual gives a broad four-pixel warning for fill scissoring; these gates
characterize the author's detailed span model and do not establish physical
scissor/subpixel accuracy. Hidden coverage/depth bits, exact DMA/raster timing,
partial writes during a hardware pipeline crash, textures, triangles, blending,
combiner arithmetic and actual depth comparisons remain ungraded or missing.
Hazardous profiles and out-of-RAM draws are rejected before writes in this
functional core. Reserved image formats and input-bit behavior are conventions,
not accuracy credit. Delivery completion stays 40% (8/20 milestones).
