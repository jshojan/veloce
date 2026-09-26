#!/usr/bin/env python3
"""
Self-test for the testkit's pure logic (detection parsing + scoring math).
No emulator or ROMs required; safe to run in CI as a fast sanity gate.

  python tests/veloce_testkit/selftest.py
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tests"))

from veloce_testkit.detect import (  # noqa: E402
    detect_blargg_memory, detect_serial_output, detect_gba_register,
    detect_cpu_trace, TestStatus,
)
from veloce_testkit.schema import AccuracyType, Priority  # noqa: E402
from veloce_testkit.scoring import score_test, score_console  # noqa: E402

failures = 0


def check(name: str, cond: bool):
    global failures
    if not cond:
        failures += 1
        print(f"  FAIL: {name}")
    else:
        print(f"  ok:   {name}")


# --- detection ---
check("blargg pass", detect_blargg_memory("BLARGG_STATUS: 0x00", 0).status == TestStatus.PASS)
check("blargg fail", detect_blargg_memory("BLARGG_STATUS: 0x03", 0).status == TestStatus.FAIL)
check("blargg running->runs", detect_blargg_memory("BLARGG_STATUS: 0x80", 0).status == TestStatus.RUNS)
check("serial pass", detect_serial_output("Passed all tests", 0).status == TestStatus.PASS)
check("serial fail", detect_serial_output("Failed #4", 0).status == TestStatus.FAIL)
check("gba pass", detect_gba_register("[GBA] PASSED", 0).status == TestStatus.PASS)
check("gba fail#", detect_gba_register("[GBA] FAILED - Failed at test #7", 0).status_code == 7)


# --- cpu-trace partial credit ---
import tempfile, os  # noqa: E402
golden = "C000  4C F5 C5  JMP $C5F5\nC5F5  A2 00  LDX #$00\nC5F7  86 00  STX $00\nFFFF END"
emitted = "C000  4C F5 C5  JMP $C5F5\nC5F5  A2 00  LDX #$00\nDEAD bad line\nFFFF END"
with tempfile.NamedTemporaryFile("w", suffix=".log", delete=False) as f:
    f.write(golden)
    gpath = f.name
tr = detect_cpu_trace(emitted, Path(gpath))
os.unlink(gpath)
check("trace diverges line 3", tr.status == TestStatus.FAIL and tr.status_code == 3)
check("trace partial progress ~0.5", 0.4 < tr.progress < 0.6)


# --- scoring: rigor weighting ---
# A passing cycle-accurate critical test must outweigh a passing functional low test.
cyc = score_test(test_id="a", subsystem="cpu", accuracy_type=AccuracyType.CYCLE_ACCURATE,
                 priority=Priority.CRITICAL, status=TestStatus.PASS)
fun = score_test(test_id="b", subsystem="cpu", accuracy_type=AccuracyType.FUNCTIONAL,
                 priority=Priority.LOW, status=TestStatus.PASS)
check("cycle-acc weight > functional weight", cyc.weight > fun.weight * 5)

# Known fail excluded from denominator: all-known-fail subsystem -> 0 scored.
kf = score_test(test_id="c", subsystem="apu", accuracy_type=AccuracyType.TIMING,
                priority=Priority.HIGH, status=TestStatus.KNOWN_FAIL)
check("known_fail not scored", kf.scored is False)

# RUNS contributes nothing and is not scored.
runs = score_test(test_id="d", subsystem="ppu", accuracy_type=AccuracyType.VISUAL,
                  priority=Priority.HIGH, status=TestStatus.RUNS)
check("runs not scored", runs.scored is False)

# Console roll-up: one perfect cpu test + one failed apu test.
fail_apu = score_test(test_id="e", subsystem="apu", accuracy_type=AccuracyType.FUNCTIONAL,
                      priority=Priority.MEDIUM, status=TestStatus.FAIL)
card = score_console("nes", [cyc, fail_apu])
# cpu subsystem 100%, apu 0%; importance cpu 1.0 apu 0.55 -> 1.0/(1.55) ~ 0.645
check("console rollup weights importance", 0.60 < card.overall < 0.69)
check("uncovered subsystems flagged", "ppu" in card.uncovered_subsystems)


# ===========================================================================
# VELOCE-RESULT/1 file detector: reference transcripts (docs/testing/transcripts)
# ===========================================================================
import json  # noqa: E402
import shutil  # noqa: E402
import subprocess  # noqa: E402

from veloce_testkit.detect import (  # noqa: E402
    detect_result_file, parse_result_file,
)

REPO = Path(__file__).resolve().parents[2]
TRANSCRIPTS = REPO / "docs" / "testing" / "transcripts"
manifest = json.loads((TRANSCRIPTS / "expected.json").read_text())
check("transcript manifest has cases", len(manifest["cases"]) >= 20)
for case in manifest["cases"]:
    fpath = TRANSCRIPTS / case["file"]
    opts = case.get("options", {})
    det = detect_result_file(fpath, **opts)
    rf = parse_result_file(fpath)
    label = f"{case['file']} {opts or ''}".strip()
    ok = det.status.value == case["status"]
    if rf.checks:
        tally = (sum(c.passed for c in rf.checks), len(rf.checks))
    elif rf.end is not None:
        tally = rf.end
    else:
        tally = (0, 0)
    if "pass" in case:
        ok &= tally == (case["pass"], case["total"])
    if "progress" in case:
        ok &= abs(det.progress - case["progress"]) < 1e-3
    for key in ("end_reason", "frames_used", "adapter"):
        if key in case:
            ok &= getattr(det, key) == case[key]
    if "status_code" in case:
        ok &= det.status_code == case["status_code"]
    if "resets" in case:
        ok &= rf.resets == case["resets"]
    if "blobs" in case:
        ok &= len(rf.blobs) == case["blobs"]
    if "has_verdict" in case:
        ok &= rf.has_verdict == case["has_verdict"]
    if not ok:
        print(f"    got status={det.status.value} tally={tally} progress={det.progress:.4f} "
              f"code={det.status_code} reason={det.end_reason} adapter={det.adapter!r} "
              f"detail={det.detail!r}")
    check(f"transcript {label} -> {case['status']}", ok)

# per-check detail survives parsing
_pf = detect_result_file(TRANSCRIPTS / "port_fail_partial.result")
check("check detail exp/got/mask",
      [(c.id, c.passed, c.exp, c.got, c.mask) for c in _pf.checks] ==
      [("1", True, "", "", ""), ("2", False, "01a0", "00a0", ""), ("3", False, "30", "31", "c3")])
check("check detail in verdict text", "exp=01a0 got=00a0" in _pf.detail)
_ml = parse_result_file(TRANSCRIPTS / "mgba_levels.result")
check("forged #VELOCE in LOG does not become the trailer",
      _ml.trailer.get("reason") == "terminator" and _ml.notes.get("level") == "4")
_nf = parse_result_file(TRANSCRIPTS / "no_end_fail.result")
check("check 'at=' parsed", _nf.checks[1].at == "2002")

# ===========================================================================
# schema v2 fields + validator rules
# ===========================================================================
from veloce_testkit.schema import (  # noqa: E402
    load_config, validate_config, DetectionMethod,
)

_tmp = Path(tempfile.mkdtemp(prefix="veloce_selftest_"))


def _write_cfg(console: str, doc: dict) -> Path:
    root = _tmp / f"repo_{console}_{len(list(_tmp.iterdir()))}"
    (root / "tests").mkdir(parents=True)
    d = root / "cores" / console / "tests"
    d.mkdir(parents=True)
    doc = {"schema_version": 2, "console": console, **doc}
    p = d / "test_config.json"
    p.write_text(json.dumps(doc))
    return p


def _suite(*tests, **kw):
    return {"name": "s", "subsystem": "cpu", "tests": list(tests), **kw}


def _validate(console, doc):
    w: list[str] = []
    e = validate_config(_write_cfg(console, doc), console, w)
    return e, w


_shot = {"file": "a.nes", "result_detection": "screenshot-crc", "expected": "known_fail"}
e, w = _validate("nes", {"test_suites": {"v": _suite(_shot)}})
check("legacy: bare screenshot-crc is a warning", not e and any("screenshot-crc" in x for x in w))
e, w = _validate("nes", {"result_policy": "strict", "test_suites": {"v": _suite(_shot)}})
check("strict: bare screenshot-crc is an error", any("visual_test_suites" in x for x in e))
e, w = _validate("nes", {"result_policy": "strict", "visual_test_suites": {"v": _suite(_shot)}})
check("strict: screenshot-crc under visual_test_suites ok", not e)
e, _ = _validate("gba", {"result_policy": "strict", "test_suites": {"s": _suite(
    {"file": "a.gba", "result_detection": "file", "channel": "r12"})}})
check("strict: Tier C channel rejected", any("Tier C" in x for x in e))
e, _ = _validate("gba", {"test_suites": {"s": _suite(
    {"file": "a.gba", "result_detection": "file", "channel": "r12"})}})
check("legacy: Tier C channel allowed", not e)
e, _ = _validate("nes", {"result_policy": "strict", "test_suites": {"s": _suite(
    {"file": "a.nes", "result_detection": "memory"})}})
check("strict: memory alias rejected", any("deprecated" in x for x in e))
e, _ = _validate("gb", {"test_suites": {"s": _suite(
    {"file": "a.gb", "result_detection": "file", "channel": "port"})}})
check("channel must belong to the console", any("not a gb channel" in x for x in e))
e, _ = _validate("nes", {"test_suites": {"s": _suite(
    {"file": "a.nes", "result_detection": "cpu-trace", "trace_log": "x.log", "channel": "port"})}})
check("channel only on file-family tests", any("sets channel" in x for x in e))
e, _ = _validate("nes", {"test_suites": {"s": _suite(
    {"file": "a.nes", "result_detection": "file", "rom_variant": "veloce"})}})
check("rom_variant veloce needs rom_build.recipes", any("rom_build.recipes" in x for x in e))
_p = _write_cfg("nes", {"rom_build": {"recipes": "tests/roms-src/nes/build.py"},
                        "test_suites": {"s": _suite(
                            {"file": "a.nes", "result_detection": "file", "rom_variant": "veloce",
                             "channel": "port", "require_channel": "port", "expected_checks": 4,
                             "allow_empty": False, "resets": 1, "input": "200:40,201:0"})}})
(_p.parents[3] / "tests" / "roms-src" / "nes").mkdir(parents=True)
(_p.parents[3] / "tests" / "roms-src" / "nes" / "build.py").write_text("")
check("rom_variant veloce with existing recipe ok", validate_config(_p, "nes") == [])
_t = load_config(_p, "nes").suites[0].tests[0]
check("v2 fields parsed",
      (_t.result_detection, _t.channel, _t.require_channel, _t.rom_variant, _t.expected_checks,
       _t.allow_empty, _t.resets, _t.input) ==
      (DetectionMethod.FILE, "port", "port", "veloce", 4, False, 1, "200:40,201:0"))
e, _ = _validate("nes", {"test_suites": {"s": _suite(
    {"file": "a.nes", "result_detection": "file", "expected_checks": -1, "input": "abc"})}})
check("expected_checks / input validated",
      any("expected_checks" in x for x in e) and any("input" in x for x in e))
e, _ = _validate("nes", {"test_suites": {"s": _suite({"file": "a.nes", "result_detection": "bogus"})}})
check("unknown result_detection rejected", any("unknown result_detection" in x for x in e))

# ===========================================================================
# harness: env contract + end-to-end with a fake emulator
# ===========================================================================
from veloce_testkit.harness import Harness, RunSettings  # noqa: E402
from veloce_testkit.runner import result_to_dict  # noqa: E402

_fake = _tmp / "fake_veloce"
_fake.write_text(f"""#!{sys.executable}
# Fake veloce: the "ROM" is JSON telling it what to do.
import json, os, shutil, sys, time
spec = json.load(open(sys.argv[1]))
out = os.environ.get("VELOCE_TEST_OUT")
if spec.get("copy") and out:
    shutil.copyfile(spec["copy"], out)
if spec.get("stderr"):
    sys.stderr.write(spec["stderr"] + "\\n")
if spec.get("dump_env"):
    json.dump(dict(os.environ), open(spec["dump_env"], "w"))
time.sleep(spec.get("sleep", 0))
""")
_fake.chmod(0o755)

_roms = _tmp / "roms"
_roms.mkdir()


def _rom(name: str, **spec) -> str:
    (_roms / name).write_text(json.dumps(spec))
    return name


_hcfg_path = _write_cfg("nes", {"timeout_seconds": 2, "frame_limit": 900, "test_suites": {"s": _suite(
    {"id": "t.file_fail", "file": _rom("file_fail.nes", copy=str(TRANSCRIPTS / "port_fail_partial.result")),
     "result_detection": "file"},
    {"id": "t.alias_stdout", "file": _rom("alias_stdout.nes", copy=str(TRANSCRIPTS / "no_channel.result"),
                                          stderr="BLARGG_STATUS: 0x00"),
     "result_detection": "memory"},
    {"id": "t.alias_file_wins", "file": _rom("alias_file.nes", copy=str(TRANSCRIPTS / "blargg6000_fail.result"),
                                             stderr="BLARGG_STATUS: 0x00"),
     "result_detection": "memory"},
    {"id": "t.no_file", "file": _rom("no_file.nes"), "result_detection": "file"},
    {"id": "t.hang", "file": _rom("hang.nes", copy=str(TRANSCRIPTS / "killed_timeout.result"), sleep=10),
     "result_detection": "file"},
    {"id": "t.env", "file": _rom("env.nes", dump_env=str(_tmp / "env.json")),
     "result_detection": "file", "resets": 1, "input": "10:1", "frames": 77},
)}})
_hcfg = load_config(_hcfg_path, "nes")
_h = Harness(_hcfg, RunSettings(project_root=_tmp, roms_dir=_roms, artifacts_dir=_tmp / "artifacts",
                                emulator=_fake, console="nes"))
_tests = {t.id: t for t in _hcfg.suites[0].tests}

_env, _paths = _h.build_env(_tests["t.file_fail"], base={"DEBUG": "1", "PATH": "/bin"})
check("file test: VELOCE_TEST_OUT set, DEBUG not set",
      _env.get("VELOCE_TEST_OUT", "").endswith("file_fail.nes.result") and "DEBUG" not in _env
      and _env["VELOCE_TEST_EXIT"] == "1" and _env["HEADLESS"] == "1")
_env, _ = _h.build_env(_tests["t.alias_stdout"], base={})
check("memory alias: DEBUG=1 kept for the stdout fallback", _env.get("DEBUG") == "1"
      and "VELOCE_TEST_OUT" in _env)
_shot_t = load_config(_write_cfg("nes", {"test_suites": {"v": _suite(
    {"file": "x.nes", "result_detection": "screenshot-crc", "expected": "known_fail",
     "screenshot_frame": 50})}}), "nes").suites[0].tests[0]
_env, _paths = _h.build_env(_shot_t, base={})
check("screenshot test: full budget (EXIT=0), no DEBUG",
      _env["VELOCE_TEST_EXIT"] == "0" and _env["FRAMES"] == "60" and "DEBUG" not in _env
      and "screenshot" in _paths)

if os.name == "posix":
    _r = _h.run_test(_tests["t.file_fail"])
    check("e2e file: FAIL with per-check detail",
          _r.status == TestStatus.FAIL and len(_r.checks) == 3 and _r.frames_used == 7
          and _r.source == "file" and abs(_r.point.credit - 1 / 3) < 1e-6)
    _d = result_to_dict(_r)
    check("--json entry carries checks/frames_used/result_path",
          _d["checks"][1] == {"id": "2", "name": "adc_bin16_imm", "status": "fail",
                              "exp": "01a0", "got": "00a0"}
          and _d["frames_used"] == 7 and _d["result_path"].endswith(".result")
          and list(_d)[:5] == ["id", "subsystem", "status", "detail", "actual_hash"])
    _r = _h.run_test(_tests["t.alias_stdout"])
    check("e2e memory alias, core without channel: stdout fallback",
          _r.status == TestStatus.PASS and _r.source == "stdout")
    _r = _h.run_test(_tests["t.alias_file_wins"])
    check("e2e memory alias, core with channel: file verdict wins",
          _r.status == TestStatus.FAIL and _r.point is not None and _r.source == "file"
          and _r.adapter == "blargg6000")
    # stale file from an earlier run must not be read
    _stale = _h.artifact_path(_tests["t.no_file"], ".result")
    shutil.copyfile(TRANSCRIPTS / "port_pass.result", _stale)
    _r = _h.run_test(_tests["t.no_file"])
    check("e2e stale result file cleared -> ERROR", _r.status == TestStatus.ERROR)
    _r = _h.run_test(_tests["t.hang"])
    check("e2e timeout keeps partial checks",
          _r.status == TestStatus.TIMEOUT and len(_r.checks) == 2 and "timeout 2s" in _r.detail)
    _h.run_test(_tests["t.env"])
    _seen = json.loads((_tmp / "env.json").read_text())
    check("e2e env: resets/input/frames forwarded",
          (_seen.get("VELOCE_TEST_RESETS"), _seen.get("INPUT"), _seen.get("FRAMES")) == ("1", "10:1", "77"))

# ===========================================================================
# baseline per-check diff + frames_used drift
# ===========================================================================
from veloce_testkit.baseline import diff_documents  # noqa: E402

_base = {"console": "nes", "results": [
    {"id": "a", "status": "pass", "frames_used": 40, "checks": []},
    {"id": "b", "status": "pass", "checks": [{"id": "1", "name": "x", "status": "pass"}]},
    {"id": "c", "status": "fail", "checks": [{"id": "2", "name": "y", "status": "fail", "got": "04"}]},
]}
_cur = {"consoles": {"nes": {"console": "nes", "results": [
    {"id": "a", "status": "pass", "frames_used": 43, "checks": []},
    {"id": "b", "status": "fail", "checks": [{"id": "1", "name": "x", "status": "fail", "got": "01"}]},
    {"id": "c", "status": "fail", "checks": [{"id": "2", "name": "y", "status": "fail", "got": "05"}]},
]}}}
_dd = diff_documents(_base, _cur)[0]
check("baseline: pass->fail is a regression", len(_dd.regressions) == 1 and _dd.regressions[0].startswith("b:"))
check("baseline: per-check changes", len(_dd.check_changes) == 2)
check("baseline: frames_used drift", _dd.frame_drift == ["a: frames_used 40 -> 43"])
check("baseline: identical docs -> empty", diff_documents(_base, _base)[0].is_empty())

shutil.rmtree(_tmp, ignore_errors=True)

print(f"\n{'ALL PASS' if failures == 0 else str(failures) + ' FAILURES'}")
sys.exit(1 if failures else 0)
