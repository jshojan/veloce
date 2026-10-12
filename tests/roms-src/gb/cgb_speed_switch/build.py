#!/usr/bin/env python3
"""Build an original CGB KEY1/STOP register-output cartridge."""
from pathlib import Path
import hashlib


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

    def address(self, opcode, name):
        self.emit(opcode, 0, 0)
        self.fixups.append((self.pos - 2, name, False))

    def relative(self, opcode, name):
        self.emit(opcode, 0)
        self.fixups.append((self.pos - 1, name, True))

    def string(self, name, value):
        self.mark(name)
        self.emit(*value.encode('ascii'), 0)

    def puts(self, name):
        self.address(0x21, name)
        self.address(0xCD, 'puts')

    def resolve(self):
        for offset, name, relative in self.fixups:
            address = self.labels[name]
            if relative:
                delta = address - offset - 1
                if not -128 <= delta <= 127:
                    raise ValueError(f'JR out of range: {name}')
                self.data[offset] = delta & 255
            else:
                self.data[offset:offset + 2] = address.to_bytes(2, 'little')


def build(arm_value=1):
    rom = Rom()
    rom.data[0x100:0x104] = bytes((0xC3, 0x50, 0x01, 0))
    rom.data[0x134:0x143] = b'VELOCE CGB SPEED'
    rom.data[0x143] = 0x80  # CGB-compatible cartridge, not DMG-only.
    rom.data[0x147:0x14A] = bytes((0, 0, 0))
    rom.emit(0xF3, 0x31, 0xFE, 0xDF, 0x0E, 0)  # DI; SP=$DFFE; C=passed reads
    rom.puts('header')

    def observe(name, expected):
        rom.emit(0xF0, 0x4D, 0xF5)  # LDH A,(KEY1); PUSH AF
        rom.puts(name)
        rom.emit(0xF1, 0xF5)  # Restore observed A for hex output.
        rom.address(0xCD, 'hex')
        rom.puts('newline')
        rom.emit(0xF1, 0xFE, expected)  # Compare actual register read.
        rom.relative(0x20, name + '_bad')
        rom.emit(0x0C)  # INC C
        rom.mark(name + '_bad')

    def domain(name):
        # Sixteen short rows preserve every output byte through the file sink.
        # The host grades all 256 pairs; only bit 0 may be written.
        rom.emit(0x16, 0, 0x1E, 16)  # D=stimulus, E=rows remaining
        rom.mark(name + '_row')
        rom.puts(name)
        rom.emit(0x06, 16)  # B=values remaining in this row
        rom.mark(name + '_loop')
        rom.emit(0x7A, 0xE0, 0x4D, 0xF0, 0x4D)  # KEY1=D; A=KEY1
        rom.address(0xCD, 'hex')
        rom.emit(0x14, 0x05)  # D++; B--
        rom.relative(0x20, name + '_loop')
        rom.puts('newline')
        rom.emit(0x1D)  # E--
        rom.relative(0x20, name + '_row')

    observe('normal', 0x7E)
    domain('normal_domain')
    rom.emit(0x3E, arm_value, 0xE0, 0x4D)  # Arm speed switch.
    observe('armed', 0x7F)
    rom.emit(0x10, 0)  # STOP $00: normal -> double speed.
    observe('double', 0xFE)
    domain('double_domain')
    rom.emit(0x3E, 1, 0xE0, 0x4D)
    observe('armed_double', 0xFF)
    rom.emit(0x10, 0)  # STOP $00: double -> normal speed.
    observe('restored', 0x7E)

    rom.emit(0x79, 0xFE, 5)  # All five actual KEY1 reads matched?
    rom.relative(0x20, 'fail')
    rom.puts('pass')
    rom.relative(0x18, 'halt')
    rom.mark('fail')
    rom.puts('failure')
    rom.mark('halt')
    rom.relative(0x18, 'halt')

    rom.mark('puts')
    rom.emit(0x2A, 0xB7, 0xC8)  # LD A,(HL+); OR A; RET Z
    rom.address(0xCD, 'putc')
    rom.relative(0x18, 'puts')
    rom.mark('putc')
    rom.emit(0xE0, 0x01, 0x3E, 0x81, 0xE0, 0x02)  # SB=A; SC=$81
    rom.mark('wait_serial')
    rom.emit(0xF0, 0x02, 0xCB, 0x7F)  # Wait for internal serial completion.
    rom.relative(0x20, 'wait_serial')
    rom.emit(0xC9)
    rom.mark('hex')
    rom.emit(0xF5, 0xCB, 0x37, 0xE6, 0x0F)
    rom.address(0xCD, 'nibble')
    rom.emit(0xF1, 0xE6, 0x0F)
    rom.address(0xCD, 'nibble')
    rom.emit(0xC9)
    rom.mark('nibble')
    rom.emit(0xFE, 10)
    rom.relative(0x38, 'digit')
    rom.emit(0xC6, 0x37)
    rom.address(0xC3, 'putc')
    rom.mark('digit')
    rom.emit(0xC6, 0x30)
    rom.address(0xC3, 'putc')

    rom.string('header', 'VELOCE 1 gb cgb_speed_switch\n')
    rom.string('normal', 'REG normal=')
    rom.string('normal_domain', 'REG normal_domain=')
    rom.string('armed', 'REG armed=')
    rom.string('double', 'REG double=')
    rom.string('double_domain', 'REG double_domain=')
    rom.string('armed_double', 'REG armed_double=')
    rom.string('restored', 'REG restored=')
    rom.string('newline', '\n')
    rom.string('pass', 'CHECK 1 PASS key1_stop_roundtrip\nEND 1/1\n')
    rom.string('failure', 'CHECK 1 FAIL key1_stop_roundtrip\nEND 0/1 code=1\n')
    rom.resolve()
    rom.data[0x14D] = (-sum(rom.data[0x134:0x14D]) - 25) & 255
    return bytes(rom.data)


if __name__ == '__main__':
    output = Path(__file__).with_name('cgb_speed_switch.gb')
    image = build()
    output.write_bytes(image)
    print(f'{hashlib.sha256(image).hexdigest()}  {output}')
