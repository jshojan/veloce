# Optional combiner arithmetic reference

Uses Angrylion RDP Plus commit `9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc`.
No third-party source is vendored. From the repository root:

```sh
reference_dir=$(mktemp -d /tmp/veloce-combiner-reference.XXXXXX)
git clone https://github.com/ata4/angrylion-rdp-plus.git "$reference_dir"
git -C "$reference_dir" checkout 9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc
cc -std=c11 -O2 -ffunction-sections -fdata-sections   -I "$reference_dir/src/core" -c tests/reference/n64-combiner/reference.c   -o "$reference_dir/reference.o"
g++ -std=c++17 -O3 -DN64_COMBINER_REFERENCE -I cores/n64/src   tests/cpp/n64_rdp_combiner_math_test.cpp "$reference_dir/reference.o"   -Wl,--gc-sections -lm -o "$reference_dir/compare"
"$reference_dir/compare"
```

Observed: **268,173,312 RGB/alpha arithmetic comparisons and 512 clamp
comparisons pass**. This is software-reference evidence for the arithmetic
kernel. It does not test source selectors, pixel coverage, draw scheduling,
keying, dithering, blending, depth testing, rendering or physical-console timing.
See [the arithmetic contract and finite reduction](../../../cores/n64/COMBINER-MATH.md).
