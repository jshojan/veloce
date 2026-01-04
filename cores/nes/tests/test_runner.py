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
    python test_runner.py --generate-refs  # Generate reference hashes for visual tests

Test Result Detection:
    - Serial output tests: Look for "Passed"/"Failed" or status codes
    - Visual tests: Compare screenshot CRC32 against reference hash
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import zlib
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


class TestType(Enum):
    SERIAL = "serial"  # Uses serial/status output for pass/fail detection
    VISUAL = "visual"  # Uses screenshot comparison


@dataclass
class TestCase:
    name: str
    path: Path
    expected: str
    test_type: TestType = TestType.SERIAL
    notes: str = ""
    result: Optional[TestResult] = None
    output: str = ""
    exit_code: int = 0
    # Visual test fields
    screenshot_frame: int = 300  # Frame to capture screenshot
    reference_hash: str = ""  # Expected CRC32 hash of screenshot
    actual_hash: str = ""  # Actual hash from test run
    screenshot_path: Optional[Path] = None


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
    MAGENTA = "\033[0;35m"
    NC = "\033[0m"  # No Color

    @classmethod
    def disable(cls):
        cls.RED = cls.GREEN = cls.YELLOW = cls.BLUE = cls.CYAN = cls.MAGENTA = cls.NC = ""


def compute_file_crc32(filepath: Path) -> str:
    """Compute CRC32 hash of a file and return as hex string."""
    if not filepath.exists():
        return ""
    with open(filepath, 'rb') as f:
        data = f.read()
    return format(zlib.crc32(data) & 0xFFFFFFFF, '08x')


class NESTestRunner:
    TEST_ROMS_REPO = "https://github.com/christopherpow/nes-test-roms.git"
    TIMEOUT_SECONDS = 10

    def __init__(
        self,
        keep_roms: bool = False,
        verbose: bool = False,
        json_output: bool = False,
        generate_refs: bool = False,
    ):
        self.keep_roms = keep_roms
        self.verbose = verbose
        self.json_output = json_output
        self.generate_refs = generate_refs
        self.script_dir = Path(__file__).parent
        self.project_root = self.script_dir.parent.parent.parent
        self.test_roms_dir = self.script_dir / "nes-test-roms"
        self.screenshots_dir = self.script_dir / "screenshots"
        self.screenshots_dir.mkdir(parents=True, exist_ok=True)
        self.emulator = self._find_emulator()
        self.config = self._load_config()
        self.suites: list[TestSuite] = []
        self.generated_refs: dict[str, str] = {}  # For --generate-refs mode

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

        # Set up environment for headless test execution
        env = os.environ.copy()
        env["DEBUG"] = "1"
        env["HEADLESS"] = "1"

        # Determine frame count based on test type
        if test.test_type == TestType.VISUAL:
            # Visual tests need frames up to screenshot point + buffer
            env["FRAMES"] = str(test.screenshot_frame + 10)
            # Set up screenshot capture - use full path to avoid name collisions
            safe_name = str(test.path).replace("/", "_").replace(" ", "_")
            screenshot_path = self.screenshots_dir / f"{safe_name}.png"
            env["SAVE_SCREENSHOT"] = str(screenshot_path)
            test.screenshot_path = screenshot_path
        else:
            env["FRAMES"] = "1500"  # Serial tests run for ~25 seconds at 60fps

        try:
            # Run from project root so emulator can find its plugins
            result = subprocess.run(
                [str(self.emulator.resolve()), str(rom_path.resolve())],
                capture_output=True,
                text=True,
                timeout=self.TIMEOUT_SECONDS,
                env=env,
                cwd=str(self.project_root),
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

        # Handle visual tests with screenshot comparison
        if test.test_type == TestType.VISUAL:
            return self._evaluate_visual_test(test)

        # Serial output tests - analyze output for results
        return self._evaluate_serial_test(test)

    def _evaluate_visual_test(self, test: TestCase) -> TestResult:
        """Evaluate a visual test by comparing screenshot hash."""
        if not test.screenshot_path or not test.screenshot_path.exists():
            # Screenshot wasn't captured
            if test.expected == "known_fail":
                test.result = TestResult.KNOWN_FAIL
            else:
                test.result = TestResult.FAIL
                test.output += "\nScreenshot not captured"
            return test.result

        # Compute hash of captured screenshot
        test.actual_hash = compute_file_crc32(test.screenshot_path)

        # In generate mode, just record the hash
        if self.generate_refs:
            self.generated_refs[str(test.path)] = test.actual_hash
            test.result = TestResult.PASS  # Mark as pass for reporting
            return TestResult.PASS

        # Compare against reference hash
        if test.reference_hash:
            if test.actual_hash == test.reference_hash:
                test.result = TestResult.PASS
            else:
                if test.expected == "known_fail":
                    test.result = TestResult.KNOWN_FAIL
                else:
                    test.result = TestResult.FAIL
                    test.output += f"\nHash mismatch: expected {test.reference_hash}, got {test.actual_hash}"
        else:
            # No reference hash - this is a new visual test
            if test.expected == "known_fail":
                test.result = TestResult.KNOWN_FAIL
            else:
                test.result = TestResult.SKIP
                test.output += f"\nNo reference hash. Run with --generate-refs to create. Hash: {test.actual_hash}"

        return test.result

    def _evaluate_serial_test(self, test: TestCase) -> TestResult:
        """Evaluate a serial output test."""
        output = test.output

        # Check for explicit pass indicators (case-sensitive for reliability)
        if "(PASSED)" in output or "Status code: 0" in output:
            test.result = TestResult.PASS
        elif "\nPassed\n" in output or "\nPassed" in output:
            test.result = TestResult.PASS
        # Check for explicit fail indicators
        elif "(FAILED)" in output or "\nFailed" in output:
            if test.expected == "known_fail":
                test.result = TestResult.KNOWN_FAIL
            else:
                test.result = TestResult.FAIL
        # Fallback to exit code
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
                print(f"    {Colors.CYAN}{suite.description}{Colors.NC}")

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

                test_type_indicator = f" {Colors.CYAN}[visual]{Colors.NC}" if test.test_type == TestType.VISUAL else ""
                print(f"  {symbol} {test.name}{test_type_indicator}")

                if result == TestResult.FAIL:
                    if test.notes:
                        print(f"       Note: {test.notes}")
                    if test.test_type == TestType.VISUAL and test.actual_hash:
                        print(f"       Hash: {test.actual_hash}")
                    # Show relevant debug output
                    for line in test.output.split('\n'):
                        if 'Failed' in line or 'Hash' in line:
                            print(f"       {line.strip()}")
                elif result == TestResult.PASS and self.generate_refs and test.test_type == TestType.VISUAL:
                    print(f"       Generated hash: {test.actual_hash}")

        if not self.json_output:
            parts = [
                f"{Colors.GREEN}Passed: {suite.passed}{Colors.NC}",
                f"{Colors.RED}Failed: {suite.failed}{Colors.NC}",
            ]
            if suite.known_fails > 0:
                parts.append(f"{Colors.YELLOW}Known: {suite.known_fails}{Colors.NC}")
            if suite.skipped > 0:
                parts.append(f"Skipped: {suite.skipped}")
            print("  " + " | ".join(parts))

    def load_suites(self, categories: Optional[list[str]] = None):
        """Load test suites from configuration."""
        category_map = {
            "cpu": ["cpu_instructions", "cpu_timing", "cpu_interrupts", "cpu_dummy_reads", "cpu_dummy_writes", "instr_misc", "blargg_cpu"],
            "ppu": ["ppu_vbl_nmi", "ppu_sprite", "ppu_misc", "sprite_overflow", "ppu_read_buffer"],
            "mapper": ["mmc3", "mmc3_irq"],
            "apu": ["apu", "dmc"],
        }

        # Merge test_suites and visual_test_suites
        all_suites = {**self.config.get("test_suites", {}), **self.config.get("visual_test_suites", {})}

        # Determine which suites to load
        if categories:
            suite_names = set()
            for cat in categories:
                if cat in category_map:
                    suite_names.update(category_map[cat])
                elif cat in all_suites:
                    suite_names.add(cat)
        else:
            suite_names = set(all_suites.keys())

        # Remove internal keys that start with _
        suite_names = {s for s in suite_names if not s.startswith("_")}

        # Load suites
        for suite_name in sorted(suite_names):
            suite_config = all_suites.get(suite_name)
            if not suite_config:
                continue

            suite = TestSuite(
                name=suite_config.get("name", suite_name),
                description=suite_config.get("description", ""),
                priority=suite_config.get("priority", "medium"),
            )

            for test_config in suite_config.get("tests", []):
                # Determine test type
                test_type_str = test_config.get("test_type", "serial")
                test_type = TestType.VISUAL if test_type_str == "visual" else TestType.SERIAL

                test = TestCase(
                    name=Path(test_config["path"]).stem,
                    path=Path(test_config["path"]),
                    expected=test_config.get("expected", "pass"),
                    test_type=test_type,
                    notes=test_config.get("notes", ""),
                    screenshot_frame=test_config.get("screenshot_frame", 300),
                    reference_hash=test_config.get("reference_hash", ""),
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
                print(f"\nEmulator:    {self.emulator}")
                print(f"Timeout:     {self.TIMEOUT_SECONDS}s per test")
                print(f"Debug mode:  Enabled (DEBUG=1)")
                if self.generate_refs:
                    print(f"{Colors.YELLOW}Mode:        Generating reference hashes{Colors.NC}")

            for suite in self.suites:
                self.run_suite(suite)

            result = self._print_summary()

            # Output generated references if in generate mode
            if self.generate_refs and self.generated_refs:
                self._output_generated_refs()

            return result
        finally:
            self.cleanup()

    def _output_generated_refs(self):
        """Output generated reference hashes for visual tests."""
        if not self.json_output:
            print(f"\n{Colors.BLUE}{'=' * 56}{Colors.NC}")
            print(f"{Colors.BLUE}         GENERATED REFERENCE HASHES{Colors.NC}")
            print(f"{Colors.BLUE}{'=' * 56}{Colors.NC}")
            print("\nAdd these to your test_config.json:\n")
            for path, hash_val in sorted(self.generated_refs.items()):
                print(f'  "{path}": "{hash_val}"')
        else:
            print(json.dumps({"generated_refs": self.generated_refs}, indent=2))

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
                    "screenshots_dir": str(self.screenshots_dir),
                },
                "suites": [
                    {
                        "name": s.name,
                        "description": s.description,
                        "priority": s.priority,
                        "passed": s.passed,
                        "failed": s.failed,
                        "known_failures": s.known_fails,
                        "tests": [
                            {
                                "name": t.name,
                                "path": str(t.path),
                                "test_type": t.test_type.value,
                                "result": t.result.value if t.result else "unknown",
                                "reference_hash": t.reference_hash,
                                "actual_hash": t.actual_hash,
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

Visual Tests:
  Tests that write to screen use screenshot comparison. Run with
  --generate-refs to create reference hashes for new visual tests.

Examples:
  python test_runner.py              # Run all tests
  python test_runner.py cpu ppu      # Run CPU and PPU tests
  python test_runner.py --keep       # Keep test ROMs
  python test_runner.py --json       # JSON output for CI
  python test_runner.py --generate-refs  # Generate reference hashes
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
    parser.add_argument(
        "--generate-refs",
        action="store_true",
        help="Generate reference hashes for visual tests",
    )
    args = parser.parse_args()

    try:
        runner = NESTestRunner(
            keep_roms=args.keep,
            verbose=args.verbose,
            json_output=args.json,
            generate_refs=args.generate_refs,
        )
        sys.exit(runner.run(args.categories or None))
    except FileNotFoundError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
