"""
Veloce Test Kit - shared cross-console test + accuracy-measurement framework.

This package is the single source of truth for:
  * the standard test_config.json schema (see schema.py)
  * result-detection protocols (see detect.py)
  * the harness that drives the headless `veloce` binary (see harness.py)
  * the defensible completeness-scoring methodology (see scoring.py)

Per-console runners under cores/<c>/tests/test_runner.py import from here so the
four console agents do not re-implement detection or scoring.  They own ONLY
their own test_config.json files.
"""

from .schema import (
    AccuracyType,
    DetectionMethod,
    Priority,
    SCHEMA_VERSION,
    TestSpec,
    SuiteSpec,
    ConsoleConfig,
    load_config,
    validate_config,
)
from .detect import (
    TestStatus,
    DetectionResult,
    detect_blargg_memory,
    detect_serial_output,
    detect_gba_register,
    detect_screenshot_crc,
    detect_cpu_trace,
)
from .harness import Harness, RunSettings, RunOutput
from .scoring import (
    SUBSYSTEM_WEIGHTS,
    ACCURACY_WEIGHTS,
    PRIORITY_WEIGHTS,
    score_test,
    score_suite,
    score_console,
    Scorecard,
)

__all__ = [
    "AccuracyType", "DetectionMethod", "Priority", "SCHEMA_VERSION",
    "TestSpec", "SuiteSpec", "ConsoleConfig", "load_config", "validate_config",
    "TestStatus", "DetectionResult",
    "detect_blargg_memory", "detect_serial_output", "detect_gba_register",
    "detect_screenshot_crc", "detect_cpu_trace",
    "Harness", "RunSettings", "RunOutput",
    "SUBSYSTEM_WEIGHTS", "ACCURACY_WEIGHTS", "PRIORITY_WEIGHTS",
    "score_test", "score_suite", "score_console", "Scorecard",
]
