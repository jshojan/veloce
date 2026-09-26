# VELOCE-RESULT/1: the test-result channel

Every scored verdict comes from a test ROM that writes the values it observed (registers, flags, counters, cycle counts) to a file that the harness reads. Verdicts never come from stdout/stderr scraping, screenshots or CRCs of the framebuffer.

This document covers the whole path: the ROM-facing port on each console, the plugin ABI that carries bytes from the core to the application, the file the application writes, and the rules the testkit uses to turn that file into a verdict. The files in [`transcripts/`](transcripts/) are reference outputs. The testkit selftest uses them as detector fixtures, so the examples here are checked in CI.

---

## 1. Environment contract

| Variable | Default | Meaning |
|---|---|---|
| `VELOCE_TEST_OUT=<path>` | unset | Create (truncate) `<path>` and write the result file there. Implies `HEADLESS=1` unless `HEADLESS=0` is given explicitly. |
| `VELOCE_TEST_EXIT=0\|1` | `1` | Stop the frame loop after the frame in which the terminator (`END` line or adapter `finish()`) was seen. |
| `VELOCE_TEST_RESETS=<n>` | `3` | Most resets the ROM may request (blargg status `0x81`). One more request ends the run with `reason=reset_limit`. |
| `FRAMES=<n>` | `600` headless | Frame budget only. When it runs out first the trailer says `reason=frames`. The exit code is still 0. |
| `INPUT=frame:hexmask,...` | none | The existing deterministic button schedule, for ROMs that need input. |

The process exit code and stdout/stderr never carry the verdict. `DEBUG=1` is a human aid only.

## 2. File structure

The file is plain ASCII with `\n` line endings. Lines come in two kinds.

**Application lines** start with `#VELOCE`. Only the application writes them. If a ROM line starts with `#`, the sink writes it out as `LOG #...` so a ROM cannot forge metadata.

| Line | When |
|---|---|
| `#VELOCE 1 core=<name> rom_crc32=<hex8> channels=<a,b,...>` | Always the first line. `channels` is empty when the core has no result channel yet. |
| `#VELOCE <key>=<value>` | A core called `note(key, value)`, e.g. `adapter=blargg6000`, `level=4`, `stp=00:8123 cgram0=03e0`. |
| `#VELOCE blob name=<n> bytes=<n> sha256=<hex>` | A core called `blob()`. The bytes are in `<path>.<name>.bin`. |
| `#VELOCE reset n=<k> frame=<f>` | The application performed the k-th ROM-requested reset. |
| `#VELOCE reset_limit=<n>` | A reset was requested after `n` resets had already been done. |
| `#VELOCE end reason=<r> frames=<n> cycles=<n> [status=<s>] [resets=<k>]` | Always the last line, unless the process was killed. `r` is `terminator`, `frames`, `reset_limit` or `quit`. `status` is present once a terminator was seen: 0 is pass, >0 is a fail code, -1 is unknown. |

When a line is written in the middle of a frame, the first token after `#VELOCE` tells the two forms apart. If that token contains `=`, the line is a single note and its value runs to the end of the line. Otherwise the token is a keyword followed by `key=value` tokens.

**ROM lines** are the bytes the ROM emitted, one line each, verbatim except as follows:

- `\r` is stripped.
- Control bytes other than `\t` are dropped.
- Bytes `>= 0x80` become `?`.
- A line is capped at 255 bytes.
- Every completed line is flushed, so a hung or killed run still leaves a parseable prefix.

Space-separated tokens; the first token is the keyword. Hex is lowercase without prefix. Unknown keywords are ignored (forward compatible).

```
VELOCE 1 <console> <suite-id>            suite header
BEGIN <group>                            optional grouping
INFO <key>=<value> ...                   metadata (region=ntsc, model=dmg, first_fail=12, ...)
TEST <id> <name>                         progress marker before a sub-test's checks
CHECK <id> PASS <name>
CHECK <id> FAIL <name> exp=<hex> got=<hex> [mask=<hex>] [at=<hex>] [code=<n>]
REG <name>=<hex> [<name>=<hex> ...]      raw observed state, no verdict
LOG <text>                               free text (blargg console output lands here)
END <pass>/<total> [code=<n>]            terminator; total counts CHECK lines
```

Everything after `PASS`/`FAIL` up to the first `key=value` token is the check name. Only the first `END` counts. When the sink completes an `END` line it treats it as the terminator by itself (status = `code`, else 0 when `pass == total`, else 1). A core that emits `END` lines therefore never has to call `finish()`.

Example (a modified PeterLemon `CPUADC`, SNES, [`transcripts/port_fail_partial.result`](transcripts/port_fail_partial.result)):

```
#VELOCE 1 core=SNES rom_crc32=9b1c02aa channels=port,blargg6000
VELOCE 1 snes cpu.adc
TEST 1 adc_bin8_imm
REG a=1f p=30
CHECK 1 PASS adc_bin8_imm
TEST 2 adc_bin16_imm
CHECK 2 FAIL adc_bin16_imm exp=01a0 got=00a0
CHECK 3 FAIL adc_bin16_psr exp=30 got=31 mask=c3
END 1/3
#VELOCE end reason=terminator frames=7 cycles=2497314 status=1
```

## 3. Verdict rules (`veloce_testkit.detect.detect_result_file`)

These rules apply in order. `pass`/`total` are recomputed from the `CHECK` lines before the first `END`.

1. **The file or header is missing.** The verdict is `ERROR`. If the wall-clock timeout killed the process, the verdict is `TIMEOUT` instead.
2. **`require_channel` is set.** If the file came from an adapter other than the required channel, or the core does not list that channel, the verdict is `SKIP` ("needs modified ROM"). This is how a config demands a Tier A ROM.
3. **There is an `END p/t` line.**
   - `CHECK` lines exist and `p/t` differs from their tally: `ERROR` (tally mismatch, meaning the emitter is broken).
   - `END 0/0`: `ERROR`, unless the test sets `allow_empty` (smoke ROMs), in which case it is `PASS`.
   - `t < expected_checks`: `FAIL` with progress `p / expected_checks` (the ROM died early or skipped checks).
   - `p == t`: `PASS`.
   - Otherwise: `FAIL` with progress `p / t` and `status_code` = `code`, else `t - p`.
   - With no `CHECK` lines, the `END` tally is taken as-is. Legacy adapters emit only `END 1/1` or `END 0/1 code=N`.
4. **There is no `END` line.**
   - `reason=reset_limit`: `FAIL`.
   - `reason=terminator` with a trailer `status`: a core called `finish()` without an `END` line. `status` 0 is `PASS`, >0 is `FAIL`.
   - The trailer is missing (the process was killed) or the harness timed out: `TIMEOUT`.
   - Any `CHECK ... FAIL`: `FAIL`.
   - Otherwise: `RUNS`, which is unscored. The detail says how many checks passed before the frame budget ran out, or that the channel was silent.

Partial credit is `progress = pass/total`, which feeds the existing `score_test()` path. Every `CHECK` is carried into the runner's `--json` output as `checks: [{id, name, status, exp, got, mask, at}]`, so a regression report can say "MMC3 IRQ reload: exp=05 got=04" rather than "hash mismatch".

## 4. Compliance tiers

| Tier | Definition | `channel` values | Scored |
|---|---|---|---|
| **A** | ROM built from patched source emits `VELOCE-RESULT/1` over the console's port | `port` (NES/SNES), `serial` (GB), `mgba` (GBA) | yes; `rom_variant: veloce` |
| **B** | ROM writes structured observed values to a fixed RAM block or hardware port; a core adapter streams or dumps it | `blargg6000` (NES/SNES), `a000`, `hram` (GB), `sram`+`stp` (SNES byuu), `spcport` (SNES gilyon) | yes; adapter recorded via `#VELOCE adapter=` |
| **C** | Core infers a verdict from CPU registers at a stable PC | `r12` (GBA), `mooneye` (GB fingerprint) | bootstrap only; rejected in strict configs |
| n/a | Screenshot or CRC of any kind | `screenshot-crc` | never; lives in `visual_test_suites` |

## 5. Console ports

| Console | Data | Control / probe | On real hardware | Legacy adapters (Tier B/C) |
|---|---|---|---|---|
| NES | `STA $401E` | `$401F` write: `$00` nop, `$01` terminator hint; read returns `$56` (`'V'`) | disabled CPU test registers: writes ignored, reads open bus | `blargg6000` (`$6000` status + `$6004` text streamed as `LOG`, `END`), status `0x81` goes to `request_reset()` |
| SNES | `STA $21FE` (any bank, DMA-able B-bus) | `$21FF` write control, read returns `$56` | unused B-bus register, no-op | `blargg6000`; `stp` (`note stp=`, `blob sram`, `blob cgram`, synthesised `END`); `spcport` APU-port watcher |
| GB | `SB`/`$FF01` + `SC=$81` (every byte, raw) | none | real serial output | `mooneye` (`LD B,B` + Fibonacci fingerprint), `a000` (blargg `$A000` block), `hram` (`$FF80-82` GBMicrotest) |
| GBA | mGBA `0x4FFF600` string + `0x4FFF700` flush (`level \| 0x100`) | `0x4FFF780`: write `0xC0DE`, read `0x1DEA` | inert | `r12` (stable one-instruction loop, R12 = failing test) |

Channel names used in `channels=`, `#VELOCE adapter=` and the config `channel` field: `port`, `serial`, `mgba`, `blargg6000`, `a000`, `hram`, `sram`, `stp`, `spcport`, `mooneye`, `r12`. Per console: NES `port, blargg6000`; SNES `port, blargg6000, stp, sram, spcport`; GB `serial, mooneye, a000, hram`; GBA `mgba, r12`.

## 6. Emitter vocabulary (ROM side)

One include per assembler lives under `tests/roms-src/include/`. Every dialect uses the same vocabulary:

```
VT_BEGIN  suite                          -> "VELOCE 1 <console> <suite>\n"
VT_TEST   id, name                       -> "TEST id name\n"
VT_PASS   id, name                       -> "CHECK id PASS name\n"
VT_FAIL   id, name, exp, got [, mask]    -> "CHECK id FAIL name exp=.. got=..\n"
VT_REG    name, value                    -> "REG name=..\n"
VT_LOG_CH / VT_LOG_STR                   -> raw text (hook the suite's print_char)
VT_END    pass, total                    -> "END p/t\n", then the console's native terminator
```

The primitive is `vt_putc`: `sta $401E` (NES), `sta.w $21FE` (SNES), an `SB`/`SC` write-and-wait (GB), or a line buffer at `0x4FFF600` plus a flush write (GBA). Timing-sensitive ROMs buffer results in RAM and emit after the measured section.

## 7. Core side: adopting the channel (plugin ABI v2)

`include/emu/emulator_plugin.hpp`, `EMU_PLUGIN_API_VERSION 2`:

```cpp
struct ITestSink {
    virtual void write(const char* bytes, size_t n) = 0;          // raw channel bytes
    virtual void finish(int32_t status_code) = 0;                 // adapter terminator: 0 pass, >0 code, -1 unknown
    virtual void note(const char* key, const char* value) = 0;    // "#VELOCE key=value"
    virtual void blob(const char* name, const uint8_t* bytes, size_t n) = 0;  // <out>.<name>.bin
    virtual void request_reset() = 0;                             // blargg 0x81
    void print(const char* text);                                 // write(text, strlen(text))
};

class IEmulatorPlugin {
    virtual bool set_test_sink(ITestSink* sink) { return false; }  // nullptr = detach
    virtual const char* test_channels() const { return ""; }       // "port,blargg6000"
    virtual void on_test_reset() {}                                // after an app-performed reset
};
```

A core adopts the channel in five steps:

1. Keep `ITestSink* m_test_sink = nullptr;` (usually in a small `<core>::TestChannel` next to the bus), set it in `set_test_sink()` and return `true`. The application calls `set_test_sink(nullptr)` before closing the file.
2. Return the drivers it implements from `test_channels()`.
3. Intercept the port write (NES `$401E` in the `$4000-$401F` branch, SNES `$21FE` before the PPU route, GB `SC=$81`, GBA `flush_debug_string()`) and call `m_test_sink->write(&byte, 1)`, or `write(line, len)` followed by `"\n"`. Answer the probe read. Do nothing when `m_test_sink` is null, so behaviour without `VELOCE_TEST_OUT` is unchanged.
4. Adapters call `note("adapter", "<name>")` once, stream the text as `LOG` lines, then emit `END 1/1` or `END 0/1 code=<n>` through `write()`. `finish()` is only for adapters that cannot form a line. For blargg `0x81`, call `request_reset()` once per `0x80`→`0x81` transition and re-arm in `on_test_reset()`.
5. Delete the stdout/stderr verdict prints and any `DEBUG=1` gating of the verdict path. Keep power-on RAM deterministic whenever a sink is attached.

The core never opens files, buffers lines or counts frames. The application owns early exit, the reset delay (3 frames), the reset cap, the 255-byte cap and flushing. The app only calls the v2 methods on plugins whose `get_plugin_api_version()` is at least 2.

## 8. Testkit and config

The testkit handles the result file as follows (`tests/veloce_testkit`):

- **Methods.** `result_detection: "file"` is the target for every scored test. `memory` and `serial` are deprecated aliases. The harness reads the result file first and uses that verdict as soon as the file has one (an `END`, a `CHECK`, or an adapter terminator). Only while a core has not adopted the sink does it fall back to the old stdout parsers. For those aliases alone it still sets `DEBUG=1`, because today's cores print their verdict only under `DEBUG`. The legacy shims are deleted in SH-8.
- **Environment.** The harness always sets `VELOCE_TEST_OUT=<artifacts>/<rom>.result` and `VELOCE_TEST_RESETS=<resets>`. It sets `INPUT=<input>` when the test declares one. It sets `VELOCE_TEST_EXIT=0` only for screenshot tests, which must reach their frame. Stale result and blob files are deleted before each run. On a wall-clock timeout the partial file is still parsed.
- **Per-test config fields:**
  - `channel`: what the ROM reports through; one of the console's channels, or `auto`.
  - `require_channel`: the verdict must come from this channel, otherwise `SKIP`.
  - `rom_variant`: `upstream` or `veloce`. `veloce` requires `rom_build.recipes` to exist.
  - `expected_checks`, `allow_empty`: see section 3.
  - `resets`: default 3.
  - `input`: `frame:hexmask,...`.
- **Top-level config fields:** `result_policy` (`legacy` or `strict`) and `rom_build`.
- **Validator.** Under `strict`, a bare `screenshot-crc` test in `test_suites`, a Tier C channel, or a `memory`/`serial` alias is an error. Under `legacy`, each of these is reported as a warning count.
- **Output.** `runner.py --json` `results[]` entries keep the v1 keys and add:
  - `checks: [{id, name, status, exp, got, mask, at}]`
  - `frames_used`, `end_reason`, `adapter`, `result_path`
  - `source` (`file` / `stdout` / `screenshot` / `trace`)
- **Baseline diff.** `run_all.py --baseline <json>`, or `python -m veloce_testkit.baseline base.json cur.json`, reports:
  - per-test status changes (a PASS that becomes FAIL/TIMEOUT/ERROR counts as a regression; `--no-regressions` gates on it)
  - per-`CHECK` changes
  - `frames_used` drift on tests that pass in both runs

## 9. Reference transcripts

Every file in [`transcripts/`](transcripts/) is exactly what `TestFileSink` writes. [`transcripts/expected.json`](transcripts/expected.json) lists the verdict each one must produce, and `tests/veloce_testkit/selftest.py` checks it.

| File | Shows | Verdict |
|---|---|---|
| `port_pass.result` | Tier A, NES `$401E`, `INFO`/`TEST`/`REG` | PASS 3/3 (FAIL 0.6 with `expected_checks: 5`) |
| `port_fail_partial.result` | failing checks with `exp`/`got`/`mask` | FAIL, progress 1/3 |
| `tally_mismatch.result` | `END 3/3` with two `CHECK` lines | ERROR |
| `no_end_frames.result` | budget ran out mid-suite, all checks passing | RUNS, progress 1.0 |
| `no_end_fail.result` | budget ran out after a failing check | FAIL, progress 0.5 |
| `killed_timeout.result` | process killed, no trailer | TIMEOUT |
| `empty_smoke.result` | `END 0/0` | ERROR, or PASS with `allow_empty` |
| `blargg6000_pass.result` / `_fail` | NES `$6000` adapter | PASS / FAIL code 3; SKIP under `require_channel: port` |
| `blargg6000_reset.result` | `0x81` reset handshake | PASS, 1 reset |
| `reset_limit.result` | ROM keeps asking for reset | FAIL (`reason=reset_limit`) |
| `mooneye_pass.result` | GB fingerprint adapter with `REG` | PASS |
| `r12_fail.result` | GBA R12 adapter | FAIL code 7 |
| `stp_blob_pass.result` | SNES `STP` adapter with `sram`/`cgram` blobs | PASS |
| `mgba_levels.result` | interleaved `level` notes, forged `#VELOCE` demoted to `LOG` | PASS 2/2 |
| `no_channel.result` | core without a channel (today's cores) | RUNS, no verdict |
| `missing_header.result` | no `#VELOCE 1` header | ERROR |
