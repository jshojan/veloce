# Optional pinned ordered-dither comparison

The original adapter compiles unchanged Angrylion RDP Plus
[`dither.c`](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/dither.c)
fetched outside the repository. No third-party implementation is vendored.

```sh
reference_dir=$(mktemp -d /tmp/veloce-dither-reference.XXXXXX)
curl -fLsS https://raw.githubusercontent.com/ata4/angrylion-rdp-plus/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/dither.c -o "$reference_dir/dither.c"
cc -std=c11 -O2 -I "$reference_dir" -c tests/reference/n64-dither/reference.c -o "$reference_dir/reference.o"
g++ -std=c++17 -O2 -DPRIMARY_DITHER -I cores/n64/src tests/cpp/n64_rdp_dither_test.cpp "$reference_dir/reference.o" -o "$reference_dir/dither-test"
"$reference_dir/dither-test"
```

Observed: zero mismatches in 18,432 periodic/field matrix-seed cases and all
2,048 byte-color/three-bit-seed channel rounds. The same executable also runs
18,874,368 independent alpha/coverage/shade decisions and 27,648 production
RGBA16/RGBA32 triangle captures; those stages use independent arithmetic, not
the compiled primary adapter. Ordered RGB/alpha selectors 0, 1 and 3 are covered.
Random selector 2, RNG/noise state, random alpha thresholds and physical timing
are excluded. The RNG shim is unused for every exposed primary comparison.
