#!/bin/bash
#
# NES Emulator Test Suite entrypoint.
#
# This is a thin shim over the shared Python testkit (tests/veloce_testkit) via
# cores/nes/tests/runner.py, so detection (Blargg $6000 memory protocol,
# screenshot-CRC, nestest cpu-trace) and the completeness scorecard are identical
# across every console. The bespoke bash detection that used to live here has been
# retired in favor of the standardized harness.
#
# Usage:
#   ./run_tests.sh                  # run all suites, human scorecard
#   ./run_tests.sh cpu ppu          # run by subsystem key (cpu|ppu|apu|timing|mapper|memory|misc) or suite id
#   ./run_tests.sh --json           # machine-readable scorecard JSON (for tests/run_all.py)
#   ./run_tests.sh -v               # per-test verdict lines
#   ./run_tests.sh --keep           # keep the cloned nes-test-roms checkout
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
