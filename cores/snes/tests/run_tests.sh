#!/bin/bash
#
# SNES Emulator Test Suite
# Runs snes-tests to validate emulator accuracy
#
# Usage:
#   ./run_tests.sh              # Run all tests
#   ./run_tests.sh cpu          # Run CPU tests only
#   ./run_tests.sh spc          # Run SPC700 tests only
#   ./run_tests.sh --keep       # Keep test ROMs after completion
#   ./run_tests.sh --verbose    # Show detailed output
#   ./run_tests.sh --json       # JSON output for CI
#
# Requirements:
#   - Python 3.8+
#   - Git (for cloning test ROMs)
#   - Built veloce emulator in build/bin/
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Check for Python 3
if command -v python3 &> /dev/null; then
    PYTHON=python3
elif command -v python &> /dev/null; then
    PYTHON=python
else
    echo "Error: Python 3 is required but not found"
    exit 1
fi

# Verify Python version
PY_VERSION=$($PYTHON -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
PY_MAJOR=$($PYTHON -c 'import sys; print(sys.version_info.major)')

if [ "$PY_MAJOR" -lt 3 ]; then
    echo "Error: Python 3 is required (found Python $PY_VERSION)"
    exit 1
fi

# Run the test runner
exec "$PYTHON" "$SCRIPT_DIR/test_runner.py" "$@"
