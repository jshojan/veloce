#!/usr/bin/env python3
"""Build an original LoROM observing individual math-register bytes by CPU phase."""
from pathlib import Path
import hashlib
import argparse
MULTIPLICANDS = (0, 1, 2, 3, 15, 16, 31, 32, 63, 64, 127, 128, 191, 192, 254, 255)
DIVIDENDS = (0, 1, 2, 127, 128, 254, 255, 256, 257, 32767, 32768, 65534, 65535)
GROUPS = tuple((op, register, phase) for op in ("mul", "div")
               for register in range(0x14, 0x18) for phase in range(2, 19))
SEED_QUOTIENT = 0xA5


def group_name(op, register, phase):
    return f"{op}_r{register:02x}_p{phase:02d}"


class Program:
    def __init__(self):
        self.data = bytearray([0xEA] * 0x8000)
        self.pos = 0
        self.labels = {}
        self.fixups = []

    def emit(self, *data):
        if self.pos + len(data) > 0x7FC0:
            raise ValueError("code overlaps header")
        self.data[self.pos:self.pos + len(data)] = bytes(data)
        self.pos += len(data)

    def mark(self, name):
        if name in self.labels:
            raise ValueError(f"duplicate label: {name}")
        self.labels[name] = self.pos + 0x8000

    def address(self, opcode, name, extra=0):
        self.emit(opcode, 0, 0)
        self.fixups.append((self.pos - 2, name, extra))

    def branch(self, opcode, name):
        self.emit(opcode, 0)
        self.fixups.append((self.pos - 1, name, None))

    def puts(self, label):
        self.address(0xA2, label)  # LDX #string (X is 16-bit)
        self.address(0x20, "puts")

    def text(self, label, value):
        self.mark(label)
        self.emit(*value.encode("ascii"), 0)

    def resolve(self):
        for offset, name, extra in self.fixups:
            target = self.labels[name]
            if extra is None:
                delta = target - (0x8000 + offset + 1)
                if not -128 <= delta <= 127:
                    raise ValueError(f"branch out of range: {name}")
                self.data[offset] = delta & 255
            else:
                self.data[offset:offset + 2] = (target + extra).to_bytes(2, "little")


def build():
    p = Program()
    p.mark("entry")
    # Native mode, 8-bit A, 16-bit X/Y, stack in WRAM, DP=$4200.
    p.emit(0x78, 0xD8, 0x18, 0xFB, 0xC2, 0x30, 0xA9, 0xFF, 0x1F, 0x1B,
           0xA9, 0x00, 0x42, 0x5B, 0xE2, 0x20)
    p.emit(0xA9, 0x80, 0x8D, 0x00, 0x21, 0xA9, 0)
    for address in (0x4200, 0x420B, 0x420C):
        p.emit(0x8D, address & 255, address >> 8)
    p.puts("header")
    for op, register, phase in GROUPS:
        name = group_name(op, register, phase)
        p.puts(name + "_begin")
        p.emit(0xA9, 0)
        for address in (0, 1, 2, 3):
            p.emit(0x8D, address, 0)
        p.mark(name + "_outer")
        p.emit(0xAE, 0, 0)  # LDX lhs table index (high byte zero)
        p.address(0xBD, "multiplicands" if op == "mul" else "dividends")
        p.emit(0x8D, 4, 0)
        if op == "div":
            p.address(0xBD, "dividends", 1)
            p.emit(0x8D, 5, 0)
        p.mark(name + "_inner")
        p.emit(0xAD, 2, 0, 0x29, 31)
        p.branch(0xD0, name + "_trial")
        p.puts("row")
        p.mark(name + "_trial")
        if op == "div":
            # Every trial starts with a known quotient; divide leaves RDDIV
            # untouched on trigger and shifts it during intermediate phases.
            p.emit(0xA9, 0, 0x8D, 2, 0x42, 0xA9, SEED_QUOTIENT, 0x8D, 3, 0x42,
                   *([0xEA] * 4), 0xAD, 4, 0, 0x8D, 4, 0x42,
                   0xAD, 5, 0, 0x8D, 5, 0x42, 0xAD, 2, 0, 0x8D, 6, 0x42)
        else:
            p.emit(0xAD, 4, 0, 0x8D, 2, 0x42, 0xAD, 2, 0, 0x8D, 3, 0x42)
        # DP read samples after two fetch clocks; absolute read after three.
        # NOP adds two clocks; LDA dp adds three, giving every phase >=2.
        if phase == 3:
            p.emit(0xAD, register, 0x42)
        else:
            delay = phase - 2
            if delay & 1:
                p.emit(0xA5, 2)  # harmless write-only I/O read, three clocks
                delay -= 3
            p.emit(*([0xEA] * (delay // 2)))
            p.emit(0xA5, register)
        p.emit(0x8D, 6, 0)  # Store observed byte before formatting.
        p.address(0x20, "hex")
        p.emit(0xEE, 2, 0, 0xAD, 2, 0, 0x29, 31)
        p.branch(0xD0, name + "_next")
        p.emit(0xA9, 10)
        p.address(0x20, "putc")
        p.mark(name + "_next")
        p.emit(0xAD, 2, 0)
        p.branch(0xF0, name + "_inner_done")
        p.address(0x4C, name + "_inner")
        p.mark(name + "_inner_done")
        p.emit(0xEE, 0, 0)
        if op == "div":
            p.emit(0xEE, 0, 0)
        p.emit(0xAD, 0, 0, 0xC9, len(MULTIPLICANDS) if op == "mul" else len(DIVIDENDS) * 2)
        p.branch(0xF0, name + "_done")
        p.address(0x4C, name + "_outer")
        p.mark(name + "_done")
    p.puts("complete")
    p.emit(0xA9, 1, 0x8D, 0xFF, 0x21)  # Unknown completion, not PASS.
    p.mark("halt")
    p.branch(0x80, "halt")
    p.mark("puts")
    p.emit(0xBD, 0, 0, 0xF0, 7)  # LDA $0000,X; BEQ return
    p.address(0x20, "putc")
    p.emit(0xE8)
    p.address(0x4C, "puts")
    p.emit(0x60)
    p.mark("putc")
    p.emit(0x8D, 0xFE, 0x21, 0x60)
    p.mark("hex")
    p.emit(0x48, 0x4A, 0x4A, 0x4A, 0x4A)
    p.address(0x20, "nibble")
    p.emit(0x68, 0x29, 15)
    p.address(0x20, "nibble")
    p.emit(0x60)
    p.mark("nibble")
    p.emit(0xC9, 10)
    p.branch(0x90, "digit")
    p.emit(0x18, 0x69, 0x57)
    p.address(0x4C, "putc")
    p.mark("digit")
    p.emit(0x18, 0x69, 0x30)
    p.address(0x4C, "putc")
    p.text("header", "VELOCE 1 snes math_timing\nINFO phases=2..18 seed=00a5\n")
    p.text("row", "REG bytes=")
    for op, register, phase in GROUPS:
        name = group_name(op, register, phase)
        p.text(name + "_begin", f"BEGIN {name}\n")
    p.text("complete", "LOG complete cases=504832\n")
    p.mark("multiplicands")
    p.emit(*MULTIPLICANDS)
    p.mark("dividends")
    for dividend in DIVIDENDS:
        p.emit(dividend & 255, dividend >> 8)
    p.resolve()
    p.data[0x7FC0:0x7FD5] = b"VELOCE MATH TIMING".ljust(21, b" ")
    p.data[0x7FD5:0x7FDB] = bytes((0x20, 0, 5, 0, 1, 0))
    for vector in (0x7FEA, 0x7FEE, 0x7FFA, 0x7FFC, 0x7FFE):
        p.data[vector:vector + 2] = p.labels["entry"].to_bytes(2, "little")
    # Four checksum bytes contribute 0x1fe regardless of the sum/complement.
    checksum = (sum(p.data[:0x7FDC]) + sum(p.data[0x7FE0:]) + 0x1FE) & 0xFFFF
    p.data[0x7FDC:0x7FE0] = (checksum ^ 0xFFFF).to_bytes(2, "little") + checksum.to_bytes(2, "little")
    return bytes(p.data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", nargs="?", type=Path, default=Path(__file__).with_name("math_timing.sfc"))
    args = parser.parse_args()
    data = build()
    args.output.write_bytes(data)
    print(f"{hashlib.sha256(data).hexdigest()}  {args.output}")
