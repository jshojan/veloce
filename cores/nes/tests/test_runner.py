#!/usr/bin/env python3
"""
NES Emulator Test Runner

A comprehensive test suite runner for validating NES emulator accuracy
using the nes-test-roms collection.

Usage:
    python test_runner.py              # Run all tests
    python test_runner.py cpu          # Run CPU tests only
    python test_runner.py --keep       # Keep test ROMs after completion
    python test_runner.py --json       # Output results as JSON
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Optional


class TestResult(Enum):
    PASS = "pass"
    FAIL = "fail"
    TIMEOUT = "timeout"
    SKIP = "skip"
    KNOWN_FAIL = "known_fail"


@dataclass
class TestCase:
    name: str
    path: Path
    expected: str
    notes: str = ""
    result: Optional[TestResult] = None
    output: str = ""
    exit_code: int = 0


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
    def known_fails(self) -> int:
        return sum(1 for t in self.tests if t.result == TestResult.KNOWN_FAIL)

    @property
    def skipped(self) -> int:
        return sum(1 for t in self.tests if t.result == TestResult.SKIP)


class Colors:
    RED = "\033[0;31m"
    GREEN = "\033[0;32m"
    YELLOW = "\033[0;33m"
    BLUE = "\033[0;34m"
    CYAN = "\033[0;36m"
    NC = "\033[0m"  # No Color

    @classmethod
    def disable(cls):
        cls.RED = cls.GREEN = cls.YELLOW = cls.BLUE = cls.CYAN = cls.NC = ""


class NESTestRunner:
    TEST_ROMS_REPO = "https://github.com/christopherpow/nes-test-roms.git"
    TIMEOUT_SECONDS = 10

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
        self.test_roms_dir = self.script_dir / "nes-test-roms"
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

    def clone_test_roms(self):
        """Clone the nes-test-roms repository if not present."""
        if self.test_roms_dir.exists():
            if self.verbose:
                print(f"{Colors.BLUE}Test ROMs already present{Colors.NC}")
            return

        print(f"{Colors.BLUE}Cloning nes-test-roms repository...{Colors.NC}")
        subprocess.run(
            ["git", "clone", "--depth", "1", self.TEST_ROMS_REPO, str(self.test_roms_dir)],
            check=True,
            capture_output=not self.verbose,
        )
        print()

    def cleanup(self):
        """Remove test ROMs directory."""
        if not self.keep_roms and self.test_roms_dir.exists():
            print(f"\n{Colors.BLUE}Cleaning up test ROMs...{Colors.NC}")
            shutil.rmtree(self.test_roms_dir)

    def run_test(self, test: TestCase) -> TestResult:
        """Run a single test ROM."""
        rom_path = self.test_roms_dir / test.path

        if not rom_path.exists():
            test.result = TestResult.SKIP
            test.output = "ROM not found"
            return TestResult.SKIP

        try:
            result = subprocess.run(
                [str(self.emulator), str(rom_path)],
                capture_output=True,
                text=True,
                timeout=self.TIMEOUT_SECONDS,
            )
            test.exit_code = result.returncode
            test.output = result.stdout + result.stderr
        except subprocess.TimeoutExpired:
            test.result = TestResult.TIMEOUT
            test.output = f"Timeout after {self.TIMEOUT_SECONDS}s"
            return TestResult.TIMEOUT
        except Exception as e:
            test.result = TestResult.FAIL
            test.output = str(e)
            return TestResult.FAIL

        # Check for blargg test result patterns
        output = test.output.lower()
        if any(p in output for p in ["test passed", "result: 0", "passed"]):
            test.result = TestResult.PASS
        elif any(p in output for p in ["result: ", "failed", "error"]):
            # Check if this is a known failure
            if test.expected == "known_fail":
                test.result = TestResult.KNOWN_FAIL
            else:
                test.result = TestResult.FAIL
        elif test.exit_code == 0:
            test.result = TestResult.PASS
        else:
            if test.expected == "known_fail":
                test.result = TestResult.KNOWN_FAIL
            else:
                test.result = TestResult.FAIL

        return test.result

    def run_suite(self, suite: TestSuite):
        """Run all tests in a suite."""
        if not self.json_output:
            print(f"\n{Colors.BLUE}=== {suite.name} ==={Colors.NC}")
            if self.verbose:
                print(f"    {suite.description}")

        for test in suite.tests:
            result = self.run_test(test)

            if self.verbose and not self.json_output:
                symbol = {
                    TestResult.PASS: f"{Colors.GREEN}PASS{Colors.NC}",
                    TestResult.FAIL: f"{Colors.RED}FAIL{Colors.NC}",
                    TestResult.KNOWN_FAIL: f"{Colors.YELLOW}KNOWN{Colors.NC}",
                    TestResult.TIMEOUT: f"{Colors.YELLOW}TIMEOUT{Colors.NC}",
                    TestResult.SKIP: f"{Colors.YELLOW}SKIP{Colors.NC}",
                }.get(result, "???")
                print(f"  {symbol} {test.name}")
                if result == TestResult.FAIL and test.notes:
                    print(f"       Note: {test.notes}")

        if not self.json_output:
            print(
                f"  {Colors.GREEN}Passed: {suite.passed}{Colors.NC} | "
                f"{Colors.RED}Failed: {suite.failed}{Colors.NC} | "
                f"{Colors.YELLOW}Known: {suite.known_fails}{Colors.NC} | "
                f"Skipped: {suite.skipped}"
            )

    def load_suites(self, categories: Optional[list[str]] = None):
        """Load test suites from configuration."""
        category_map = {
            "cpu": ["cpu_instructions", "cpu_timing", "cpu_interrupts"],
            "ppu": ["ppu_vbl_nmi", "ppu_sprite", "ppu_misc"],
            "mapper": ["mmc3"],
            "apu": ["apu"],
        }

        # Determine which suites to load
        if categories:
            suite_names = set()
            for cat in categories:
                if cat in category_map:
                    suite_names.update(category_map[cat])
                elif cat in self.config.get("test_suites", {}):
                    suite_names.add(cat)
        else:
            suite_names = set(self.config.get("test_suites", {}).keys())

        # Load suites
        for suite_name in suite_names:
            suite_config = self.config.get("test_suites", {}).get(suite_name)
            if not suite_config:
                continue

            suite = TestSuite(
                name=suite_config.get("name", suite_name),
                description=suite_config.get("description", ""),
                priority=suite_config.get("priority", "medium"),
            )

            for test_config in suite_config.get("tests", []):
                test = TestCase(
                    name=Path(test_config["path"]).stem,
                    path=Path(test_config["path"]),
                    expected=test_config.get("expected", "pass"),
                    notes=test_config.get("notes", ""),
                )
                suite.tests.append(test)

            self.suites.append(suite)

    def run(self, categories: Optional[list[str]] = None) -> int:
        """Run the test suite."""
        try:
            self.clone_test_roms()
            self.load_suites(categories)

            if not self.json_output:
                print(f"{Colors.BLUE}{'=' * 56}{Colors.NC}")
                print(f"{Colors.BLUE}           NES EMULATOR TEST SUITE{Colors.NC}")
                print(f"{Colors.BLUE}{'=' * 56}{Colors.NC}")
                print(f"\nEmulator: {self.emulator}")
                print(f"Timeout:  {self.TIMEOUT_SECONDS}s per test")

            for suite in self.suites:
                self.run_suite(suite)

            return self._print_summary()
        finally:
            self.cleanup()

    def _print_summary(self) -> int:
        """Print final summary and return exit code."""
        total_passed = sum(s.passed for s in self.suites)
        total_failed = sum(s.failed for s in self.suites)
        total_known = sum(s.known_fails for s in self.suites)
        total_skipped = sum(s.skipped for s in self.suites)
        total_run = total_passed + total_failed + total_known

        if self.json_output:
            results = {
                "summary": {
                    "passed": total_passed,
                    "failed": total_failed,
                    "known_failures": total_known,
                    "skipped": total_skipped,
                    "pass_rate": round(total_passed / total_run * 100, 1) if total_run > 0 else 0,
                },
                "suites": [
                    {
                        "name": s.name,
                        "passed": s.passed,
                        "failed": s.failed,
                        "known_failures": s.known_fails,
                        "tests": [
                            {
                                "name": t.name,
                                "result": t.result.value if t.result else "unknown",
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
            print(f"\n{Colors.BLUE}{'=' * 56}{Colors.NC}")
            print(f"{Colors.BLUE}                 FINAL RESULTS{Colors.NC}")
            print(f"{Colors.BLUE}{'=' * 56}{Colors.NC}")
            print()
            print(f"  {Colors.GREEN}Passed:       {total_passed}{Colors.NC}")
            print(f"  {Colors.RED}Failed:       {total_failed}{Colors.NC}")
            print(f"  {Colors.YELLOW}Known Issues: {total_known}{Colors.NC}")
            print(f"  Skipped:      {total_skipped}")
            if total_run > 0:
                pass_rate = total_passed / total_run * 100
                print(f"\n  Pass Rate: {pass_rate:.1f}%")
            print()

        return 1 if total_failed > 0 else 0


def main():
    parser = argparse.ArgumentParser(
        description="NES Emulator Test Suite",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Categories:
  cpu       CPU instruction and timing tests
  ppu       PPU rendering and timing tests
  mapper    Mapper-specific tests (MMC3, etc.)
  apu       Audio processing unit tests

Examples:
  python test_runner.py              # Run all tests
  python test_runner.py cpu ppu      # Run CPU and PPU tests
  python test_runner.py --keep       # Keep test ROMs
  python test_runner.py --json       # JSON output for CI
        """,
    )
    parser.add_argument(
        "categories",
        nargs="*",
        help="Test categories to run (cpu, ppu, mapper, apu)",
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
    args = parser.parse_args()

    runner = NESTestRunner(
        keep_roms=args.keep,
        verbose=args.verbose,
        json_output=args.json,
    )

    sys.exit(runner.run(args.categories or None))


if __name__ == "__main__":
    main()
