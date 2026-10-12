# Contributing & Branch Workflow

This repo is worked on by multiple people **and multiple AI agents in parallel**
(per-console emulator work, test suites, docs). To keep that from drifting out of
sync, **all changes go through a pull request — nothing is committed directly to
`main`.**

## The rule

- `main` is protected. Never commit or push to it directly.
- Every change — feature, fix, test, docs — lives on a short-lived branch and
  merges via PR.
- One logical change per PR. Different systems (NES / SNES / GB / GBA / shared
  tooling / docs) should be **separate branches and PRs** so parallel work never
  collides.

## One-time setup (per clone / machine)

```bash
git config core.hooksPath .githooks   # enables the pre-push guard below
```

A tracked pre-push hook (`.githooks/pre-push`) refuses direct pushes to `main`
as a local safety net. GitHub branch protection enforces the same rule
server-side.

## Day-to-day flow

```bash
git switch main && git pull            # start from latest main
git switch -c snes/fix-window-clipping # branch:  <system>/<short-desc>
# ...make changes...
git add -A && git commit -m "fix(snes): ..."
git push -u origin snes/fix-window-clipping
gh pr create --fill                    # open the PR
# after local verification and review:
gh pr merge --squash --delete-branch
```

For concurrent NES, GB, GBA, SNES and N64 work, use the persistent
[console worktree lanes](docs/EMULATOR-LANES.md). Each lane keeps its own active
branch and build directory; the root checkout is the integration view.

Branch naming: `nes/…`, `snes/…`, `gb/…`, `gba/…`, `tests/…`, `docs/…`,
`chore/…`. Commit messages follow Conventional Commits
(`fix(snes): …`, `feat(nes): …`, `test(gb): …`, `docs: …`).

## For AI agents

When an agent (or a multi-agent workflow) makes changes:

- **Do the work on a branch, never on `main`.** The orchestrator opens one
  cohesive PR per workstream with `gh pr create` rather than many tiny ones.
- Keep each agent scoped to its own area (its console's `cores/<c>/` or `tests/`)
  so concurrent agents touch disjoint files.
- Use the `gh` CLI to manage work: `gh pr create`, `gh pr status`, `gh pr view`,
  `gh pr checks`, `gh pr merge`.

## Verification while CI/CD is disabled

There are no GitHub workflow files, and GitHub Actions is disabled. Run the
relevant core's original register-output ROMs, component tests and production
integration gates locally before merging. Record the commands and results in
the PR. Keep private retail ROMs, checkpoints and generated profiles out of git.

See [TESTING.md](TESTING.md) for how the suites and the accuracy scorecard work.
