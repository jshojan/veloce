# Original N64 Config fields and K0 cache corpus

This archived `config_fields.z64`, MIT assembler and oracle are repository-owned
originals, without SDK/firmware/retail assets or external emulator code. The
shared original assembler is in `../cache_coherence/build.py`. Rebuild with
`python3 build.py`.

## Hardware rule and domain

[NEC VR4300 manual](https://hack64.net/docs/VR43XX.pdf), 5.4.6 and Figure 5-16
(pp.151–153), defines Config as a 32-bit register: bit 31 reads zero, EC30:28 is
read-only, fixed23:16 reads `0x06`, fixed14:4 reads `0x646`, and EP27:24, BE15,
CU3 and K0 bits2:0 are writable. K0=2 bypasses caching; other encodings use the
cache. EP/BE changes have cold-initialization restrictions and downstream
behavior that this register-field suite does not establish.

The native corpus keeps **EP=0 and BE=1** throughout, which avoids changing
these initialization-only fields after stores. Every selected sixteen-bit
source domain at positions 0 and 16, two surrounding patterns, all sixteen
CU/K0 combinations, both MTC0/DMTC0 widths and direct/actual taken-delay
placements are executed: **16,777,216 cases / 201,326,592 actual RAM/register
words**. Source construction excludes EP/BE changes and then assigns CU/K0
explicitly; some sixteen-bit encodings therefore produce the same source.
Upper 64-bit surrounds also verify that DMTC0 does not expand Config's width.
This does not enumerate every possible 64-bit source cross-product.

All code and source tables use uncached KSEG1. Each attempt invalidates its
data line, seeds uncached RAM, writes Config, reads through KSEG0, mutates the
backing RAM through KSEG1, then reads through KSEG0 again. The second read must
observe backing memory only at K0=2; the other settings must retain cached data.
Twelve actual 64-bit words at physical 0x2000 export write encoding, unchanged
source, surround, field position, CU/K0 control, placement, Config word/full
reads, first/second memory observations, Status and sequence. NOP gaps separate
CP0 writes/reads and dependent use. Diagnostic BFF yields delimit records and
are not physical VI fields.

## Capture, independent oracle and controls

```sh
ctest --test-dir build -R '^(n64_config_fields|n64_config_fields_test)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

`n64_integer_trap_capture` loads the production core, captures actual guest RAM
into a ROM-identity-bound `V64TRAP1` file and requires real completion. Twelve
records and complete native states must match ordinary and fast rollback.
The Python oracle independently builds field/source order, named fixed Config
fields and the K0 cache/bypass expectation; it imports only ROM construction.

An executed DMFC0 EPC substitution is rejected in all **4,096 short cases**;
twelve field/sequence mutations and six identity/order/length/completion faults
reject. Core, producer, builder, oracle, shared assembler, recipe and archived
ROM hashes must remain fixed throughout capture. Ignored evidence lives under
`build/artifacts/owned/n64_config_fields/`: full/short/`executed-wrong-read`
`.registers` and JSON reports, field/stream mutations and `provenance.json`.

The component gate checks **7,356,417 observations**: complete selected
sixteen-bit domains at four positions/four surrounds in Kernel32/Kernel64,
all 512 writable-bit patterns and eight EC values, all source allocations and
one-hot/one-clear values, privilege/extension/EXL/ERL/CU0 combinations and delay
metadata. Rejected writes preserve Config/GPRs/LLbit and perform no data access.
EP encodings reserved for downstream transfers are checked only for register
readback, not as physical transfer goldens. The old core fails fixed-field checks.

Reset entry profiles now expose fixed bits and BE=1 with EC=0, using K0=2 for
homebrew and K0=3 for the existing IPL handoff. EC=0 retains the explicit profile;
physical Nintendo board DivMode selection and firmware/cold-reset timing remain
unverified. Existing nonzero Config values in old checkpoints remain unchanged.
No ABI/S031 layout change. Little/reverse endian execution, EP transfer patterns,
physical hazards/cache/timing, Status/reserved CP0 and full CPU/hardware remain
unfinished; N64 stays **50%**.
