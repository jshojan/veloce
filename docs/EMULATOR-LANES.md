# Console worktree lanes

NES, Game Boy, Game Boy Advance, SNES and N64 have separate persistent worktrees
under `.worktrees/`. Each has its own branch and ignored `build/` directory.
The root checkout tracks `main` and is used to review, merge and synchronize
the lanes. `.worktrees/` is ignored by the root checkout; the lane files are
still ordinary git worktrees, not copies or submodules.

| Lane | Worktree | Active branch | Delivery checklist | Next core work |
|---|---|---|---:|---|
| NES | `.worktrees/nes` | `nes/accuracy-lane` | 50% (10/20) | Mapper, open-bus and timing gaps; add register-output ROM evidence for each repaired behavior. |
| Game Boy / Color | `.worktrees/gb` | `gb/accuracy-lane` | 40% (8/20) | Interrupt and timing gaps, result detection and link behavior with independent file grading. |
| GBA | `.worktrees/gba` | `gba/accuracy-lane` | 40% (8/20) | PSR banking and DMA/IRQ behavior with original register-output ROMs. |
| SNES | `.worktrees/snes` | `snes/accuracy-lane` | 40% (8/20) | CPU/timing and PPU/APU gaps with original register-output ROMs. |
| N64 | `.worktrees/n64` | `n64/performance-lane` | 50% (10/20) | SM64 throughput, then wider retail playability and hardware corpus. |

The percentages are the fixed delivery checklist in
[CORE-COMPLETION.md](CORE-COMPLETION.md), not hardware accuracy. A passing
bounded ROM never grants a whole subsystem milestone by itself.

## Working in a lane

Run commands from the repository root, substituting the lane name:

```bash
git -C .worktrees/nes status --short --branch
cmake -S .worktrees/nes -B .worktrees/nes/build -DCMAKE_BUILD_TYPE=Release
cmake --build .worktrees/nes/build -j 4
ctest --test-dir .worktrees/nes/build --output-on-failure -R '^nes_'
```

Use the lane's own build and artifact paths so one console's measurements do
not overwrite another's. Start from the latest `main` for each logical change.
Commit and push only from that lane, open a console-scoped PR, and merge it to
`main` after local gates pass. The root checkout is never committed to directly.
After a PR merges, fast-forward `main` in the root checkout, then bring each
clean lane up to date before its next change. A lane with unfinished work stays
on its branch until that work is committed or intentionally discarded; never
force-reset it to make synchronization convenient.

Core README evidence and the front-page completion block must be updated when
implementation evidence changes. Run `python3 tests/update_completion.py` and
its `--check` mode from the branch that owns those edits. Coordinate the shared
front-page file before concurrent PRs to avoid overwriting another lane's
documentation. Generated ROMs and oracles may be committed; private retail ROMs,
checkpoints and performance profiles stay local.

CI/CD remains disabled. GitHub Actions must remain disabled; use local checks
and put their results in each PR.
