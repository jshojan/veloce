# Optional N64 TMEM reference comparison

This manually run comparison uses Angrylion RDP Plus at commit
`9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc`. The third-party implementation
is fetched into a temporary directory and is not vendored or required by the build.
Run from the repository root with a C/C++ compiler and Git:

```sh
reference_dir=$(mktemp -d /tmp/veloce-tmem-reference.XXXXXX)
git clone https://github.com/ata4/angrylion-rdp-plus.git "$reference_dir"
git -C "$reference_dir" checkout 9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc
cc -std=c11 -O2 -ffunction-sections -fdata-sections \
  -I "$reference_dir/src/core" -c tests/reference/n64-tmem/reference.c \
  -o "$reference_dir/reference.o"
g++ -std=c++17 -O2 -ffunction-sections -fdata-sections \
  -I cores/n64/src tests/reference/n64-tmem/compare.cpp \
  "$reference_dir/reference.o" -Wl,--gc-sections -lm -o "$reference_dir/compare"
"$reference_dir/compare"
```

Observed result: **168,192 comparisons, zero failures**. Each successful command
compares all 4,096 TMEM bytes; the adapter also compares the crash classification.
The RAM pattern, initial TMEM bytes and command parameters are explicit in the
harness. This is a finite software-reference check, not physical-console evidence:

- 103,680 loads: three commands, three source sizes, all four tile sizes/eight
  formats, five TMEM bases, eight source alignments and nine counts.
- 36,864 coordinate cases: all 4,096 encodings, three commands and source sizes,
  with deterministic stride/base/alignment partners.
- 27,648 multirow tile cases: three source sizes, four tile sizes/eight formats,
  four fractional origins, eight row counts, three bases and three strides.

It exposed aligned versus odd-address palette fetch behavior, fraction-three
single-row edge resets and signed coordinate wrapping. These are also covered by
literal regressions in the normal component test. Physical timings, crash side
effects and draw sampling remain outside this comparison.
