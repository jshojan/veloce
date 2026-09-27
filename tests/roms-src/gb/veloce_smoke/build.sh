#!/usr/bin/env bash
# tests/roms-src/gb/veloce_smoke/build.sh — GB-2
#
# Builds the two GB veloce-test.inc smoke fixtures (one per dialect: wla-gb
# and RGBDS) inside the pinned tools/rom-toolchain image and copies them to
# build/roms/gb/veloce_smoke/, matching where tests/roms-src/build.py puts
# every other suite's output.
#
# This is a first-party Veloce fixture, not an upstream rebuild, so it
# intentionally does NOT go through build.py's clone-at-SHA + golden-gate
# recipe.json contract (there is no upstream repo to pin or gate against).
# It exists to prove the two includes under tests/roms-src/include/gb/ are
# usable end to end, and (once wired into SH-5) as the ROM the shared
# determinism selftest runs twice and byte-diffs.
#
# Usage: tests/roms-src/gb/veloce_smoke/build.sh [--image IMG]

set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/../../../.." && pwd)"
IMAGE="${VELOCE_ROM_TOOLCHAIN_IMAGE:-veloce/rom-toolchain:dev}"
if [ "${1:-}" = "--image" ]; then IMAGE="$2"; fi

OUT_DIR="$REPO_ROOT/build/roms/gb/veloce_smoke"
mkdir -p "$OUT_DIR"
ROMS_SRC="$REPO_ROOT/tests/roms-src"

# wla-gb dialect. The whole tests/roms-src tree is mounted (read-write) so
# the smoke source's relative ".include ../../include/gb/veloce-test.inc"
# resolves the same way it will for every future suite that includes it.
docker run --rm -v "$ROMS_SRC:/work" -w /work/gb/veloce_smoke "$IMAGE" sh -c '
  set -e
  wla-gb -o /tmp/smoke_wla.o smoke_wla.s
  printf "[objects]\n/tmp/smoke_wla.o\n" > /tmp/smoke_wla.link
  wlalink -d -S /tmp/smoke_wla.link /tmp/veloce_smoke.gb
  cp /tmp/veloce_smoke.gb ./veloce_smoke.gb
'
cp "$HERE/veloce_smoke.gb" "$OUT_DIR/veloce_smoke.gb"

# RGBDS dialect (0.6.1; see PROVENANCE.md for why this pin, not 0.7.0)
docker run --rm -v "$ROMS_SRC:/work" -w /work/gb/veloce_smoke -e PATH="/opt/rgbds-0.6.1/bin:/usr/bin:/bin" "$IMAGE" sh -c '
  set -e
  rgbasm -o /tmp/smoke_rgbds.o smoke_rgbds.asm
  rgblink -o /tmp/veloce_smoke_rgbds.gb /tmp/smoke_rgbds.o
  rgbfix -v -p 0xFF /tmp/veloce_smoke_rgbds.gb
  cp /tmp/veloce_smoke_rgbds.gb ./veloce_smoke_rgbds.gb
'
cp "$HERE/veloce_smoke_rgbds.gb" "$OUT_DIR/veloce_smoke_rgbds.gb"

sha256sum "$HERE/veloce_smoke.gb" "$HERE/veloce_smoke_rgbds.gb"
echo "wrote $OUT_DIR/veloce_smoke.gb and veloce_smoke_rgbds.gb"
