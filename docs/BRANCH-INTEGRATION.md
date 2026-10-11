# Existing branch reconciliation

The user requested merging existing work into the GitHub project and keeping
CI/CD disabled. These integrations preserve original branch ancestry through
normal pull-request merges. Source changes were reconciled with the newer
owned register/RAM file capture pipeline and verified before shipping.

| Existing branch | Original tip | Integration evidence |
|---|---|---|
| `nes/test-channel` | `e842227ec2658291248507b3451e9ca5475c0545` | [PR 83](https://github.com/jshojan/veloce/pull/83), eight production fixtures and 524,288 independently graded ADC/SBC cases |
| `gb/test-channel` | `ced0be7a32b63ecafd678057214deca6a5192b78` | [PR 84](https://github.com/jshojan/veloce/pull/84), 12 production fixtures, 8,388,608 arithmetic and 73,728 unary cases |
| `gba/test-channel` | `0011328cf9f0b9727b543ad7c90a9036f4787d3a` | [PR 85](https://github.com/jshojan/veloce/pull/85), nine production fixtures and 65,536 result/CPSR pairs |
| `snes/test-channel` | `a507d763c9d33c477682ee08a34ac5de31a8d193` | [PR 86](https://github.com/jshojan/veloce/pull/86), 12 production fixtures, 68,864 settled pairs and 504,832 timed captures |
| `docs/readme-refresh` | `398a1f34b8b8accba7e0c9ddbd5cc17cc480119f` | Original README-only commit reviewed in full; current document supersedes its obsolete status, while useful build/frontend gaps and the CLI crash are reconciled below |

## README refresh reconciliation

The older branch predates additional console plugins, deterministic profile
states, native controller ports, owned original register oracles, file adapters
and functioning TCP rollback netplay. Replacing the current README with its
snapshot would erase those verified deliveries and fixed milestone percentages.
Its history is retained using a merge that preserves the current README, with
these evidence-based updates:

- Build dependencies match `CMakeLists.txt`: C++17, CMake 3.16+, system SDL2
  or FetchContent fallback, Git/network for first configure, Python for tests.
- The application still drives built-in input/audio managers; alternate input/
  audio plugin interfaces and TAS frontend integration remain unfinished.
- Linux local gates are executed; current macOS/Windows runtime validation is
  unverified, despite build configuration being present.
- The old `--help` crash warning was reproduced (exit 139). Early command-line
  exits now skip the uninitialized frame loop, and unknown options return 1.
  Seven real subprocess tests run in an empty directory, with invalid runtime
  environment settings, and verify successful exits create no runtime artifacts.
- Current delivery percentages and source-file evidence remain synchronized.
  CI/CD workflows remain removed and GitHub Actions stays disabled.

## Remaining integration work

`wip/nes-apu-timing` at `d94ce23aee2c31961149dbdfc99f346e4cc0988e`
is explicitly incomplete. Its timing changes require repair and independently
graded original observations before merging. Full hardware corpus coverage,
physical validation and unfinished modern frontend/core features remain part
of the active goal; the branch merges do not establish goal completion.
