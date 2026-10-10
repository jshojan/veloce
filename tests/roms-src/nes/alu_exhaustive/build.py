#!/usr/bin/env python3
"""Original NROM program that streams every ADC/SBC A/P result to $401E.

The ROM contains no expected arithmetic results or pass verdicts. The host
oracle in run.py grades the observed bytes. Python 3 is the only build tool.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

GROUPS = tuple((op, carry, decimal)
               for op in ("adc", "sbc")
               for decimal in (0, 1)
               for carry in (0, 1))
ORIGIN = 0xC000


def group_name(op: str, carry: int, decimal: int) -> str:
    return f"{op}_c{carry}_d{decimal}"


class Program:
    def __init__(self):
        self.data = bytearray([0xEA] * 0x4000)
        self.pos = 0
        self.labels: dict[str, int] = {}
        self.fixups: list[tuple[int, str, bool]] = []

    def emit(self, *values: int):
        if self.pos + len(values) > 0x3FFA:
            raise ValueError("program overlaps NROM interrupt vectors")
        self.data[self.pos:self.pos + len(values)] = bytes(values)
        self.pos += len(values)

    def mark(self, name: str):
        if name in self.labels:
            raise ValueError(f"duplicate label: {name}")
        self.labels[name] = self.pos + ORIGIN

    def absolute(self, opcode: int, name: str):
        self.emit(opcode, 0, 0)
        self.fixups.append((self.pos - 2, name, False))

    def branch(self, opcode: int, name: str):
        self.emit(opcode, 0)
        self.fixups.append((self.pos - 1, name, True))

    def text(self, value: str):
        for byte in value.encode("ascii"):
            self.emit(0xA9, byte, 0x8D, 0x1E, 0x40)  # LDA #byte; STA $401E

    def resolve(self):
        for offset, name, relative in self.fixups:
            address = self.labels[name]
            if relative:
                delta = address - (ORIGIN + offset + 1)
                if not -128 <= delta <= 127:
                    raise ValueError(f"branch out of range: {name}")
                self.data[offset] = delta & 0xFF
            else:
                self.data[offset:offset + 2] = address.to_bytes(2, "little")


def build(operands: int = 256) -> bytes:
    if operands not in (1, 2, 4, 8, 16, 32, 64, 128, 256):
        raise ValueError("operand count must be a power of two from 1 to 256")
    p = Program()
    p.mark("entry")
    p.emit(0x78, 0xD8, 0xA2, 0xFF, 0x9A)  # SEI; CLD; LDX #$FF; TXS
    p.emit(0xA9, 0)
    for address in (0x2000, 0x2001, 0x4010, 0x4015):
        p.emit(0x8D, address & 0xFF, address >> 8)  # Blank PPU, disable DMC/APU
    p.emit(0xA9, 0x40, 0x8D, 0x17, 0x40)  # Disable frame-counter IRQ
    p.text("VELOCE 1 nes alu_exhaustive\n")
    p.text(f"INFO operands={operands} groups={len(GROUPS)}\n")
    p.text("REG probe=")
    p.emit(0xAD, 0x1F, 0x40)  # Read optional test-port signature
    p.absolute(0x20, "hex")
    p.text("\n")

    # Zero-page 00/01 hold lhs/rhs; 02/03 hold the observed A/P bytes.
    for op, carry, decimal in GROUPS:
        group = group_name(op, carry, decimal)
        p.text(f"BEGIN {group}\n")
        p.emit(0xA9, 0, 0x85, 0, 0x85, 1)
        p.mark(f"{group}_loop")
        p.emit(0xA5, 1, 0x29, min(32, operands) - 1)  # Prefix each packed row
        p.branch(0xD0, f"{group}_sample")
        p.text("REG ap=")
        p.mark(f"{group}_sample")
        # Set C/D/I and seed V=1 so cases that clear V also test overwrite.
        # Load A before PLP so it does not disturb the restored input flags.
        p.emit(0xA9, 0x64 | carry | (decimal << 3), 0x48, 0xA5, 0, 0x28)
        p.emit(0x65 if op == "adc" else 0xE5, 1)  # ADC/SBC $01
        p.emit(0x85, 2, 0x08, 0x68, 0x85, 3)  # Save A, then PHP; PLA; save P
        for address in (2, 3):
            p.emit(0xA5, address)
            p.absolute(0x20, "hex")
        p.emit(0xE6, 1, 0xA5, 1, 0x29, min(32, operands) - 1)
        p.branch(0xD0, f"{group}_next")
        p.text("\n")
        p.mark(f"{group}_next")
        p.emit(0xA5, 1)
        if operands != 256:
            p.emit(0xC9, operands)
        p.branch(0xD0, f"{group}_loop")
        if operands != 256:
            p.emit(0xA9, 0, 0x85, 1)
        p.emit(0xE6, 0, 0xA5, 0)
        if operands != 256:
            p.emit(0xC9, operands)
        p.branch(0xD0, f"{group}_loop")

    p.text(f"LOG complete cases={len(GROUPS) * operands * operands}\n")
    # Completion is deliberately unknown to the core; only the host oracle
    # can turn the raw observed register corpus into PASS or FAIL.
    p.emit(0xA9, 1, 0x8D, 0x1F, 0x40)
    p.mark("idle")
    p.absolute(0x4C, "idle")

    p.mark("hex")
    p.emit(0x48, 0x4A, 0x4A, 0x4A, 0x4A, 0xAA)  # PHA; four LSR A; TAX
    p.absolute(0xBD, "digits")
    p.emit(0x8D, 0x1E, 0x40, 0x68, 0x29, 0x0F, 0xAA)
    p.absolute(0xBD, "digits")
    p.emit(0x8D, 0x1E, 0x40, 0x60)
    p.mark("digits")
    p.emit(*b"0123456789abcdef")
    p.resolve()
    for vector in (0x3FFA, 0x3FFC, 0x3FFE):
        p.data[vector:vector + 2] = p.labels["entry"].to_bytes(2, "little")
    header = b"NES\x1a" + bytes((1, 0)) + bytes(10)  # 16 KiB NROM, CHR RAM
    return header + bytes(p.data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", nargs="?", type=Path,
                        default=Path(__file__).with_name("alu_exhaustive.nes"))
    args = parser.parse_args()
    data = build()
    args.output.write_bytes(data)
    print(f"{hashlib.sha256(data).hexdigest()}  {args.output}")
