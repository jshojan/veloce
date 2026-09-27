#!/usr/bin/env python3
"""
tests/roms-src/gba/nba_hw_test/verify_windows.py -- the behavioural gate
that stands in for this suite's byte-for-byte golden gate (GBA-3).

nba-emu/hw-test's expected values are cycle counts measured on hardware
with the binaries nba ships. Only dma/latch rebuilds byte-identically from
the pinned source with tools/rom-toolchain's devkitARM; the other nine
shipped binaries came from older devkitARM releases, and a different GCC
can move an instruction into or out of a measured window, which changes
what the ROM measures on hardware and silently invalidates the expected
value. So instead of comparing whole ROMs, this script compares every
function that executes inside a measured window (timer start .. timer
read, IRQ entry .. timer read, ...) in the *patched* build against the same
function located in upstream's shipped binary, instruction by instruction.

Branch targets, PC-relative literal offsets and literal-pool words are
masked (they move with the link layout and do not change timing within one
memory region). Every other difference must be listed in windows.json with
the exact diff and why it does not affect the measured value; an
unreviewed or changed diff fails the gate.

Run after `tests/roms-src/build.py gba/nba_hw_test` (which leaves the
patched build in upstream/):

    tests/roms-src/gba/nba_hw_test/verify_windows.py          # gate
    tests/roms-src/gba/nba_hw_test/verify_windows.py --show   # print every diff

Needs docker and the toolchain image (arm-none-eabi-objdump/nm), same
default as build.py.
"""

from __future__ import annotations

import argparse
import difflib
import json
import os
import re
import subprocess
import sys
from pathlib import Path

SUITE_DIR = Path(__file__).resolve().parent
UPSTREAM = SUITE_DIR / "upstream"
REPO_ROOT = SUITE_DIR.parents[3]
WINDOWS = SUITE_DIR / "windows.json"
DEFAULT_IMAGE = os.environ.get("VELOCE_ROM_TOOLCHAIN_IMAGE") or "veloce/rom-toolchain:dev"
OUT = UPSTREAM / ".verify"          # inside the gitignored clone


def dump(image: str, rom_dir: str) -> None:
    """objdump/nm output for one ROM's patched ELF and both .gba files."""
    name = rom_dir.split("/")[-1]
    out = f"/repo/{(OUT / name).relative_to(REPO_ROOT).as_posix()}"
    script = (
        f"set -e; mkdir -p {out}; "
        f"git show HEAD:{rom_dir}/{name}.gba > {out}/shipped.gba; "
        f"cp {rom_dir}/{name}.gba {out}/patched.gba; "
        f"arm-none-eabi-objdump -h {rom_dir}/{name}.elf > {out}/sections.txt; "
        f"arm-none-eabi-objdump -d {rom_dir}/{name}.elf > {out}/elf.txt; "
        f"for f in shipped patched; do "
        f"  arm-none-eabi-objdump -D -b binary -marm {out}/$f.gba > {out}/$f.arm.txt; "
        f"  arm-none-eabi-objdump -D -b binary -marm -M force-thumb {out}/$f.gba > {out}/$f.thumb.txt; "
        f"done"
    )
    uid = ["--user", f"{os.getuid()}:{os.getgid()}", "-e", "HOME=/tmp"] if hasattr(os, "getuid") else []
    proc = subprocess.run(
        ["docker", "run", "--rm", *uid, "-v", f"{REPO_ROOT}:/repo",
         "-w", f"/repo/{UPSTREAM.relative_to(REPO_ROOT).as_posix()}", image,
         "sh", "-c", "git config --global --add safe.directory '*' 2>/dev/null; " + script],
        capture_output=True, text=True)
    if proc.returncode != 0:
        sys.exit(f"objdump failed for {rom_dir}:\n{proc.stderr}")


_LINE = re.compile(r"^\s*([0-9a-f]+):\t([0-9a-f ]+?)\s*\t(.*)$")


def norm(ins: str) -> str:
    ins = re.split(r"[@;]", ins)[0].strip()
    ins = re.sub(r"\s+", " ", ins)
    # branch targets move with the layout; keep the mnemonic (and condition)
    ins = re.sub(r"^(b[a-z]*)(?:\.[nw])? +(?:0x)?[0-9a-f]+(?: <.*>)?$", r"\1 T", ins)
    ins = re.sub(r"\[pc(?:, #-?\d+)?\]", "[pc, #X]", ins)
    return ins


_POOL_REF = re.compile(r"\[pc(?:, #-?\d+)?\]\s*@\s*\(?(?:0x)?([0-9a-f]+)")


def raw_listing(path: Path, thumb: bool) -> list[tuple[int, str]]:
    """[(file offset, normalised insn)] for a raw-binary disassembly, minus
    the literal-pool words that PC-relative loads anywhere in it point at
    (garbage decodes of data regions can only drop entries, which then
    shows up as a diff -- never hide one)."""
    rows, pool = [], set()
    for line in path.read_text().splitlines():
        m = _LINE.match(line)
        if not m:
            continue
        off, ins = int(m.group(1), 16), m.group(3)
        rows.append((off, ins))
        r = _POOL_REF.search(ins)
        if r and re.match(r"^ldr[a-z]*\s", ins):
            t = int(r.group(1), 16)
            pool.update((t, t + 2) if thumb else (t,))
    return [(off, norm(ins)) for off, ins in rows if off not in pool]


def elf_range(d: Path, fn: str) -> tuple[bool, int, int]:
    """(thumb?, first vma, end vma) of one function's block in the ELF
    disassembly (code, inline .hword data and its literal pool)."""
    cur, thumb, first, end = None, None, None, None
    for line in (d / "elf.txt").read_text().splitlines():
        m = re.match(r"^[0-9a-f]+ <(.+)>:$", line.strip())
        if m:
            if cur == fn:
                break
            cur = m.group(1)
            continue
        if cur != fn:
            continue
        m = _LINE.match(line)
        if not m:
            continue
        vma, raw, ins = int(m.group(1), 16), m.group(2).strip(), m.group(3)
        width = sum(len(t) for t in raw.split()) // 2
        if thumb is None and not ins.startswith((".word", ".short")):
            thumb = len(raw.split()[0]) == 4
        first = vma if first is None else first
        end = vma + width
    if first is None:
        sys.exit(f"{d.name}: function {fn} not in the ELF")
    return bool(thumb), first, end


def vma_to_offset(d: Path, vma: int) -> int:
    best = None
    for line in (d / "sections.txt").read_text().splitlines():
        t = line.split()
        if len(t) >= 6 and t[0].isdigit():
            size, sec_vma, lma = int(t[2], 16), int(t[3], 16), int(t[4], 16)
            if sec_vma <= vma < sec_vma + size and lma >= 0x08000000:
                best = lma - 0x08000000 + (vma - sec_vma)
    if best is None:
        sys.exit(f"{d.name}: no loadable section holds 0x{vma:x}")
    return best


def _regless(ins: str) -> str:
    return re.sub(r"\b(?:r\d+|ip|lr|sp|fp|sl)\b", "R", ins)


def compare(rom_dir: str, fn: str) -> tuple[list, int]:
    d = OUT / rom_dir.split("/")[-1]
    thumb, first, end = elf_range(d, fn)
    mode = "thumb" if thumb else "arm"
    lo, hi = vma_to_offset(d, first), vma_to_offset(d, first) + (end - first)
    a = [ins for off, ins in raw_listing(d / f"patched.{mode}.txt", thumb) if lo <= off < hi]
    shipped = raw_listing(d / f"shipped.{mode}.txt", thumb)
    ship = [ins for _, ins in shipped]
    n = len(a)
    # Locate the function in the shipped binary: register-agnostic first
    # instruction, then the window that shares the most instructions.
    ka, ks = [_regless(x) for x in a], [_regless(x) for x in ship]
    def score(x, y):
        return sum(t.size for t in difflib.SequenceMatcher(None, x, y, autojunk=False).get_matching_blocks())
    best_p, best_m = -1, (-1, -1)
    for p in range(len(ship) - n + 1):
        if ks[p] != ka[0]:
            continue
        m = (score(a, ship[p:p + n]), score(ka, ks[p:p + n]))
        if m > best_m:
            best_p, best_m = p, m
    if best_p < 0:
        return [["unlocated", a, []]], -1
    # The shipped copy may be a few instructions longer or shorter.
    def matched(b):
        return sum(t.size for t in difflib.SequenceMatcher(None, a, b, autojunk=False).get_matching_blocks())
    b = max((ship[best_p:best_p + n + k] for k in range(-4, 9)),
            key=lambda b: (matched(b), -abs(len(b) - n)))
    ops = [o for o in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes() if o[0] != "equal"]
    if ops and ops[-1][0] == "insert" and ops[-1][1] == n:
        ops.pop()   # window slack past the end of the function
    diff = [[tag, a[i1:i2], b[j1:j2]] for tag, i1, i2, j1, j2 in ops]
    return diff, shipped[best_p][0]


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--image", default=DEFAULT_IMAGE)
    ap.add_argument("--show", action="store_true", help="print every diff, reviewed or not")
    args = ap.parse_args(argv)

    spec = json.loads(WINDOWS.read_text())
    failures = 0
    for rom_dir, funcs in spec["roms"].items():
        dump(args.image, rom_dir)
        for fn, want in funcs.items():
            diff, at = compare(rom_dir, fn)
            expect = want["expect"]
            if expect == "identical":
                ok = not diff
            else:
                ok = diff == want.get("diff")
            status = "ok" if ok else "FAIL"
            label = "identical" if not diff else f"{len(diff)} reviewed diff(s)" if ok else "UNREVIEWED DIFF"
            print(f"{status:4s} {rom_dir}:{fn} (shipped @0x{at:x}) {label}")
            if (not ok) or (args.show and diff):
                print("     " + json.dumps(diff))
            failures += 0 if ok else 1
    if failures:
        print(f"\n{failures} measured function(s) differ from the shipped binary without review; "
              "the rebuilt ROM may not measure what nba measured on hardware.", file=sys.stderr)
        return 1
    print("\nall measured windows match upstream's shipped binaries")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
