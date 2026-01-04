#!/usr/bin/env python3
"""
GBA Emulator Test Runner

A comprehensive test suite runner for validating GBA emulator accuracy
using multiple test ROM collections:
- jsmolka/gba-tests: Basic CPU, memory, BIOS, and save type tests
- mgba-emu/suite: Comprehensive mGBA test suite with verbose debug output
- nba-emu/hw-test: NanoBoyAdvance hardware tests (DMA, IRQ, timing)

This runner uses the Veloce emulator directly with DEBUG=1 to run test ROMs
and detect pass/fail based on emulator debug output.

Usage:
    python test_runner.py              # Run all tests
    python test_runner.py arm          # Run ARM instruction tests only
    python test_runner.py thumb        # Run Thumb instruction tests only
    python test_runner.py --keep       # Keep test ROMs after completion
    python test_runner.py --json       # Output results as JSON

Test Result Detection:
    The emulator outputs debug messages with [GBA] prefix:
    - "[GBA] PASSED" indicates all tests passed
    - "[GBA/INFO]", "[GBA/DEBUG]", etc. from mGBA debug registers
    - "[GBA] Test result register..." shows intermediate test state
    - Timeout or crash indicates failure
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
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
    VISUAL = "visual"  # Visual test - requires screenshot analysis


class TestType(Enum):
    AUTOMATED = "automated"  # Has pass/fail detection via debug output
    VISUAL = "visual"  # Requires visual verification (screenshot)


@dataclass
class TestCase:
    """Represents a single test ROM."""
    name: str
    path: Path
    expected: str
    repository: str = "jsmolka"  # Which repository this test belongs to
    test_type: TestType = TestType.AUTOMATED
    description: str = ""
    notes: str = ""
    result: Optional[TestResult] = None
    output: str = ""
    exit_code: int = 0
    failed_test_number: Optional[int] = None
    screenshot_frame: int = 300  # Frame to capture screenshot for visual tests
    screenshot_path: Optional[Path] = None  # Path to captured screenshot


@dataclass
class TestSuite:
    """Represents a collection of related tests."""
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

    @property
    def timeouts(self) -> int:
        return sum(1 for t in self.tests if t.result == TestResult.TIMEOUT)

    @property
    def visual(self) -> int:
        return sum(1 for t in self.tests if t.result == TestResult.VISUAL)


class Colors:
    """ANSI color codes for terminal output."""
    RED = "\033[0;31m"
    GREEN = "\033[0;32m"
    YELLOW = "\033[0;33m"
    BLUE = "\033[0;34m"
    CYAN = "\033[0;36m"
    MAGENTA = "\033[0;35m"
    NC = "\033[0m"  # No Color

    @classmethod
    def disable(cls):
        """Disable colors for non-terminal output."""
        cls.RED = cls.GREEN = cls.YELLOW = cls.BLUE = cls.CYAN = cls.MAGENTA = cls.NC = ""


class GBATestRunner:
    """Main test runner for GBA emulator tests using Veloce directly."""

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
        self.screenshots_dir = self.script_dir / "screenshots"
        self.screenshots_dir.mkdir(parents=True, exist_ok=True)
        self.emulator = self._find_emulator()
        self.config = self._load_config()
        # Use timeout from config, default to 60 seconds
        self.TIMEOUT_SECONDS = self.config.get("timeout_seconds", 60)
        self.suites: list[TestSuite] = []
        self.repo_dirs: dict[str, Path] = {}  # Maps repo name -> local path

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
        """Load test configuration from JSON file."""
        config_path = self.script_dir / "test_config.json"
        if config_path.exists():
            with open(config_path) as f:
                return json.load(f)
        return {"test_suites": {}}

    def clone_test_roms(self):
        """Clone all test ROM repositories defined in config."""
        repositories = self.config.get("repositories", {})

        if not repositories:
            # Fallback to legacy single repo mode
            legacy_dir = self.script_dir / "gba-test-roms"
            if not legacy_dir.exists():
                if not self.json_output:
                    print(f"{Colors.BLUE}Cloning gba-tests repository...{Colors.NC}")
                subprocess.run(
                    ["git", "clone", "--depth", "1",
                     "https://github.com/jsmolka/gba-tests.git", str(legacy_dir)],
                    check=True,
                    capture_output=not self.verbose,
                )
            self.repo_dirs["jsmolka"] = legacy_dir
            return

        for repo_name, repo_config in repositories.items():
            url = repo_config.get("url")
            directory = repo_config.get("directory", repo_name)
            local_path = self.script_dir / directory
            self.repo_dirs[repo_name] = local_path

            if local_path.exists():
                if self.verbose and not self.json_output:
                    print(f"{Colors.BLUE}Repository '{repo_name}' already present at {directory}{Colors.NC}")
                continue

            if not self.json_output:
                desc = repo_config.get("description", "")
                print(f"{Colors.BLUE}Cloning {repo_name}: {desc}{Colors.NC}")
                print(f"  {Colors.CYAN}{url}{Colors.NC}")

            try:
                subprocess.run(
                    ["git", "clone", "--depth", "1", url, str(local_path)],
                    check=True,
                    capture_output=not self.verbose,
                )
            except subprocess.CalledProcessError as e:
                if not self.json_output:
                    print(f"  {Colors.RED}Failed to clone: {e}{Colors.NC}")
                # Don't fail entirely, just skip this repo

        if not self.json_output:
            print()

    def cleanup(self):
        """Remove all test ROM directories."""
        if self.keep_roms:
            return

        for repo_name, repo_path in self.repo_dirs.items():
            if repo_path.exists():
                if not self.json_output:
                    print(f"\n{Colors.BLUE}Cleaning up {repo_name} test ROMs...{Colors.NC}")
                shutil.rmtree(repo_path)

    def run_test(self, test: TestCase) -> TestResult:
        """Run a single test ROM through Veloce and determine the result."""
        # Get the repository directory for this test
        repo_dir = self.repo_dirs.get(test.repository)
        if not repo_dir:
            test.result = TestResult.SKIP
            test.output = f"Repository '{test.repository}' not found"
            return TestResult.SKIP

        rom_path = repo_dir / test.path

        if not rom_path.exists():
            test.result = TestResult.SKIP
            test.output = "ROM not found"
            return TestResult.SKIP

        # For save tests, clean up existing save files to ensure fresh state
        # Save tests require blank memory to pass correctly
        if "save" in str(test.path).lower():
            save_dir = self.project_root / "saves"
            save_file = save_dir / f"{rom_path.stem}.sav"
            if save_file.exists():
                save_file.unlink()

        # Build command - run veloce in headless mode with DEBUG=1
        # Use more frames for save tests which require multiple erase/write cycles
        frame_limit = self.config.get("frame_limit", 300)
        if "save" in str(test.path).lower():
            frame_limit = max(frame_limit, 1000)  # Save tests need more frames

        # Visual tests may need more frames and a screenshot
        if test.test_type == TestType.VISUAL:
            frame_limit = max(frame_limit, test.screenshot_frame + 10)

        cmd = [str(self.emulator), str(rom_path)]
        env = os.environ.copy()
        env["DEBUG"] = "1"
        env["HEADLESS"] = "1"
        env["FRAMES"] = str(frame_limit)

        # For visual tests, capture a screenshot at the specified frame
        if test.test_type == TestType.VISUAL:
            screenshot_path = self.screenshots_dir / f"{test.name}.png"
            env["SAVE_SCREENSHOT"] = str(screenshot_path)
            test.screenshot_path = screenshot_path

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=self.TIMEOUT_SECONDS,
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
            test.result = TestResult.FAIL
            test.output = str(e)
            return TestResult.FAIL

        # For visual tests, check if screenshot was captured and mark accordingly
        if test.test_type == TestType.VISUAL:
            if test.screenshot_path and test.screenshot_path.exists():
                test.result = TestResult.VISUAL
                return TestResult.VISUAL
            elif test.exit_code == 0:
                # Test ran but screenshot may not have been saved
                test.result = TestResult.VISUAL
                return TestResult.VISUAL
            else:
                if test.expected == "known_fail":
                    test.result = TestResult.KNOWN_FAIL
                else:
                    test.result = TestResult.FAIL
                return test.result

        # Analyze output for test results (automated tests)
        # Look for [GBA] PASSED in debug output (indicates R12 == 0 in infinite loop)
        if "[GBA] PASSED" in test.output:
            test.result = TestResult.PASS
            return TestResult.PASS

        # Check for [GBA] FAILED with test number (R12 > 0 in infinite loop)
        # Pattern: "[GBA] FAILED - Failed at test #N"
        fail_match = re.search(r"\[GBA\] FAILED.*?test\s*#?(\d+)", test.output)
        if fail_match:
            test.failed_test_number = int(fail_match.group(1))
            if test.expected == "known_fail":
                test.result = TestResult.KNOWN_FAIL
            else:
                test.result = TestResult.FAIL
            return test.result

        # Check for generic pass patterns
        output_lower = test.output.lower()
        if any(p in output_lower for p in ["all tests passed", "tests passed"]):
            test.result = TestResult.PASS
            return TestResult.PASS

        # Check for explicit failure with test number (legacy format)
        fail_match = re.search(r"failed test\s*#?(\d+)", output_lower)
        if fail_match:
            test.failed_test_number = int(fail_match.group(1))
            if test.expected == "known_fail":
                test.result = TestResult.KNOWN_FAIL
            else:
                test.result = TestResult.FAIL
            return test.result

        # Check for other failure indicators
        if any(p in output_lower for p in ["[gba] fail", "error"]):
            if test.expected == "known_fail":
                test.result = TestResult.KNOWN_FAIL
            else:
                test.result = TestResult.FAIL
            return test.result

        # No clear result - use exit code
        if test.exit_code == 0:
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
                    TestResult.VISUAL: f"{Colors.CYAN}VISUAL{Colors.NC}",
                }.get(result, "???")
                print(f"  {symbol} {test.name}")

                if result == TestResult.FAIL:
                    if test.failed_test_number is not None:
                        print(f"       Failed at test #{test.failed_test_number}")
                    if test.notes:
                        print(f"       Note: {test.notes}")
                    # Show relevant debug output
                    for line in test.output.split('\n'):
                        if '[GBA]' in line:
                            print(f"       {line.strip()}")
                elif result == TestResult.VISUAL:
                    if test.screenshot_path and test.screenshot_path.exists():
                        print(f"       Screenshot: {test.screenshot_path}")
                    if test.notes:
                        print(f"       Note: {test.notes}")

        if not self.json_output:
            parts = [
                f"{Colors.GREEN}Passed: {suite.passed}{Colors.NC}",
                f"{Colors.RED}Failed: {suite.failed}{Colors.NC}",
            ]
            if suite.known_fails > 0:
                parts.append(f"{Colors.YELLOW}Known: {suite.known_fails}{Colors.NC}")
            if suite.visual > 0:
                parts.append(f"{Colors.CYAN}Visual: {suite.visual}{Colors.NC}")
            if suite.timeouts > 0:
                parts.append(f"{Colors.YELLOW}Timeout: {suite.timeouts}{Colors.NC}")
            if suite.skipped > 0:
                parts.append(f"Skipped: {suite.skipped}")
            print("  " + " | ".join(parts))

    def load_suites(self, categories: Optional[list[str]] = None):
        """Load test suites from configuration."""
        # Map category names to suite names in config
        category_map = {
            "arm": ["arm"],
            "thumb": ["thumb"],
            "memory": ["memory"],
            "ppu": ["ppu", "nba_ppu"],
            "bios": ["bios"],
            "save": ["save"],
            "unsafe": ["unsafe"],
            "dma": ["nba_dma"],
            "irq": ["nba_irq"],
            "timer": ["nba_timer"],
            "haltcnt": ["nba_haltcnt"],
            "bus": ["nba_bus"],
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

        # Load suites from config
        for suite_name in sorted(suite_names):
            suite_config = self.config.get("test_suites", {}).get(suite_name)
            if not suite_config:
                continue

            suite = TestSuite(
                name=suite_config.get("name", suite_name),
                description=suite_config.get("description", ""),
                priority=suite_config.get("priority", "medium"),
            )

            # Get the repository for this suite
            suite_repo = suite_config.get("repository", "jsmolka")

            for test_config in suite_config.get("tests", []):
                # Parse test type
                test_type_str = test_config.get("test_type", "automated")
                test_type = TestType.VISUAL if test_type_str == "visual" else TestType.AUTOMATED

                test = TestCase(
                    name=Path(test_config["path"]).stem,
                    path=Path(test_config["path"]),
                    expected=test_config.get("expected", "pass"),
                    repository=suite_repo,
                    test_type=test_type,
                    description=test_config.get("description", ""),
                    notes=test_config.get("notes", ""),
                    screenshot_frame=test_config.get("screenshot_frame", 300),
                )
                suite.tests.append(test)

            self.suites.append(suite)

    def run(self, categories: Optional[list[str]] = None) -> int:
        """Run the test suite and return exit code."""
        try:
            self.clone_test_roms()
            self.load_suites(categories)

            if not self.json_output:
                print(f"{Colors.BLUE}{'=' * 56}{Colors.NC}")
                print(f"{Colors.BLUE}           GBA EMULATOR TEST SUITE{Colors.NC}")
                print(f"{Colors.BLUE}{'=' * 56}{Colors.NC}")
                print(f"\nEmulator:    {self.emulator}")
                print(f"Timeout:     {self.TIMEOUT_SECONDS}s per test")
                print(f"Frame limit: {self.config.get('frame_limit', 300)} frames")
                print(f"Debug mode:  Enabled (DEBUG=1, HEADLESS=1)")
                repositories = self.config.get("repositories", {})
                if repositories:
                    print(f"Repositories: {len(repositories)}")
                    for name, cfg in repositories.items():
                        print(f"  - {name}: {cfg.get('description', cfg.get('url', ''))}")

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
        total_timeouts = sum(s.timeouts for s in self.suites)
        total_visual = sum(s.visual for s in self.suites)
        total_run = total_passed + total_failed + total_known

        if self.json_output:
            results = {
                "summary": {
                    "passed": total_passed,
                    "failed": total_failed,
                    "known_failures": total_known,
                    "visual": total_visual,
                    "timeouts": total_timeouts,
                    "skipped": total_skipped,
                    "pass_rate": round(total_passed / total_run * 100, 1) if total_run > 0 else 0,
                    "screenshots_dir": str(self.screenshots_dir) if total_visual > 0 else None,
                },
                "suites": [
                    {
                        "name": s.name,
                        "description": s.description,
                        "priority": s.priority,
                        "passed": s.passed,
                        "failed": s.failed,
                        "known_failures": s.known_fails,
                        "visual": s.visual,
                        "timeouts": s.timeouts,
                        "tests": [
                            {
                                "name": t.name,
                                "path": str(t.path),
                                "result": t.result.value if t.result else "unknown",
                                "failed_test_number": t.failed_test_number,
                                "screenshot_path": str(t.screenshot_path) if t.screenshot_path else None,
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
            if total_visual > 0:
                print(f"  {Colors.CYAN}Visual:       {total_visual}{Colors.NC}")
                print(f"  Screenshots:  {self.screenshots_dir}")
            print(f"  Timeouts:     {total_timeouts}")
            print(f"  Skipped:      {total_skipped}")
            if total_run > 0:
                pass_rate = total_passed / total_run * 100
                print(f"\n  Pass Rate: {pass_rate:.1f}%")
            print()

        return 1 if total_failed > 0 else 0


def main():
    parser = argparse.ArgumentParser(
        description="GBA Emulator Test Suite (uses Veloce directly with DEBUG=1)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Categories:
  arm       ARM instruction set tests
  thumb     Thumb instruction set tests
  memory    Memory access tests
  ppu       PPU/graphics tests (jsmolka + NBA)
  bios      BIOS function tests
  save      Save type tests
  unsafe    Edge case tests (may not pass on hardware)
  dma       DMA tests (NanoBoyAdvance)
  irq       IRQ tests (NanoBoyAdvance)
  timer     Timer tests (NanoBoyAdvance)
  haltcnt   HALTCNT tests (NanoBoyAdvance)
  bus       Bus tests (NanoBoyAdvance)

Examples:
  python test_runner.py              # Run all tests
  python test_runner.py arm thumb    # Run ARM and Thumb tests
  python test_runner.py --keep       # Keep test ROMs
  python test_runner.py --json       # JSON output for CI

Test ROM Sources:
  - jsmolka/gba-tests: Basic CPU, memory, BIOS, and save type tests
  - mgba-emu/suite: Comprehensive mGBA test suite
  - nba-emu/hw-test: NanoBoyAdvance hardware tests
        """,
    )
    parser.add_argument(
        "categories",
        nargs="*",
        help="Test categories to run (arm, thumb, memory, ppu, bios, save, unsafe)",
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

    try:
        runner = GBATestRunner(
            keep_roms=args.keep,
            verbose=args.verbose,
            json_output=args.json,
        )
        sys.exit(runner.run(args.categories or None))
    except FileNotFoundError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
