# Provenance: vendored mgba.c/mgba.h

Source: [`mgba-emu/suite`](https://github.com/mgba-emu/suite), pinned at
commit `e6942030d25ffe3ba76c72b73a86da073ec857cc` (`src/mgba.c`,
`include/mgba.h`). License: MIT (this directory's `LICENSE`, copyright
2015 Jeffrey Pfau); the two source files additionally carry their own
BSD-2-Clause-style header comment from the same author -- both are kept
verbatim, unmodified, byte-for-byte as checked out.

Why vendored instead of built from a `recipe.json` clone-at-SHA like an
upstream *test suite*: this isn't a test ROM, it's a tiny (~60 line) support
library that every devkitARM Veloce ROM links against to reach the mGBA
debug registers from C (`mgba_open()`, `mgba_printf()`,
`mgba_console_open()`), the same way `tests/roms-src/include/fasmarm/` is
support code for FASMARM ROMs, not a test suite of its own. Per section 8 of
the test-ROM plan ("Keep upstream licence files alongside vendored
permissive sources"), the file is committed directly rather than re-cloned
on every `build.py` run.

`tests/roms-src/include/devkitarm/veloce_test.h` wraps `mgba_printf()` in
the VELOCE-RESULT/1 emitter vocabulary (`vt_begin`/`vt_test`/`vt_pass`/
`vt_fail`/`vt_reg`/`vt_log`/`vt_end`); it is Veloce's own code, not part of
this vendored pair.
