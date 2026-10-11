# Optional pinned one-cycle rectangle comparison

These repository-owned adapters compile unchanged Angrylion RDP Plus at
[`9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc`](https://github.com/ata4/angrylion-rdp-plus/tree/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp).
Keep its implementation and binaries outside the repository.

```sh
reference_dir=$(mktemp -d /tmp/veloce-rectangle-reference.XXXXXX)
git clone https://github.com/ata4/angrylion-rdp-plus.git "$reference_dir/source"
git -C "$reference_dir/source" checkout --detach 9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc
cc -std=c11 -O2 -ffunction-sections -fdata-sections -I "$reference_dir/source/src/core" -I "$reference_dir/source/src/core/n64video" -c tests/reference/n64-rectangle/reference.c -o "$reference_dir/reference.o"
c++ -std=c++17 -O2 -Wall -Wextra -Werror -Wl,--gc-sections -I cores/n64/src tests/reference/n64-rectangle/compare.cpp "$reference_dir/reference.o" -o "$reference_dir/compare"
"$reference_dir/compare"
```

**32,768 comparisons pass:** 8,192 seeds × two framebuffer sizes × E4/E5.
Each compares every RAM byte, hidden halfword, retained history lane and memory
color latch. Seeds couple fractional rectangle/scissor bounds and field parity,
texture formats/sizes, tile masks/mirrors/shifts/bases, point/three-point/palette/
perspective sampling, ordered dither selectors, alpha/coverage, blender inputs
and primitive depth comparison/update. This selected joint domain does not cover
every Cartesian combination or arbitrary texture contents.

This supplies independent functional software evidence. It does not establish
physical console accuracy, complete one-cycle texture LOD, two-cycle pixels,
random dithering/noise or DMA timing. The normal build uses independent arithmetic
component expectations and original guest RAM/register grading without this
external renderer.
