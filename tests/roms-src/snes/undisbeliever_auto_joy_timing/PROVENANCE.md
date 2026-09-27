# Provenance: snes/undisbeliever_auto_joy_timing

Upstream: `github.com/undisbeliever/snes-test-roms.git`, pinned at
`ac6ef8006809c0ff5dabc8ff137a4623967697fe` (current `master`; no dated
release tag covers `src/hardware-tests/auto-joypad/`, only `v20210106`/
`v20210217`, both predating it). zlib licence (`LICENSE`, Copyright ©
2016 Marcus Rowe). Submodule `bass-untech` -> `undisbeliever/bass-untech` at
`9db6088a378061afc7b82f50997a5b9a1d951175`, resolved via `git submodule
update --init --recursive` (`recipe.json`'s `"submodules": true`, added to
`tests/roms-src/build.py` for this recipe) -- confirmed: the clone lands on
the pinned commit and the submodule resolves to the exact commit the
superproject's tree records, matching `tools/rom-toolchain/Dockerfile`'s own
`BASS_UNTECH_SHA` pin exactly, with no extra pin needed for the submodule
itself.

Target ROM: `src/hardware-tests/auto-joypad/auto-joy-timing-test.asm`, the
plan's "auto-joy timing" entry (SNES-8: "`testout.inc` + `AUTOMATED=1` path
for deterministic measurement tests (auto-joy timing, wrmpyb-in-flight,
hdmaen_latch, inidisp_brightness_delay, ipl-speed-test)").

## Build status: BLOCKED -- bass-untech does not run in this toolchain image

Not a problem with this source, this recipe, or the submodule: **the pinned
`bass-untech` binary segfaults on `architecture wdc65816-strict` alone** (the
directive every file under `src/` opens with), before reading any of this
suite's code. Full repro and the (very likely shared) root cause are in
`tests/roms-src/include/PROVENANCE.md` -- `bass-untech` and `ARM9/bass` are
both built from source with the same C++17-compatibility compiler-flag
workaround in `tools/rom-toolchain/Dockerfile`, and both crash identically on
their respective architecture-selection command, with nothing else in common
between the two codebases. Minimal repro inside the image:

```sh
printf 'architecture wdc65816-strict\n' > t.asm && bass-untech t.asm   # segfault, no output
```

`golden_gate: false` in `recipe.json` is not the interesting fact here (`bin/`
is a build-only directory upstream never ships regardless); the real blocker
is that the build command cannot run at all. `tests/roms-src/build.py
snes/undisbeliever_auto_joy_timing` will report `FAILED` until
`tools/rom-toolchain/Dockerfile`'s `bass-untech` stage is fixed.

## What this recipe still proves

The acquisition half of SNES-2's scope for this source: `build.py`'s
clone-at-SHA followed by `git submodule update --init --recursive` resolves
this exact repo + its exact vendored toolchain commit, unattended, from a
depth-1 shallow clone. Whoever picks up SNES-8 does not need to re-solve
that part -- only the toolchain image needs fixing first.

## Expected values

Not authored: blocked on the toolchain fix above. Per the plan (SNES-8),
this and the other undisbeliever measurement tests need "documented expected
values" once `AUTOMATED=1`-style output exists; that is future work for
whoever unblocks and picks up this item.
