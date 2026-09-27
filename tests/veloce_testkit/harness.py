"""
Shared harness that drives the headless `veloce` binary and applies the right
detection method per test.  Per-console runners import Harness and feed it a
ConsoleConfig; they no longer re-implement subprocess/env/detection plumbing.

Environment contract with the binary:
  VELOCE_TEST_OUT=<p> ALWAYS set: the app writes the ROM's VELOCE-RESULT/1
                      stream to <artifacts>/<rom>.result (implies HEADLESS)
  VELOCE_TEST_EXIT=1  stop at the terminator (0 for screenshot tests, which need
                      the full budget)
  VELOCE_TEST_RESETS  from the test's "resets" (default 3)
  INPUT=<schedule>    from the test's "input", when set
  HEADLESS=1          run with no window
  FRAMES=<n>          frame budget
  SAVE_SCREENSHOT=<f> capture framebuffer at frame f (path or frame number)
  TRACE=1             (cpu-trace tests) emit nestest-format instruction trace
  DEBUG=1             NOT set, except for the deprecated memory/serial aliases
                      while their stdout fallback is still needed (cores that
                      have not adopted the result channel print their verdict
                      only under DEBUG). Removed with the legacy shims (SH-8).

Verdicts for "file" (and the memory/serial aliases, once the core emits to the
sink) come from the result file only; see docs/testing/VELOCE-RESULT.md.

Determinism note: tests are run with a fixed FRAMES budget and no wall-clock
dependence in the verdict, so results are reproducible across machines as long
as the binary itself is deterministic (a TAS/netplay requirement anyway).
"""

from __future__ import annotations

import glob
import os
import shutil
import subprocess
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from .schema import ConsoleConfig, TestSpec, DetectionMethod, FILE_FAMILY, LEGACY_ALIASES
from .detect import (
    TestStatus,
    DetectionResult,
    CheckResult,
    parse_result_file,
    detect_result_file,
    detect_blargg_memory,
    detect_serial_output,
    detect_gba_register,
    detect_screenshot_crc,
    detect_cpu_trace,
)
from .scoring import score_test, TestPoint
from .rom_manifest import check_rom_variant


def find_emulator(project_root: Path) -> Path:
    for cand in (project_root / "build" / "bin" / "veloce",
                 project_root / "build" / "veloce"):
        if cand.exists():
            return cand
    raise FileNotFoundError(
        "Cannot find veloce binary. Build first: cmake -B build && cmake --build build"
    )


@dataclass
class RunSettings:
    project_root: Path
    roms_dir: Path                      # base dir the test 'file' paths resolve against
    # Per-test outputs: <rom>.result (+ .<blob>.bin), <rom>.png, <rom>.trace
    artifacts_dir: Optional[Path] = None
    emulator: Optional[Path] = None
    default_frames: int = 1800
    default_timeout: int = 60
    generate_refs: bool = False
    # GBA register protocol is selected when the config declares result_detection
    # "serial" AND console == "gba"; the harness keys off the console.
    console: str = ""
    # DEPRECATED alias of artifacts_dir (kept for older runner shims)
    screenshots_dir: Optional[Path] = None

    def __post_init__(self):
        if self.artifacts_dir is None:
            if self.screenshots_dir is None:
                raise ValueError("RunSettings needs artifacts_dir")
            self.artifacts_dir = self.screenshots_dir
        self.artifacts_dir = Path(self.artifacts_dir)
        self.screenshots_dir = self.artifacts_dir
        if self.emulator is None:
            self.emulator = find_emulator(self.project_root)
        self.artifacts_dir.mkdir(parents=True, exist_ok=True)


@dataclass
class RunOutput:
    test: TestSpec
    status: TestStatus
    detail: str
    output: str = ""
    exit_code: int = 0
    actual_hash: str = ""          # for screenshot-crc generate-refs flow
    point: Optional[TestPoint] = None
    # VELOCE-RESULT/1 detail (empty for screenshot-crc / cpu-trace / stdout fallback)
    checks: list[CheckResult] = field(default_factory=list)
    frames_used: int = 0
    result_path: str = ""
    end_reason: str = ""
    adapter: str = ""
    source: str = ""               # "file" | "stdout" | "screenshot" | "trace" | ""


class Harness:
    def __init__(self, config: ConsoleConfig, settings: RunSettings):
        self.config = config
        self.s = settings
        self.s.console = self.s.console or config.console

    # -- per-test environment --------------------------------------------
    def artifact_path(self, test: TestSpec, suffix: str) -> Path:
        safe = str(test.file).replace("/", "_").replace(" ", "_")
        return self.s.artifacts_dir / f"{safe}{suffix}"

    def build_env(self, test: TestSpec, base: Optional[dict] = None) -> tuple[dict, dict]:
        """Environment for one test run. Returns (env, paths) where paths holds
        'result' and, when used, 'screenshot' / 'trace'."""
        env = dict(os.environ if base is None else base)
        for k in ("DEBUG", "VELOCE_TEST_OUT", "VELOCE_TEST_EXIT", "VELOCE_TEST_RESETS",
                  "SAVE_SCREENSHOT", "TRACE", "TRACE_FILE", "INPUT"):
            env.pop(k, None)
        env["HEADLESS"] = "1"
        m = test.result_detection

        paths: dict[str, Path] = {"result": self.artifact_path(test, ".result")}
        env["VELOCE_TEST_OUT"] = str(paths["result"])
        env["VELOCE_TEST_RESETS"] = str(test.resets)
        env["VELOCE_TEST_EXIT"] = "1"
        if test.input:
            env["INPUT"] = test.input
        if m in LEGACY_ALIASES:
            # Transitional: cores without a result channel print their verdict
            # only under DEBUG=1; the stdout fallback in _detect needs it.
            env["DEBUG"] = "1"

        if m == DetectionMethod.SCREENSHOT_CRC:
            env["FRAMES"] = str(test.screenshot_frame + 10)
            env["VELOCE_TEST_EXIT"] = "0"          # the screenshot frame must be reached
            paths["screenshot"] = self.artifact_path(test, ".png")
            env["SAVE_SCREENSHOT"] = str(paths["screenshot"])
        else:
            env["FRAMES"] = str(test.frames or self.config.frame_limit or self.s.default_frames)
            if m == DetectionMethod.CPU_TRACE:
                # The veloce binary's normal startup logging pollutes stdout, so
                # the NES core writes the nestest trace to a dedicated file named
                # by TRACE_FILE. We read that file back for comparison, keeping
                # the trace on a clean channel regardless of other stdout noise.
                env["TRACE"] = "1"
                paths["trace"] = self.artifact_path(test, ".trace")
                env["TRACE_FILE"] = str(paths["trace"])
        return env, paths

    @staticmethod
    def _clear_stale(result_path: Path) -> None:
        # A leftover file from an earlier run must never be read as this run's
        # verdict (including blob dumps "<result>.<name>.bin").
        pattern = glob.escape(result_path.name) + ".*.bin"
        for p in [result_path, *result_path.parent.glob(pattern)]:
            try:
                p.unlink()
            except FileNotFoundError:
                pass

    # -- single test ------------------------------------------------------
    def run_test(self, test: TestSpec, *, known_fail_override: bool = False) -> RunOutput:
        rom = (self.s.roms_dir / test.file)
        if not rom.exists():
            return self._finish(test, DetectionResult(TestStatus.SKIP, "ROM not found"))

        # SH-3b: a rom_variant "veloce" ROM must be reproducible from a
        # recorded build (tests/roms-src/build.py's rom_manifest.json), never
        # scored on faith. rom_variant "upstream" isn't gated here — the
        # golden gate in build.py is what vouches for those.
        skip_reason = check_rom_variant(self.s.project_root, str(test.file), rom, test.rom_variant)
        if skip_reason is not None:
            return self._finish(test, DetectionResult(TestStatus.SKIP, skip_reason))

        env, paths = self.build_env(test)
        result_path = paths["result"]
        self._clear_stale(result_path)

        timeout = self.config.timeout_seconds or self.s.default_timeout
        try:
            proc = subprocess.run(
                [str(self.s.emulator.resolve()), str(rom.resolve())],
                capture_output=True, text=True, timeout=timeout, env=env,
                cwd=str(self.s.project_root),
            )
            output = proc.stdout + proc.stderr
            exit_code = proc.returncode
        except subprocess.TimeoutExpired:
            det = self._detect_timeout(test, result_path, timeout)
            return self._finish(test, det, result_path=result_path)
        except Exception as e:  # noqa: BLE001
            return self._finish(test, DetectionResult(TestStatus.ERROR, str(e)))

        det = self._detect(test, output, exit_code, paths.get("screenshot"), paths.get("trace"),
                           result_path=result_path)
        return self._finish(test, det, output=output, exit_code=exit_code,
                            result_path=result_path)

    def _file_kwargs(self, test: TestSpec) -> dict:
        return dict(expected_checks=test.expected_checks,
                    require_channel=test.require_channel,
                    allow_empty=test.allow_empty)

    def _detect_timeout(self, test: TestSpec, result_path: Path, timeout: int) -> DetectionResult:
        # The sink flushes per line, so a hung ROM still says how far it got.
        if test.result_detection in FILE_FAMILY:
            rf = parse_result_file(result_path)
            if test.result_detection == DetectionMethod.FILE or rf.has_verdict:
                det = detect_result_file(result_path, parsed=rf, timed_out=True,
                                         **self._file_kwargs(test))
                if det.status == TestStatus.TIMEOUT:
                    det.detail = f"timeout {timeout}s; {det.detail}"
                return det
        return DetectionResult(TestStatus.TIMEOUT, f"timeout {timeout}s")

    def _detect(
        self, test: TestSpec, output: str, exit_code: int,
        screenshot_path: Optional[Path], trace_path: Optional[Path] = None,
        *, result_path: Optional[Path] = None,
    ) -> DetectionResult:
        m = test.result_detection
        if m in FILE_FAMILY:
            rf = parse_result_file(result_path) if result_path else None
            if m == DetectionMethod.FILE or (rf is not None and rf.has_verdict):
                if rf is None:
                    return DetectionResult(TestStatus.ERROR, "no result path")
                return detect_result_file(result_path, parsed=rf, exit_code=exit_code,
                                          **self._file_kwargs(test))
            # Deprecated alias and the core has no channel yet: legacy stdout parse.
            if m == DetectionMethod.MEMORY:
                return detect_blargg_memory(output, exit_code)
            if self.s.console == "gba":
                return detect_gba_register(output, exit_code)
            return detect_serial_output(output, exit_code)
        if m == DetectionMethod.SCREENSHOT_CRC:
            return detect_screenshot_crc(
                screenshot_path, test.reference_hash, generate_refs=self.s.generate_refs
            )
        if m == DetectionMethod.CPU_TRACE:
            golden = self.s.roms_dir / test.trace_log
            # Trace lines were written to the dedicated TRACE_FILE; fall back to
            # stdout if the binary emitted them there instead.
            emitted = output
            if trace_path and Path(trace_path).exists():
                emitted = Path(trace_path).read_text(errors="replace")
            return detect_cpu_trace(emitted, golden, limit=test.trace_limit)
        return DetectionResult(TestStatus.ERROR, f"unknown detection '{m}'")

    def _finish(
        self, test: TestSpec, det: DetectionResult,
        output: str = "", exit_code: int = 0,
        result_path: Optional[Path] = None,
    ) -> RunOutput:
        status = det.status
        # Apply expected=known_fail: a real FAIL becomes KNOWN_FAIL (excluded from
        # headline score). PASS stays PASS even if expected known_fail (a fixed bug).
        if status == TestStatus.FAIL and (test.expected == "known_fail"):
            status = TestStatus.KNOWN_FAIL

        point = score_test(
            test_id=test.id,
            subsystem=test.subsystem,
            accuracy_type=test.accuracy_type,
            priority=test.priority,
            status=status,
            progress=det.progress,
            detail=det.detail,
        )
        actual_hash = ""
        if test.result_detection == DetectionMethod.SCREENSHOT_CRC and det.detail.startswith("hash="):
            actual_hash = det.detail.split("=", 1)[1].split()[0]
        m = test.result_detection
        from_file = bool(det.end_reason) or m == DetectionMethod.FILE
        if from_file:
            source = "file"
        elif m in FILE_FAMILY and output:
            source = "stdout"
        elif m == DetectionMethod.SCREENSHOT_CRC:
            source = "screenshot"
        elif m == DetectionMethod.CPU_TRACE:
            source = "trace"
        else:
            source = ""
        return RunOutput(
            test=test, status=status, detail=det.detail, output=output,
            exit_code=exit_code, actual_hash=actual_hash, point=point,
            checks=list(det.checks), frames_used=det.frames_used,
            result_path=str(result_path) if result_path else "",
            end_reason=det.end_reason, adapter=det.adapter, source=source,
        )

    # -- whole config -----------------------------------------------------
    def run_all(self, *, suite_filter: Optional[set[str]] = None) -> list[RunOutput]:
        results: list[RunOutput] = []
        for suite in self.config.suites:
            if suite_filter and suite.id not in suite_filter and suite.subsystem not in suite_filter:
                continue
            for test in suite.tests:
                results.append(self.run_test(test))
        return results
