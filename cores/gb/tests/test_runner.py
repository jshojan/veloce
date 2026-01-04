#!/usr/bin/env python3
"""
Game Boy Emulator Test Runner (Unified Blargg + Mooneye + Cycle-Accurate)

A comprehensive test suite runner for validating Game Boy emulator accuracy
using multiple test ROM sources: Blargg's gb-test-roms, Mooneye, dmg-acid2, and more.

Usage:
    python test_runner.py                  # Run all tests
    python test_runner.py blargg           # Run Blargg tests only
    python test_runner.py mooneye          # Run Mooneye tests only
    python test_runner.py cpu_instrs       # Run specific suite
    python test_runner.py --keep           # Keep test ROMs after completion
    python test_runner.py --json           # Output results as JSON
    python test_runner.py --generate-refs  # Generate reference hashes for visual tests
"""

import argparse
import io
import json
import os
import shutil
import subprocess
import sys
import urllib.request
import zipfile
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
    SERIAL = "serial"
    VISUAL = "visual"


@dataclass
class TestCase:
    name: str
    path: Path
    expected: str
    repo: str = "blargg"
    test_type: TestType = TestType.SERIAL
    description: str = ""
    notes: str = ""
    result: Optional[TestResult] = None
    output: str = ""
    exit_code: int = 0
    screenshot_frame: int = 300
    reference_hash: str = ""
    actual_hash: str = ""
    screenshot_path: Optional[Path] = None


@dataclass
class TestSuite:
    name: str
    description: str
    priority: str
    repo: str = "blargg"
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


class Colors:
    RED = "\033[0;31m"
    GREEN = "\033[0;32m"
    YELLOW = "\033[0;33m"
    BLUE = "\033[0;34m"
    CYAN = "\033[0;36m"
    MAGENTA = "\033[0;35m"
    NC = "\033[0m"

    @classmethod
    def disable(cls):
        cls.RED = cls.GREEN = cls.YELLOW = cls.BLUE = cls.CYAN = cls.MAGENTA = cls.NC = ""


def compute_file_crc32(filepath: Path) -> str:
    if not filepath.exists():
        return ""
    with open(filepath, 'rb') as f:
        data = f.read()
    return format(zlib.crc32(data) & 0xFFFFFFFF, '08x')


class GBTestRunner:
    TIMEOUT_SECONDS = 60

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
        self.screenshots_dir = self.script_dir / "screenshots"
        self.screenshots_dir.mkdir(parents=True, exist_ok=True)
        self.emulator = self._find_emulator()
        self.config = self._load_config()
        self.suites: list[TestSuite] = []
        self.generated_refs: dict[str, str] = {}
        self.repos: dict[str, dict] = self.config.get("repositories", {})

        if json_output:
            Colors.disable()

    def _find_emulator(self) -> Path:
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
        config_path = self.script_dir / "test_config.json"
        if config_path.exists():
            with open(config_path) as f:
                return json.load(f)
        return {"test_suites": {}, "repositories": {}}

    def _get_repo_dir(self, repo_name: str) -> Path:
        repo_config = self.repos.get(repo_name, {})
        dir_name = repo_config.get("dir", f"{repo_name}-test-roms")
        return self.script_dir / dir_name

    def clone_repos(self, needed_repos: set[str]):
        for repo_name in needed_repos:
            repo_config = self.repos.get(repo_name)
            if not repo_config:
                continue

            repo_dir = self._get_repo_dir(repo_name)
            if repo_dir.exists():
                if self.verbose and not self.json_output:
                    print(f"{Colors.BLUE}Repository '{repo_name}' already present{Colors.NC}")
                continue

            url = repo_config.get("url")
            if not url:
                continue

            repo_type = repo_config.get("type", "git")

            if repo_type == "zip":
                if not self.json_output:
                    print(f"{Colors.BLUE}Downloading {repo_name} test ROMs...{Colors.NC}")
                self._download_and_extract_zip(url, repo_dir)
            else:
                if not self.json_output:
                    print(f"{Colors.BLUE}Cloning {repo_name} repository...{Colors.NC}")
                subprocess.run(
                    ["git", "clone", "--depth", "1", url, str(repo_dir)],
                    check=True,
                    capture_output=not self.verbose,
                )
            if not self.json_output:
                print()

    def _download_and_extract_zip(self, url: str, dest_dir: Path):
        with urllib.request.urlopen(url) as response:
            zip_data = response.read()
        with zipfile.ZipFile(io.BytesIO(zip_data)) as zf:
            dest_dir.mkdir(parents=True, exist_ok=True)
            zf.extractall(dest_dir)

    def cleanup(self):
        if not self.keep_roms:
            for repo_name in self.repos:
                repo_dir = self._get_repo_dir(repo_name)
                if repo_dir.exists():
                    if not self.json_output:
                        print(f"{Colors.BLUE}Cleaning up {repo_name}...{Colors.NC}")
                    shutil.rmtree(repo_dir)

    def run_test(self, test: TestCase) -> TestResult:
        repo_dir = self._get_repo_dir(test.repo)
        rom_path = repo_dir / test.path

        if not rom_path.exists():
            test.result = TestResult.SKIP
            test.output = f"ROM not found: {rom_path}"
            return TestResult.SKIP

        env = os.environ.copy()
        env["DEBUG"] = "1"
        env["HEADLESS"] = "1"

        if test.test_type == TestType.VISUAL:
            env["FRAMES"] = str(test.screenshot_frame + 10)
            safe_name = str(test.path).replace("/", "_").replace(" ", "_")
            screenshot_path = self.screenshots_dir / f"{safe_name}.png"
            env["SAVE_SCREENSHOT"] = str(screenshot_path)
            test.screenshot_path = screenshot_path
        else:
            env["FRAMES"] = "3000"

        try:
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

        if test.test_type == TestType.VISUAL:
            return self._evaluate_visual_test(test)

        return self._evaluate_serial_test(test)

    def _evaluate_visual_test(self, test: TestCase) -> TestResult:
        if not test.screenshot_path or not test.screenshot_path.exists():
            if test.expected == "known_fail":
                test.result = TestResult.KNOWN_FAIL
            else:
                test.result = TestResult.FAIL
                test.output += "\nScreenshot not captured"
            return test.result

        test.actual_hash = compute_file_crc32(test.screenshot_path)

        if self.generate_refs:
            self.generated_refs[str(test.path)] = test.actual_hash
            test.result = TestResult.PASS
            return TestResult.PASS

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
            if test.expected == "known_fail":
                test.result = TestResult.KNOWN_FAIL
            else:
                test.result = TestResult.SKIP
                test.output += f"\nNo reference hash. Run with --generate-refs. Hash: {test.actual_hash}"

        return test.result

    def _evaluate_serial_test(self, test: TestCase) -> TestResult:
        output = test.output

        # Check for pass indicators
        if "=== TEST PASSED ===" in output or "[GB] PASSED" in output:
            test.result = TestResult.PASS
            return TestResult.PASS

        if "Passed" in output:
            test.result = TestResult.PASS
            return TestResult.PASS

        # Check for fail indicators
        if "=== TEST FAILED ===" in output or "[GB] FAILED" in output:
            if test.expected == "known_fail":
                test.result = TestResult.KNOWN_FAIL
            else:
                test.result = TestResult.FAIL
            return test.result

        if "Failed" in output:
            if test.expected == "known_fail":
                test.result = TestResult.KNOWN_FAIL
            else:
                test.result = TestResult.FAIL
            return test.result

        # Use exit code as fallback
        if test.exit_code == 0:
            if test.expected == "known_fail":
                test.result = TestResult.KNOWN_FAIL
            else:
                test.result = TestResult.PASS
        else:
            if test.expected == "known_fail":
                test.result = TestResult.KNOWN_FAIL
            else:
                test.result = TestResult.FAIL

        return test.result

    def run_suite(self, suite: TestSuite):
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
                elif result == TestResult.PASS and self.generate_refs and test.test_type == TestType.VISUAL:
                    print(f"       Generated hash: {test.actual_hash}")

        if not self.json_output:
            parts = [
                f"{Colors.GREEN}Passed: {suite.passed}{Colors.NC}",
                f"{Colors.RED}Failed: {suite.failed}{Colors.NC}",
            ]
            if suite.known_fails > 0:
                parts.append(f"{Colors.YELLOW}Known: {suite.known_fails}{Colors.NC}")
            if suite.timeouts > 0:
                parts.append(f"{Colors.YELLOW}Timeout: {suite.timeouts}{Colors.NC}")
            if suite.skipped > 0:
                parts.append(f"Skipped: {suite.skipped}")
            print("  " + " | ".join(parts))

    def load_suites(self, categories: Optional[list[str]] = None):
        all_suites = {**self.config.get("test_suites", {}), **self.config.get("visual_test_suites", {})}
        category_aliases = self.config.get("category_aliases", {})

        if categories:
            suite_names = set()
            for cat in categories:
                if cat in category_aliases:
                    suite_names.update(category_aliases[cat])
                elif cat in all_suites:
                    suite_names.add(cat)
        else:
            suite_names = set(all_suites.keys())

        suite_names = {s for s in suite_names if not s.startswith("_")}

        for suite_name in sorted(suite_names):
            suite_config = all_suites.get(suite_name)
            if not suite_config:
                continue

            repo = suite_config.get("repo", "blargg")
            suite = TestSuite(
                name=suite_config.get("name", suite_name),
                description=suite_config.get("description", ""),
                priority=suite_config.get("priority", "medium"),
                repo=repo,
            )

            for test_config in suite_config.get("tests", []):
                test_type_str = test_config.get("test_type", "serial")
                test_type = TestType.VISUAL if test_type_str == "visual" else TestType.SERIAL

                test = TestCase(
                    name=Path(test_config["path"]).stem,
                    path=Path(test_config["path"]),
                    expected=test_config.get("expected", "pass"),
                    repo=repo,
                    test_type=test_type,
                    description=test_config.get("description", ""),
                    notes=test_config.get("notes", ""),
                    screenshot_frame=test_config.get("screenshot_frame", 300),
                    reference_hash=test_config.get("reference_hash", ""),
                )
                suite.tests.append(test)

            self.suites.append(suite)

    def run(self, categories: Optional[list[str]] = None) -> int:
        try:
            self.load_suites(categories)

            # Determine which repos we need
            needed_repos = {s.repo for s in self.suites}
            self.clone_repos(needed_repos)

            if not self.json_output:
                print(f"{Colors.BLUE}{'=' * 60}{Colors.NC}")
                print(f"{Colors.BLUE}      GAME BOY EMULATOR TEST SUITE (Unified){Colors.NC}")
                print(f"{Colors.BLUE}{'=' * 60}{Colors.NC}")
                print(f"\nEmulator:    {self.emulator}")
                print(f"Timeout:     {self.TIMEOUT_SECONDS}s per test")
                print(f"Debug mode:  Enabled (DEBUG=1)")
                print(f"Repos:       {', '.join(sorted(needed_repos))}")
                if self.generate_refs:
                    print(f"{Colors.YELLOW}Mode:        Generating reference hashes{Colors.NC}")

            for suite in self.suites:
                self.run_suite(suite)

            result = self._print_summary()

            if self.generate_refs and self.generated_refs:
                self._output_generated_refs()

            return result
        finally:
            self.cleanup()

    def _output_generated_refs(self):
        if not self.json_output:
            print(f"\n{Colors.BLUE}{'=' * 60}{Colors.NC}")
            print(f"{Colors.BLUE}         GENERATED REFERENCE HASHES{Colors.NC}")
            print(f"{Colors.BLUE}{'=' * 60}{Colors.NC}")
            print("\nAdd these to your test_config.json:\n")
            for path, hash_val in sorted(self.generated_refs.items()):
                print(f'  "{path}": "{hash_val}"')
        else:
            print(json.dumps({"generated_refs": self.generated_refs}, indent=2))

    def _print_summary(self) -> int:
        total_passed = sum(s.passed for s in self.suites)
        total_failed = sum(s.failed for s in self.suites)
        total_known = sum(s.known_fails for s in self.suites)
        total_skipped = sum(s.skipped for s in self.suites)
        total_timeouts = sum(s.timeouts for s in self.suites)
        total_run = total_passed + total_failed + total_known

        if self.json_output:
            results = {
                "summary": {
                    "passed": total_passed,
                    "failed": total_failed,
                    "known_failures": total_known,
                    "timeouts": total_timeouts,
                    "skipped": total_skipped,
                    "pass_rate": round(total_passed / total_run * 100, 1) if total_run > 0 else 0,
                },
                "suites": [
                    {
                        "name": s.name,
                        "description": s.description,
                        "priority": s.priority,
                        "repo": s.repo,
                        "passed": s.passed,
                        "failed": s.failed,
                        "known_failures": s.known_fails,
                        "timeouts": s.timeouts,
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
            print(f"\n{Colors.BLUE}{'=' * 60}{Colors.NC}")
            print(f"{Colors.BLUE}                    FINAL RESULTS{Colors.NC}")
            print(f"{Colors.BLUE}{'=' * 60}{Colors.NC}")
            print()
            print(f"  {Colors.GREEN}Passed:       {total_passed}{Colors.NC}")
            print(f"  {Colors.RED}Failed:       {total_failed}{Colors.NC}")
            print(f"  {Colors.YELLOW}Known Issues: {total_known}{Colors.NC}")
            print(f"  Timeouts:     {total_timeouts}")
            print(f"  Skipped:      {total_skipped}")
            if total_run > 0:
                pass_rate = total_passed / total_run * 100
                print(f"\n  Pass Rate: {pass_rate:.1f}%")
            print()

        return 1 if total_failed > 0 else 0


def main():
    parser = argparse.ArgumentParser(
        description="Game Boy Emulator Test Suite (Unified Blargg + Mooneye)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Categories:
  blargg        All Blargg tests (cpu_instrs, instr_timing, mem_timing, oam_bug)
  mooneye       All Mooneye acceptance tests
  mooneye_ppu   Mooneye PPU timing tests
  mooneye_timer Mooneye timer tests
  mealybug      Mealybug Tearoom PPU tests
  visual        Visual tests (dmg_sound, cgb_sound, acid2)
  apu           SameSuite APU tests

Individual Suites:
  cpu_instrs, instr_timing, mem_timing, oam_bug
  mooneye_bits, mooneye_instr, mooneye_interrupts, mooneye_oam_dma
  mooneye_ppu, mooneye_timer, mooneye_timing
  mealybug_ppu, same_suite_apu
  dmg_sound, cgb_sound, dmg_acid2, cgb_acid2

Examples:
  python test_runner.py                    # Run all tests
  python test_runner.py blargg             # Run Blargg tests only
  python test_runner.py mooneye            # Run Mooneye tests only
  python test_runner.py cpu_instrs         # Run specific suite
  python test_runner.py --keep             # Keep test ROMs
  python test_runner.py --json             # JSON output for CI
  python test_runner.py --generate-refs    # Generate visual test hashes
        """,
    )
    parser.add_argument(
        "categories",
        nargs="*",
        help="Test categories or suites to run",
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
        runner = GBTestRunner(
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
