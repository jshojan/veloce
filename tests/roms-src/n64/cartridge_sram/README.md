# Original N64 cartridge SRAM corpus

This original MIPS cartridge writes every 64-byte block of 32 KiB and 96 KiB
cartridge SRAM through PI DMA, then reads it back through PI DMA. The guest
records each full 64-byte readback and four PI registers as twelve actual
register words. The independent host oracle grades **2,048 guest DMA cases /
24,576 register words** and all **131,072 final saved bytes**. An executed
wrong-register cartridge and three damaged result files must fail.

The 96 KiB cartridge is archived with its generator and hash in the ROM ledger;
the same generator builds the 32 KiB variant during the test. Neither contains
Nintendo boot or retail bytes. The component gate additionally checks direct
32-bit SRAM I/O, PI bounds/interrupt behavior, known native media IDs, reset,
S036 rollback and malformed-state rejection. The frontend gate reloads 32 KiB
and 96 KiB `.sav` files across sessions and rejects malformed sizes.

This is a functional, settled PI transfer profile. Physical DMA timing,
unaligned/odd-length transfers, SRAM mirroring outside the implemented extent,
FlashRAM and retail game save APIs remain unverified.

```sh
python3 tests/roms-src/build.py n64/cartridge_sram --original-only --check
ctest --test-dir build -R '^n64_(cartridge_sram|sram_)' --output-on-failure
```

Artifacts are under `build/artifacts/owned/n64_cartridge_sram/` and
`build/artifacts/owned/n64_sram_frontend/`. Media IDs follow the
[ares N64 cartridge table](https://github.com/ares-emulator/ares/blob/master/mia/medium/nintendo-64.cpp).
