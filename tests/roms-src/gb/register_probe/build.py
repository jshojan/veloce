#!/usr/bin/env python3
"""Build Veloce's original DMG CPU/WRAM register probe without an assembler.

All instructions below are SM83 opcodes. Labels are resolved after emission so
the checked-in ROM can be reproduced on any machine with Python 3.
"""
from pathlib import Path
import hashlib
import sys


class Rom:
    def __init__(self):
        self.data = bytearray(0x8000)
        self.pos = 0x150
        self.labels = {}
        self.fixups = []

    def mark(self, name):
        self.labels[name] = self.pos

    def emit(self, *values):
        for value in values:
            self.data[self.pos] = value
            self.pos += 1

    def jump(self, opcode, name):
        self.emit(opcode, 0, 0)
        self.fixups.append((self.pos - 2, name, False))

    def relative(self, opcode, name):
        self.emit(opcode, 0)
        self.fixups.append((self.pos - 1, name, True))

    def string(self, label, value):
        self.mark(label)
        self.emit(*value.encode("ascii"), 0)

    def print_string(self, name):
        self.emit(0x21, 0, 0)  # LD HL, string
        self.fixups.append((self.pos - 2, name, False))
        self.jump(0xCD, "puts")

    def resolve(self):
        for offset, name, relative in self.fixups:
            address = self.labels[name]
            if relative:
                delta = address - (offset + 1)
                if not -128 <= delta <= 127:
                    raise ValueError(f"JR out of range: {name}")
                self.data[offset] = delta & 255
            else:
                self.data[offset:offset + 2] = address.to_bytes(2, "little")


def build():
    rom = Rom()
    rom.data[0x100:0x104] = bytes((0xC3, 0x50, 0x01, 0x00))  # JP $0150
    rom.data[0x134:0x143] = b"VELOCE REG TEST"
    rom.data[0x147:0x14A] = bytes((0, 0, 0))  # ROM only, 32 KiB, no RAM

    rom.emit(0xF3, 0x31, 0xFE, 0xFF, 0x06, 0x00)  # DI; SP=$FFFE; B=passed checks
    rom.emit(0x3E, 0x5A, 0xEA, 0x00, 0xC0)  # LD A,$5A; LD ($C000),A
    rom.emit(0xFA, 0x00, 0xC0, 0xF5)  # LD A,($C000); PUSH AF
    rom.print_string("wram")
    rom.emit(0xF1, 0xF5)  # POP AF; PUSH AF
    rom.jump(0xCD, "hex")
    rom.print_string("newline")
    rom.emit(0xF1, 0xFE, 0x5A)  # POP AF; CP $5A
    rom.relative(0x20, "wram_fail")
    rom.print_string("wram_pass")
    rom.emit(0x04)  # INC B
    rom.relative(0x18, "alu_test")
    rom.mark("wram_fail")
    rom.print_string("wram_bad")

    rom.mark("alu_test")
    rom.emit(0x3E, 0x0F, 0xC6, 0x01, 0xF5)  # A=$0F; ADD $01; PUSH AF
    rom.print_string("alu")
    rom.emit(0xF1, 0xF5)
    rom.jump(0xCD, "hex")
    rom.print_string("newline")
    rom.emit(0xF1, 0xFE, 0x10)  # A result == $10?
    rom.relative(0x20, "alu_fail")
    rom.print_string("alu_pass")
    rom.emit(0x04)
    rom.relative(0x18, "done")
    rom.mark("alu_fail")
    rom.print_string("alu_bad")
    rom.mark("done")
    rom.emit(0x78, 0xFE, 0x02)  # LD A,B; CP 2
    rom.relative(0x28, "two_pass")
    rom.emit(0xFE, 0x01)  # CP 1
    rom.relative(0x28, "one_pass")
    rom.print_string("end_zero")
    rom.relative(0x18, "halt")
    rom.mark("one_pass")
    rom.print_string("end_one")
    rom.relative(0x18, "halt")
    rom.mark("two_pass")
    rom.print_string("end_two")
    rom.mark("halt")
    rom.relative(0x18, "halt")

    rom.mark("puts")
    rom.mark("puts_loop")
    rom.emit(0x2A, 0xB7, 0xC8)  # LD A,(HL+); OR A; RET Z
    rom.jump(0xCD, "putc")
    rom.relative(0x18, "puts_loop")
    rom.mark("putc")
    rom.emit(0xE0, 0x01, 0x3E, 0x81, 0xE0, 0x02)  # SB=A; SC=$81
    rom.mark("wait_serial")
    rom.emit(0xF0, 0x02, 0xCB, 0x7F)  # LDH A,(SC); BIT 7,A
    rom.relative(0x20, "wait_serial")
    rom.emit(0xC9)
    rom.mark("hex")
    rom.emit(0xF5, 0xCB, 0x37, 0xE6, 0x0F)  # PUSH AF; SWAP A; AND $0F
    rom.jump(0xCD, "nibble")
    rom.emit(0xF1, 0xE6, 0x0F)
    rom.jump(0xCD, "nibble")
    rom.emit(0xC9)
    rom.mark("nibble")
    rom.emit(0xFE, 0x0A)
    rom.relative(0x38, "digit")
    rom.emit(0xC6, 0x37)
    rom.jump(0xC3, "putc")
    rom.mark("digit")
    rom.emit(0xC6, 0x30)
    rom.jump(0xC3, "putc")

    rom.string("wram", "REG wram=")
    rom.string("alu", "REG alu=")
    rom.string("newline", "\n")
    rom.string("wram_pass", "CHECK 1 PASS wram_roundtrip\n")
    rom.string("wram_bad", "CHECK 1 FAIL wram_roundtrip exp=5a\n")
    rom.string("alu_pass", "CHECK 2 PASS add_result\n")
    rom.string("alu_bad", "CHECK 2 FAIL add_result exp=10\n")
    rom.string("end_zero", "END 0/2 code=1\n")
    rom.string("end_one", "END 1/2 code=1\n")
    rom.string("end_two", "END 2/2\n")
    rom.resolve()
    rom.data[0x14D] = (-sum(rom.data[0x134:0x14D]) - 25) & 255
    return bytes(rom.data)


if __name__ == "__main__":
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_name("register_probe.gb")
    data = build()
    output.write_bytes(data)
    print(f"{hashlib.sha256(data).hexdigest()}  {output}")
