# Optional pinned RSP mixed-product comparison

This original adapter compiles unchanged parallel-rsp sources fetched outside
this repository at commit `9ab776a2ce28899769c4f911403234cac62fc799`.
The pinned SIMD implementation supplies the expected register bank and complete
48-bit accumulator for VMUDM, VMUDN, VMADM and VMADN. Third-party source is not
vendored, and this comparison is not required by the normal build.

```sh
reference_dir=$(mktemp -d /tmp/veloce-rsp-mixed-reference.XXXXXX)
git clone https://github.com/libretro/parallel-rsp.git "$reference_dir"
git -C "$reference_dir" checkout 9ab776a2ce28899769c4f911403234cac62fc799
g++ -std=c++17 -O2 -mssse3 -ffunction-sections -fdata-sections \
  -DPRIMARY_RSP_MIXED -I cores/n64/src -I include \
  -I "$reference_dir" -I "$reference_dir/arch/simd/rsp" \
  tests/cpp/n64_rsp_multiply_test.cpp tests/reference/n64-rsp-mixed/reference.cpp \
  cores/n64/src/machine.cpp cores/n64/src/vr4300.cpp \
  "$reference_dir/rsp/vfunctions.cpp" "$reference_dir/rsp/reciprocal.cpp" \
  "$reference_dir/arch/simd/rsp/rsp_core.cpp" -Wl,--gc-sections \
  -o "$reference_dir/compare"
"$reference_dir/compare" tests/roms-src/n64/rsp_multiply_pairs/rsp_multiply_pairs.z64
```

Observed: **6,299,072 comparisons, zero mismatches**. The finite domain includes
all 65,536 VS halfwords with deterministic VT partners, all sixteen element
selectors, every source/destination register triple and seventeen selected
accumulator boundaries. Each comparison checks all 32 vector registers and all
eight accumulator lanes. Other multiply-family instructions run against the
independent arithmetic oracle; they are excluded from the primary count.
The command requires an x86 host with SSSE3. This is software-reference evidence,
not physical-console execution or an exhaustive 2^32 operand-pair corpus.

The hardware author's assembler accepts `(VD, VT, VS)`, as shown in its
[encoding](https://github.com/thelemmy/nemu64-test/blob/main/src/rsp/rsp_assembler.rs).
The published mixed-product fixtures are transcribed using that order. M uses
signed VS × unsigned VT; N uses unsigned VS × signed VT. This corrects the former
operand-order transcription and the old arithmetic oracle that repeated it.
