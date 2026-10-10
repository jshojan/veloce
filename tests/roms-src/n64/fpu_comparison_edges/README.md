# Original N64 fpu_comparison_edges corpus

Original MIT assembler and observation program; no SDK, firmware or retail bytes.
Every S/D exponent on four cycling fraction patterns, both signs, all predicates and both prior condition bits; special pairs/signs, full RM/enable/FS/condition products on ten profiles, and reserved formats.

**348,384 cases / 2,438,688 captured 64-bit words.** Guest code executes C.cond,
recovers FPE through an original exception handler and records an untouched FPR
guard, FCSR, exception indicator/code, both source FPRs and actual scalar branch
markers from all four BC1 forms. Likely branch annulment is observable in the
marker register. Guest code computes no verdict. A separate exact rational
oracle grades every captured word, including legacy VR4300 NaN encoding and
normal comparison of denormal operands.

An executed capture mutant must fail every reduced case. All seven fields have
individual mutation checks and seven damaged stream forms must return ERROR.
Raw streams and reports: `build/artifacts/owned/n64_fpu_comparison_edges/`. Reproducible
image/generator hashes are committed in the ROM ledger.

```bash
python3 tests/roms-src/build.py n64/fpu_comparison_edges --original-only --check
ctest --test-dir build -R '^n64_fpu_comparison' --output-on-failure
```

Sources and limits: [comparison evidence](../../../../cores/n64/README.md#fpu-single-and-double-comparisons).
These finite original corpora have not run on a physical console. They are not
all full-width operand pairs or physical pipeline/interlock coverage. Odd FR=0
sources use the project's RI convention; reserved formats/unused encoding fields
use its E-assist convention and receive no independent physical accuracy credit.
Retail boot/playability remain unverified. N64 stays 40% on the delivery checklist.
