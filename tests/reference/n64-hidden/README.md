# Optional N64 hidden RDRAM reference comparison

The adapters compile unchanged primary sources fetched into a temporary directory:

- [ares hidden RAM](https://github.com/ares-emulator/ares/blob/9408cb43d4948fc3ea6e152a307a34348df3fe04/ares/n64/rdram/hidden.hpp), revision `9408cb43d4948fc3ea6e152a307a34348df3fe04`.
- [Angrylion RDP Plus RDRAM](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/rdram.c), revision `9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc`.

No third-party implementation is vendored or required by the normal build.
Run from the repository root with curl, a C compiler and a C++ compiler:

```sh
reference_dir=$(mktemp -d /tmp/veloce-hidden-reference.XXXXXX)
curl -fLsS https://raw.githubusercontent.com/ares-emulator/ares/9408cb43d4948fc3ea6e152a307a34348df3fe04/ares/n64/rdram/hidden.hpp -o "$reference_dir/hidden.hpp"
curl -fLsS https://raw.githubusercontent.com/ata4/angrylion-rdp-plus/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/rdram.c -o "$reference_dir/rdram.c"
cc -std=c11 -O2 -I "$reference_dir" \
  -c tests/reference/n64-hidden/reference.c -o "$reference_dir/reference.o"
g++ -std=c++17 -O2 -I "$reference_dir" -I cores/n64/src \
  tests/reference/n64-hidden/compare.cpp "$reference_dir/reference.o" \
  -o "$reference_dir/hidden-reference-test"
"$reference_dir/hidden-reference-test"
```

Observed: **1,048,576 ares CPU stores**, **245,760 ares EBUS scatter/nibble
transactions**, **786,816 Angrylion RDP history writes**, zero mismatches.
Each RDP comparison checks every hidden halfword, all eight history lanes and
the outstanding delayed address. The CPU comparison checks every hidden
halfword and every word's gathered nibble after each transaction.

CPU inputs combine four initial hidden patterns, every 16-bit seed and all four
store widths. The seed's low four bits independently select the four halfword
LSBs; address selection uses different bits. EBUS covers four widths and every
valid offset in a 32-byte region, every byte seed with complement partner and
sixteen initial pattern/history partners. RDP covers both directions, every byte
value, all lane positions and four initial hidden/history patterns, with pairs,
gaps, repeated addresses, descending traversal and lane wrap. Paired writes cover
all sixteen metadata pairs at all eight word positions and color/depth history
selection.

The RDP adapter initializes explicit hidden values. Angrylion's lazy `HB_CLEAN`
sentinel, used to infer data from externally written RAM, is not part of this
component's representation. Invalid accesses are rejected by the component;
upstream address masking and out-of-range history side effects are not compared.
Ordinary RAM bytes, real bus timing, cached EBUS freezes, MI enable routing,
masked stores, DMA devices, framebuffer rendering and machine snapshots are
outside this standalone comparison. These are finite software-reference checks,
not physical-console measurements or an exhaustive joint machine corpus.
