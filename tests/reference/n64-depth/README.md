# Optional N64 depth reference comparison

The adapter compiles unchanged Angrylion RDP Plus
[`zbuffer.c`](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/zbuffer.c)
at revision `9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc`, fetched outside the
repository. No third-party implementation is vendored or required by the normal
build. From the repository root:

```sh
reference_dir=$(mktemp -d /tmp/veloce-depth-reference.XXXXXX)
curl -fLsS https://raw.githubusercontent.com/ata4/angrylion-rdp-plus/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/zbuffer.c -o "$reference_dir/zbuffer.c"
cc -std=c11 -O2 -I "$reference_dir" \
  -c tests/reference/n64-depth/reference.c -o "$reference_dir/reference.o"
g++ -std=c++17 -O2 -DPRIMARY_DEPTH -I cores/n64/src \
  tests/cpp/n64_rdp_depth_test.cpp "$reference_dir/reference.o" \
  -o "$reference_dir/depth-reference-test"
"$reference_dir/depth-reference-test"
```

Observed: zero mismatches across every 18-bit compression input and every 16-bit
stored/decompression/delta encoding. The independent oracle also grades
19,136,512 decisions and 4,194,304 paired stores. The compiled primary directly
compares the **16,777,216 enabled-comparator decisions**: every stored word and
hidden pair, four Z modes, sixteen selected incoming delta/position/coverage/
flag partners. It checks pass, blend enable, overflow, adjusted coverage, current
blender shifts and stored delta code. The 2,359,296 disabled-comparator decisions
and paired stores use the independent host oracle, not the compiled primary.

The adapter presents explicit settled memory values and initializes the primary
lookup tables. Physical bus reads, crash/timing side effects, primitive depth,
interpixel shifter history, production triangle/blender integration and the full
joint operand corpus remain outside this component comparison. Depth stores are
checked to preserve color-write history. These are finite software-reference
checks, not physical-console measurements or retail compatibility evidence.
