#!/bin/bash
#
# Game Boy / Game Boy Color test suite entrypoint.
#
# Thin wrapper that forwards to run_tests.py, which drives the shared
# veloce_testkit harness (identical detection + scoring + JSON shape across all
# four consoles). ROMs are auto-fetched from the upstream repos (Blargg, Mooneye,
# Mealybug, SameSuite, dmg/cgb-acid2, Wilbertpol) and merged under tests/roms/.
#
# Usage:
#   ./run_tests.sh                 # run all, human scorecard
#   ./run_tests.sh cpu ppu apu     # filter by subsystem key or suite id
#   ./run_tests.sh --json          # scorecard JSON (for tests/run_all.py)
#   ./run_tests.sh -v              # per-test verdict lines
#   ./run_tests.sh --generate-refs # print measured screenshot CRC32 hashes
#
# Result detection per test is declared in test_config.json:
#   file           every non-visual test: verdict from the VELOCE-RESULT/1
#                  file the core writes, adapter named in "channel"
#                  (blarggtext/a000/hram/mooneye; see
#                  cores/gb/src/test_channel.hpp). "mooneye" is Tier C, so
#                  result_policy stays "legacy" until GB-4/GB-5 rebuild
#                  Mooneye/SameSuite/Wilbertpol from source (SH-7 gb).
#   screenshot-crc Mealybug / acid2 (visual, unscored)

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PY="python3"
exec "$PY" "$SCRIPT_DIR/run_tests.py" "$@"
