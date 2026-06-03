"""
Shared harness that drives the headless `veloce` binary and applies the right
detection method per test.  Per-console runners import Harness and feed it a
ConsoleConfig; they no longer re-implement subprocess/env/detection plumbing.

Environment contract with the binary (unchanged from existing runners):
  HEADLESS=1          run with no window
  DEBUG=1             emit BLARGG_STATUS / Status code / [GBA] lines, serial echo
  FRAMES=<n>          run n frames then exit
  SAVE_SCREENSHOT=<f> capture framebuffer at frame f (path or frame number)
  TRACE=1             (cpu-trace tests) emit nestest-format instruction trace

Determinism note: tests are run with a fixed FRAMES budget and no wall-clock
dependence in the verdict, so results are reproducible across machines as long
as the binary itself is deterministic (a TAS/netplay requirement anyway).
"""

from __future__ import annotations

import os
import shutil
import subprocess
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from .schema import ConsoleConfig, TestSpec, DetectionMethod
from .detect import (
    TestStatus,
    DetectionResult,
    detect_blargg_memory,
    detect_serial_output,
    detect_gba_register,
    detect_screenshot_crc,
    detect_cpu_trace,
)
from .scoring import score_test, TestPoint


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
    screenshots_dir: Path
    emulator: Optional[Path] = None
    default_frames: int = 1800
    default_timeout: int = 60
    generate_refs: bool = False
    # GBA register protocol is selected when the config declares result_detection
    # "serial" AND console == "gba"; the harness keys off the console.
    console: str = ""

    def __post_init__(self):
        if self.emulator is None:
            self.emulator = find_emulator(self.project_root)
        self.screenshots_dir.mkdir(parents=True, exist_ok=True)


@dataclass
class RunOutput:
    test: TestSpec
    status: TestStatus
    detail: str
    output: str = ""
    exit_code: int = 0
    actual_hash: str = ""          # for screenshot-crc generate-refs flow
    point: Optional[TestPoint] = None


class Harness:
    def __init__(self, config: ConsoleConfig, settings: RunSettings):
        self.config = config
        self.s = settings
        self.s.console = self.s.console or config.console

    # -- single test ------------------------------------------------------
    def run_test(self, test: TestSpec, *, known_fail_override: bool = False) -> RunOutput:
        rom = (self.s.roms_dir / test.file)
        if not rom.exists():
            return self._finish(test, DetectionResult(TestStatus.SKIP, "ROM not found"))

        env = os.environ.copy()
        env["HEADLESS"] = "1"
        env["DEBUG"] = "1"

        screenshot_path = None
        if test.result_detection == DetectionMethod.SCREENSHOT_CRC:
            frames = test.screenshot_frame + 10
            env["FRAMES"] = str(frames)
            safe = str(test.file).replace("/", "_").replace(" ", "_")
            screenshot_path = self.s.screenshots_dir / f"{safe}.png"
            env["SAVE_SCREENSHOT"] = str(screenshot_path)
        trace_path = None
        if test.result_detection != DetectionMethod.SCREENSHOT_CRC:
            env["FRAMES"] = str(test.frames or self.config.frame_limit or self.s.default_frames)
            if test.result_detection == DetectionMethod.CPU_TRACE:
                # The veloce binary's normal startup logging pollutes stdout, so
                # the NES core writes the nestest trace to a dedicated file named
                # by TRACE_FILE. We read that file back for comparison, keeping
                # the trace on a clean channel regardless of other stdout noise.
                env["TRACE"] = "1"
                safe = str(test.file).replace("/", "_").replace(" ", "_")
                trace_path = self.s.screenshots_dir / f"{safe}.trace"
                env["TRACE_FILE"] = str(trace_path)

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
            return self._finish(test, DetectionResult(TestStatus.TIMEOUT, f"timeout {timeout}s"))
        except Exception as e:  # noqa: BLE001
            return self._finish(test, DetectionResult(TestStatus.ERROR, str(e)))

        det = self._detect(test, output, exit_code, screenshot_path, trace_path)
        return self._finish(test, det, output=output, exit_code=exit_code)

    def _detect(
        self, test: TestSpec, output: str, exit_code: int,
        screenshot_path: Optional[Path], trace_path: Optional[Path] = None,
    ) -> DetectionResult:
        m = test.result_detection
        if m == DetectionMethod.MEMORY:
            return detect_blargg_memory(output, exit_code)
        if m == DetectionMethod.SERIAL:
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
        return RunOutput(
            test=test, status=status, detail=det.detail, output=output,
            exit_code=exit_code, actual_hash=actual_hash, point=point,
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
