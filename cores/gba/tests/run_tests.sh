#!/bin/bash
#
# GBA Emulator Test Suite entrypoint.
#
# Thin shim over the shared Python testkit (tests/veloce_testkit) via
# cores/gba/tests/runner.py, so detection and the completeness scorecard are
# identical across every console. Every verdict comes from the VELOCE-RESULT/1
# file the emulator writes to VELOCE_TEST_OUT (docs/testing/VELOCE-RESULT.md);
# the bespoke bash detection that grepped DEBUG=1 stderr for "[GBA] PASSED"
# has been retired along with that stderr output.
#
# Usage:
#   ./run_tests.sh                  # run all suites, human scorecard
#   ./run_tests.sh cpu timing       # run by subsystem key or suite id
#   ./run_tests.sh --json           # machine-readable scorecard JSON (for tests/run_all.py)
#   ./run_tests.sh -v               # per-test verdict lines
#   ./run_tests.sh --keep           # keep the cloned ROM repositories
#   ./run_tests.sh --generate-refs  # emit measured CRCs for screenshot-crc tests
#
# Requires: a built veloce binary (build/bin/veloce) and python3.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PYTHON="${PYTHON:-python3}"
if ! command -v "$PYTHON" >/dev/null 2>&1; then
    # Fall back to the pyenv interpreter used to build this project.
    if [ -x "$HOME/.pyenv/versions/3.12.10/bin/python" ]; then
        PYTHON="$HOME/.pyenv/versions/3.12.10/bin/python"
    fi
fi

exec "$PYTHON" "$SCRIPT_DIR/runner.py" "$@"
