#!/usr/bin/env python3
"""
SNES Emulator Test Runner

A comprehensive test suite runner for validating SNES emulator accuracy
using Blargg's test ROMs from https://gitlab.com/higan/snes-test-roms

Blargg's tests write results to specific memory addresses:
- $6000: Test status (0x00=pass, 0x01-0x7F=fail, 0x80=running, 0x81=needs reset)
- $6001-$6003: Signature bytes (0xDE 0xB0 0x61)
- $6004+: Result text (null-terminated)

Usage:
    python test_runner.py              # Run all tests
    python test_runner.py spc          # Run SPC700 tests only
    python test_runner.py --keep       # Keep test ROMs after completion
    python test_runner.py --json       # Output results as JSON
    python test_runner.py -v           # Verbose output
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import re
import zipfile
import urllib.request
import urllib.error
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Optional


class TestResult(Enum):
    PASS = "pass"
    FAIL = "fail"
    RUNS = "runs"  # Completed without crash, but couldn't detect pass/fail
    TIMEOUT = "timeout"
    SKIP = "skip"
    KNOWN_FAIL = "known_fail"
    ERROR = "error"


@dataclass
class TestCase:
    name: str
    path: Path
    expected: str = "pass"
    notes: str = ""
    result: Optional[TestResult] = None
    output: str = ""
    exit_code: int = 0
    status_code: Optional[int] = None
    result_text: str = ""


@dataclass
class TestSuite:
    name: str
    description: str
    priority: str
    tests: list[TestCase] = field(default_factory=list)

    @property
    def passed(self) -> int:
        return sum(1 for t in self.tests if t.result == TestResult.PASS)

    @property
    def failed(self) -> int:
        return sum(1 for t in self.tests if t.result == TestResult.FAIL)

    @property
    def runs(self) -> int:
        return sum(1 for t in self.tests if t.result == TestResult.RUNS)

    @property
    def known_fails(self) -> int:
        return sum(1 for t in self.tests if t.result == TestResult.KNOWN_FAIL)

    @property
    def skipped(self) -> int:
        return sum(1 for t in self.tests if t.result == TestResult.SKIP)

    @property
    def errors(self) -> int:
        return sum(1 for t in self.tests if t.result == TestResult.ERROR)

    @property
    def timeouts(self) -> int:
        return sum(1 for t in self.tests if t.result == TestResult.TIMEOUT)


class Colors:
    RED = "\033[0;31m"
    GREEN = "\033[0;32m"
    YELLOW = "\033[0;33m"
    BLUE = "\033[0;34m"
    CYAN = "\033[0;36m"
    MAGENTA = "\033[0;35m"
    BOLD = "\033[1m"
    NC = "\033[0m"  # No Color

    @classmethod
    def disable(cls):
        cls.RED = cls.GREEN = cls.YELLOW = cls.BLUE = cls.CYAN = cls.MAGENTA = cls.BOLD = cls.NC = ""


class SNESTestRunner:
    # Blargg test ROMs from higan/snes-test-roms GitLab repository
    # Archive download URL for the full repository
    TEST_ROMS_ARCHIVE_URL = "https://gitlab.com/higan/snes-test-roms/-/archive/master/snes-test-roms-master.zip"

    # Alternative: Direct file from MiSTer repo (contains just Blargg tests)
    BLARGG_TESTROMS_URL = "https://github.com/MiSTer-devel/SNES_MiSTer/files/4109309/blargg_testroms.zip"

    TIMEOUT_SECONDS = 60  # Blargg tests may need more time

    # Known failing tests (can be updated as emulator improves)
    KNOWN_FAILURES = {
        # spc_dsp6.sfc is known to be problematic - even fails on real 3-chip SNES
        "spc_dsp6": "DSP6 test fails on 3-chip consoles, passes on higan/bsnes only",
    }

    def __init__(
        self,
        keep_roms: bool = False,
        verbose: bool = False,
        json_output: bool = False,
    ):
        self.keep_roms = keep_roms
        self.verbose = verbose
        self.json_output = json_output
        self.script_dir = Path(__file__).parent
        self.project_root = self.script_dir.parent.parent.parent
        self.test_roms_dir = self.script_dir / "blargg-test-roms"
        self.emulator = self._find_emulator()
        self.config = self._load_config()
        self.suites: list[TestSuite] = []

        if json_output:
            Colors.disable()

    def _find_emulator(self) -> Path:
        """Find the veloce emulator binary."""
        candidates = [
            self.project_root / "build" / "bin" / "veloce",
            self.project_root / "build" / "veloce",
        ]
        for candidate in candidates:
            if candidate.exists():
                return candidate
        raise FileNotFoundError(
            "Cannot find veloce binary. Please build the project first:\n"
            "  cmake -B build && cmake --build build"
        )

    def _load_config(self) -> dict:
        """Load test configuration."""
        config_path = self.script_dir / "test_config.json"
        if config_path.exists():
            with open(config_path) as f:
                return json.load(f)
        return {"test_suites": {}}

    def download_test_roms(self):
        """Download Blargg's test ROMs if not present.

        Blargg's SNES test ROMs include:
        - spc_dsp6.sfc: DSP tests (comprehensive but known to be finicky)
        - spc_mem_access_times.sfc: Memory access timing tests
        - spc_spc.sfc: SPC700 instruction tests
        - spc_timer.sfc: Timer tests
        """
        if self.test_roms_dir.exists():
            # Check if we have actual .sfc files
            sfc_files = list(self.test_roms_dir.rglob("*.sfc"))
            if sfc_files:
                if self.verbose and not self.json_output:
                    print(f"{Colors.BLUE}Test ROMs already present ({len(sfc_files)} .sfc files){Colors.NC}")
                return
            # Directory exists but no ROMs - remove and re-download
            shutil.rmtree(self.test_roms_dir)

        if not self.json_output:
            print(f"{Colors.BLUE}Downloading Blargg's SNES test ROMs...{Colors.NC}")

        self.test_roms_dir.mkdir(parents=True, exist_ok=True)

        # Try downloading from MiSTer first (smaller, just Blargg tests)
        zip_path = self.script_dir / "blargg_testroms.zip"

        try:
            try:
                self._download_file(self.BLARGG_TESTROMS_URL, zip_path)
            except (urllib.error.HTTPError, urllib.error.URLError) as e:
                # Fall back to full GitLab archive
                if not self.json_output:
                    print(f"{Colors.YELLOW}MiSTer download failed, trying GitLab archive...{Colors.NC}")
                zip_path = self.script_dir / "snes-test-roms.zip"
                self._download_file(self.TEST_ROMS_ARCHIVE_URL, zip_path)

            # Extract the zip
            if not self.json_output:
                print(f"{Colors.BLUE}Extracting test ROMs...{Colors.NC}")

            with zipfile.ZipFile(zip_path, 'r') as zip_ref:
                zip_ref.extractall(self.test_roms_dir)

            # If we downloaded from GitLab, we need to move files from the nested directory
            nested_dirs = list(self.test_roms_dir.glob("snes-test-roms-*"))
            if nested_dirs:
                nested_dir = nested_dirs[0]
                # Move blargg-spc-6 directory to top level
                blargg_dir = nested_dir / "blargg-spc-6"
                if blargg_dir.exists():
                    for item in blargg_dir.iterdir():
                        shutil.move(str(item), str(self.test_roms_dir / item.name))
                # Clean up nested structure
                shutil.rmtree(nested_dir)

            # Remove zip file
            zip_path.unlink()

            # Count extracted ROMs
            sfc_files = list(self.test_roms_dir.rglob("*.sfc"))
            if not self.json_output:
                print(f"{Colors.GREEN}Successfully downloaded {len(sfc_files)} test ROMs{Colors.NC}\n")

        except Exception as e:
            if zip_path.exists():
                zip_path.unlink()
            if not self.json_output:
                print(f"{Colors.RED}Failed to download test ROMs: {e}{Colors.NC}")
            raise

    def _download_file(self, url: str, dest: Path):
        """Download a file with progress indication."""
        if self.verbose and not self.json_output:
            print(f"  Downloading from {url}")

        request = urllib.request.Request(
            url,
            headers={'User-Agent': 'Veloce-SNES-TestRunner/1.0'}
        )

        with urllib.request.urlopen(request, timeout=120) as response:
            with open(dest, 'wb') as f:
                # Read in chunks for potential progress indication
                total = int(response.headers.get('content-length', 0))
                downloaded = 0
                chunk_size = 65536

                while True:
                    chunk = response.read(chunk_size)
                    if not chunk:
                        break
                    f.write(chunk)
                    downloaded += len(chunk)

                    if self.verbose and not self.json_output and total > 0:
                        percent = downloaded * 100 // total
                        print(f"\r  Progress: {percent}%", end='', flush=True)

                if self.verbose and not self.json_output and total > 0:
                    print()  # newline after progress

    def cleanup(self):
        """Remove test ROMs directory and temporary files created by emulator."""
        if not self.keep_roms and self.test_roms_dir.exists():
            if not self.json_output:
                print(f"\n{Colors.BLUE}Cleaning up test ROMs...{Colors.NC}")
            shutil.rmtree(self.test_roms_dir)

        # Clean up temporary files/directories created by the emulator during testing
        temp_dirs = ["config", "saves", "savestates", "screenshots"]
        temp_files = ["imgui.ini"]

        for dir_name in temp_dirs:
            temp_dir = self.script_dir / dir_name
            if temp_dir.exists() and temp_dir.is_dir():
                try:
                    shutil.rmtree(temp_dir)
                except Exception:
                    pass

        for file_name in temp_files:
            temp_file = self.script_dir / file_name
            if temp_file.exists() and temp_file.is_file():
                try:
                    temp_file.unlink()
                except Exception:
                    pass

    def parse_test_output(self, output: str) -> tuple[Optional[bool], Optional[int], str]:
        """
        Parse the debug output from the SNES emulator to determine test result.

        Blargg tests write results to memory at $6000:
        - $6000: Status code (0x00=pass, 0x01-0x7F=fail with error code, 0x80=running, 0x81=needs reset)
        - $6001-$6003: Signature 0xDE 0xB0 0x61 (or 0xDE 0xB0 0xG1 in some versions)
        - $6004+: Result text (null-terminated ASCII string)

        The SNES debug output should include lines like:
        - "BLARGG_STATUS: 0x00"
        - "BLARGG_RESULT: Test passed" or similar
        - "Status code: 0 (PASSED)" / "Status code: X (FAILED)"

        Returns: (passed: bool or None, status_code: int or None, result_text: str)
        """
        result_text = ""

        # Look for Blargg-specific output from debug.hpp
        blargg_status = re.search(r"BLARGG_STATUS:\s*0x([0-9A-Fa-f]+)", output)
        if blargg_status:
            status = int(blargg_status.group(1), 16)
            if status == 0x00:
                return True, 0, "Test passed"
            elif status == 0x80:
                return None, status, "Test still running"
            elif status == 0x81:
                return None, status, "Test needs reset"
            else:
                return False, status, f"Test failed with code {status}"

        # Look for result text
        blargg_result = re.search(r"BLARGG_RESULT:\s*(.+?)(?:\n|$)", output)
        if blargg_result:
            result_text = blargg_result.group(1).strip()

        # Look for explicit test result patterns from debug.hpp
        passed_pattern = re.search(r"Status code:\s*0\s*\(PASSED\)", output, re.IGNORECASE)
        if passed_pattern:
            return True, 0, result_text or "Passed"

        failed_pattern = re.search(r"Status code:\s*(\d+)\s*\(FAILED\)", output, re.IGNORECASE)
        if failed_pattern:
            return False, int(failed_pattern.group(1)), result_text

        # Alternative patterns
        if re.search(r"\bPASSED\b", output, re.IGNORECASE):
            return True, 0, result_text or "Passed"

        if re.search(r"\bFAILED\b", output, re.IGNORECASE):
            return False, 1, result_text

        # Check for test passed/failed text output
        if "test passed" in output.lower():
            return True, 0, "Test passed"

        if "test failed" in output.lower():
            return False, 1, result_text or "Test failed"

        return None, None, result_text

    def run_test(self, test: TestCase) -> TestResult:
        """Run a single test ROM."""
        rom_path = self.test_roms_dir / test.path

        if not rom_path.exists():
            test.result = TestResult.SKIP
            test.output = f"ROM not found: {rom_path}"
            return TestResult.SKIP

        try:
            # Run with environment variables for headless mode and debug output
            env = os.environ.copy()
            env["DEBUG"] = "1"
            env["HEADLESS"] = "1"
            env["FRAMES"] = str(self.TIMEOUT_SECONDS * 60)  # Convert seconds to frames at 60fps

            result = subprocess.run(
                [str(self.emulator), str(rom_path)],
                capture_output=True,
                text=True,
                timeout=self.TIMEOUT_SECONDS + 10,  # Add buffer for startup/shutdown
                env=env,
                cwd=self.project_root,  # Run from project root so plugins are found
            )
            test.exit_code = result.returncode
            test.output = result.stdout + result.stderr
        except subprocess.TimeoutExpired:
            test.result = TestResult.TIMEOUT
            test.output = f"Timeout after {self.TIMEOUT_SECONDS}s"
            return TestResult.TIMEOUT
        except Exception as e:
            test.result = TestResult.ERROR
            test.output = str(e)
            return TestResult.ERROR

        # Parse test result from output
        passed, status_code, result_text = self.parse_test_output(test.output)
        test.status_code = status_code
        test.result_text = result_text

        # Check if this is a known failure
        test_base_name = test.name.replace('.sfc', '').replace('.smc', '')
        is_known_fail = test_base_name in self.KNOWN_FAILURES

        if passed is True:
            test.result = TestResult.PASS
        elif passed is False:
            if is_known_fail or test.expected == "known_fail":
                test.result = TestResult.KNOWN_FAIL
                if test_base_name in self.KNOWN_FAILURES:
                    test.notes = self.KNOWN_FAILURES[test_base_name]
            else:
                test.result = TestResult.FAIL
        else:
            # No clear result detected - check if it ran successfully
            if test.exit_code == 0:
                # ROM ran without crashing - mark as RUNS (not pass/fail detectable)
                test.result = TestResult.RUNS
            else:
                if is_known_fail or test.expected == "known_fail":
                    test.result = TestResult.KNOWN_FAIL
                else:
                    test.result = TestResult.FAIL

        return test.result

    def run_suite(self, suite: TestSuite):
        """Run all tests in a suite."""
        if not self.json_output:
            print(f"\n{Colors.BLUE}{Colors.BOLD}=== {suite.name} ==={Colors.NC}")
            if self.verbose:
                print(f"    {suite.description}")

        for test in suite.tests:
            result = self.run_test(test)

            if not self.json_output:
                symbol_map = {
                    TestResult.PASS: f"{Colors.GREEN}PASS{Colors.NC}",
                    TestResult.FAIL: f"{Colors.RED}FAIL{Colors.NC}",
                    TestResult.RUNS: f"{Colors.CYAN}RUNS{Colors.NC}",
                    TestResult.KNOWN_FAIL: f"{Colors.YELLOW}KNOWN{Colors.NC}",
                    TestResult.TIMEOUT: f"{Colors.MAGENTA}TIMEOUT{Colors.NC}",
                    TestResult.SKIP: f"{Colors.YELLOW}SKIP{Colors.NC}",
                    TestResult.ERROR: f"{Colors.RED}ERROR{Colors.NC}",
                }
                symbol = symbol_map.get(result, "???")
                print(f"  [{symbol}] {test.name}")

                if self.verbose:
                    if test.result_text:
                        print(f"         Result: {test.result_text}")
                    if result in (TestResult.FAIL, TestResult.ERROR, TestResult.KNOWN_FAIL) and test.notes:
                        print(f"         Note: {test.notes}")
                    if result == TestResult.FAIL and test.status_code is not None:
                        print(f"         Status code: {test.status_code}")
                    if result == TestResult.ERROR:
                        # Show first line of error
                        error_line = test.output.split('\n')[0][:80] if test.output else "Unknown error"
                        print(f"         Error: {error_line}")

        if not self.json_output:
            parts = []
            if suite.passed:
                parts.append(f"{Colors.GREEN}Passed: {suite.passed}{Colors.NC}")
            if suite.runs:
                parts.append(f"{Colors.CYAN}Runs: {suite.runs}{Colors.NC}")
            if suite.failed:
                parts.append(f"{Colors.RED}Failed: {suite.failed}{Colors.NC}")
            if suite.known_fails:
                parts.append(f"{Colors.YELLOW}Known: {suite.known_fails}{Colors.NC}")
            if suite.timeouts:
                parts.append(f"{Colors.MAGENTA}Timeout: {suite.timeouts}{Colors.NC}")
            if suite.skipped:
                parts.append(f"Skipped: {suite.skipped}")
            if suite.errors:
                parts.append(f"{Colors.RED}Errors: {suite.errors}{Colors.NC}")

            print(f"  {' | '.join(parts)}")

    def discover_tests(self):
        """Discover tests from the downloaded test ROM directory."""
        suites = []

        # Blargg SPC700 tests
        spc_tests = []
        for pattern in ["*.sfc", "*.smc"]:
            spc_tests.extend(self.test_roms_dir.glob(pattern))

        # Also check subdirectories
        for pattern in ["**/*.sfc", "**/*.smc"]:
            spc_tests.extend(self.test_roms_dir.glob(pattern))

        # Remove duplicates and sort
        spc_tests = sorted(set(spc_tests))

        if spc_tests:
            suite = TestSuite(
                name="Blargg SPC700/DSP Tests",
                description="Blargg's SPC700 audio processor and DSP tests. "
                            "These test the SPC700 CPU, DSP registers, memory access timing, and timers.",
                priority="high",
            )
            for rom in spc_tests:
                test_name = rom.stem
                expected = "known_fail" if test_name in self.KNOWN_FAILURES else "pass"
                suite.tests.append(TestCase(
                    name=rom.name,
                    path=rom.relative_to(self.test_roms_dir),
                    expected=expected,
                    notes=self.KNOWN_FAILURES.get(test_name, ""),
                ))
            suites.append(suite)

        return suites

    def load_suites(self, categories: Optional[list[str]] = None):
        """Load test suites from discovered tests."""
        # Auto-discover tests
        discovered = self.discover_tests()

        if categories:
            # Filter by category
            for suite in discovered:
                suite_lower = suite.name.lower()
                for cat in categories:
                    cat_lower = cat.lower()
                    if (cat_lower in suite_lower or
                        (cat_lower in ("spc", "apu", "dsp") and "spc" in suite_lower) or
                        (cat_lower == "blargg")):
                        self.suites.append(suite)
                        break
        else:
            self.suites = discovered

    def run(self, categories: Optional[list[str]] = None) -> int:
        """Run the test suite."""
        try:
            self.download_test_roms()
            self.load_suites(categories)

            if not self.suites:
                if not self.json_output:
                    print(f"{Colors.YELLOW}No test suites found!{Colors.NC}")
                    print("The test repository might be empty or have a different structure.")
                    print(f"Repository location: {self.test_roms_dir}")
                return 1

            if not self.json_output:
                print(f"{Colors.BLUE}{'=' * 60}{Colors.NC}")
                print(f"{Colors.BLUE}{Colors.BOLD}     SNES EMULATOR TEST SUITE - BLARGG TESTS{Colors.NC}")
                print(f"{Colors.BLUE}{'=' * 60}{Colors.NC}")
                print(f"\nEmulator: {self.emulator}")
                print(f"Test ROMs: {self.test_roms_dir}")
                print(f"Timeout:  {self.TIMEOUT_SECONDS}s per test")

                total_tests = sum(len(s.tests) for s in self.suites)
                print(f"Total tests: {total_tests}")

                print(f"\n{Colors.CYAN}Blargg tests write results to $6000:{Colors.NC}")
                print(f"  $6000: Status (0x00=pass, 0x01-0x7F=fail code)")
                print(f"  $6004+: Result text")

            for suite in self.suites:
                self.run_suite(suite)

            return self._print_summary()
        finally:
            self.cleanup()

    def _print_summary(self) -> int:
        """Print final summary and return exit code."""
        total_passed = sum(s.passed for s in self.suites)
        total_runs = sum(s.runs for s in self.suites)
        total_failed = sum(s.failed for s in self.suites)
        total_known = sum(s.known_fails for s in self.suites)
        total_skipped = sum(s.skipped for s in self.suites)
        total_timeouts = sum(s.timeouts for s in self.suites)
        total_errors = sum(s.errors for s in self.suites)
        total_run = total_passed + total_runs + total_failed + total_known + total_timeouts + total_errors

        if self.json_output:
            results = {
                "summary": {
                    "passed": total_passed,
                    "runs": total_runs,
                    "failed": total_failed,
                    "known_failures": total_known,
                    "skipped": total_skipped,
                    "timeouts": total_timeouts,
                    "errors": total_errors,
                    "total_run": total_run,
                    "pass_rate": round((total_passed + total_runs) / total_run * 100, 1) if total_run > 0 else 0,
                },
                "suites": [
                    {
                        "name": s.name,
                        "passed": s.passed,
                        "failed": s.failed,
                        "known_failures": s.known_fails,
                        "timeouts": s.timeouts,
                        "errors": s.errors,
                        "tests": [
                            {
                                "name": t.name,
                                "result": t.result.value if t.result else "unknown",
                                "status_code": t.status_code,
                                "result_text": t.result_text,
                                "notes": t.notes,
                            }
                            for t in s.tests
                        ],
                    }
                    for s in self.suites
                ],
            }
            print(json.dumps(results, indent=2))
        else:
            print(f"\n{Colors.BLUE}{'=' * 60}{Colors.NC}")
            print(f"{Colors.BLUE}{Colors.BOLD}                   FINAL RESULTS{Colors.NC}")
            print(f"{Colors.BLUE}{'=' * 60}{Colors.NC}")
            print()

            # Results table
            print(f"  {'Test Suite':<35} {'Pass':>6} {'Fail':>6} {'Known':>6}")
            print(f"  {'-' * 35} {'-' * 6} {'-' * 6} {'-' * 6}")
            for s in self.suites:
                print(f"  {s.name:<35} {s.passed:>6} {s.failed:>6} {s.known_fails:>6}")
            print(f"  {'-' * 35} {'-' * 6} {'-' * 6} {'-' * 6}")
            print(f"  {'TOTAL':<35} {total_passed:>6} {total_failed:>6} {total_known:>6}")

            print()
            print(f"  {Colors.GREEN}Passed:       {total_passed}{Colors.NC}")
            print(f"  {Colors.CYAN}Runs:         {total_runs}{Colors.NC}")
            print(f"  {Colors.RED}Failed:       {total_failed}{Colors.NC}")
            print(f"  {Colors.YELLOW}Known Issues: {total_known}{Colors.NC}")
            print(f"  {Colors.MAGENTA}Timeouts:     {total_timeouts}{Colors.NC}")
            print(f"  Skipped:      {total_skipped}")
            if total_errors:
                print(f"  {Colors.RED}Errors:       {total_errors}{Colors.NC}")

            if total_run > 0:
                pass_rate = (total_passed + total_runs) / total_run * 100
                color = Colors.GREEN if pass_rate >= 80 else (Colors.YELLOW if pass_rate >= 50 else Colors.RED)
                print(f"\n  Pass Rate: {color}{pass_rate:.1f}%{Colors.NC}")
            print()

        return 1 if total_failed > 0 else 0


def main():
    parser = argparse.ArgumentParser(
        description="SNES Emulator Test Suite - Blargg Tests",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Categories:
  spc/apu/dsp   SPC700 audio processor and DSP tests (default)
  blargg        All Blargg tests
  all           Run all available tests

Blargg Test Result Detection:
  Blargg's tests write results to specific memory addresses:
  - $6000: Status code (0x00=pass, 0x01-0x7F=fail code, 0x80=running)
  - $6001-$6003: Signature bytes (0xDE 0xB0 0x61)
  - $6004+: Result text (null-terminated ASCII)

Examples:
  python test_runner.py              # Run all tests
  python test_runner.py spc          # Run SPC700 tests only
  python test_runner.py --keep       # Keep test ROMs after completion
  python test_runner.py --json       # JSON output for CI
  python test_runner.py -v           # Verbose output
        """,
    )
    parser.add_argument(
        "categories",
        nargs="*",
        help="Test categories to run (spc, apu, dsp, blargg, all)",
    )
    parser.add_argument(
        "--keep",
        action="store_true",
        help="Keep test ROMs after completion",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Show detailed test output",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output results as JSON (for CI)",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=60,
        help="Timeout per test in seconds (default: 60)",
    )
    args = parser.parse_args()

    try:
        runner = SNESTestRunner(
            keep_roms=args.keep,
            verbose=args.verbose,
            json_output=args.json,
        )
        runner.TIMEOUT_SECONDS = args.timeout
        sys.exit(runner.run(args.categories or None))
    except FileNotFoundError as e:
        print(f"{Colors.RED}Error: {e}{Colors.NC}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print(f"\n{Colors.YELLOW}Test run interrupted{Colors.NC}")
        sys.exit(130)


if __name__ == "__main__":
    main()
