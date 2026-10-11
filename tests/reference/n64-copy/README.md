# Optional pinned copy-cycle comparison

These original adapters compile unchanged Angrylion RDP Plus at
[`9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc`](https://github.com/ata4/angrylion-rdp-plus/tree/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp).
The clone and compiled implementation stay outside the repository.

```sh
reference_dir=$(mktemp -d /tmp/veloce-copy-reference.XXXXXX)
git clone https://github.com/ata4/angrylion-rdp-plus.git "$reference_dir/source"
git -C "$reference_dir/source" checkout --detach 9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc
cc -std=c11 -O2 -I "$reference_dir/source/src/core" -I "$reference_dir/source/src/core/n64video" -c tests/reference/n64-copy/reference.c -o "$reference_dir/reference.o"
c++ -std=c++17 -O2 -I cores/n64/src tests/reference/n64-copy/compare.cpp "$reference_dir/reference.o" -o "$reference_dir/fetch"
c++ -std=c++17 -O2 -I cores/n64/src tests/reference/n64-copy/draw.cpp "$reference_dir/reference.o" -o "$reference_dir/draw"
"$reference_dir/fetch"
"$reference_dir/draw"
```

Observed zero mismatches:

- **4,194,304 copy fetches:** all 65,536 coupled coordinate seeds, four source
  sizes, eight format fields and both palette modes. Tile shifts, masks, mirrors,
  clamp flags, bases, palettes and bounds vary with the seed; this is a selected
  joint domain, not every combination of those fields or TMEM contents.
- **98,304 copy rectangles/triangles:** 8,192 seeds × three framebuffer sizes ×
  two flips × two shapes. Each compares all 65,536 RAM bytes, all 32,768 hidden
  halfwords and eight retained history lanes. Shapes vary fractional coordinates,
  scissor/field mode, all triangle opcodes, traversal direction, gradients,
  perspective, copy LOD/detail, texture banks and palette state.

The software reference supplies independent functional evidence, not physical
console validation. Random CI8 alpha thresholds, 32-bit framebuffer copy crashes,
DMA timing and complete two-cycle or one-cycle texture LOD remain outside scope.
The ordinary build uses independent literal/arithmetic tests and original guest
register/RAM capture grading without the external renderer.
