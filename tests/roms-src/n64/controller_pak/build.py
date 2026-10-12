#!/usr/bin/env python3
"""Original MIPS Joybus Controller Pak ROM; no SDK or retail bytes."""
import argparse
import importlib.util
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('n64_assembler', HERE.parent / 'dp_registers/build.py')
asm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(asm)
SUITE = 'controller_pak'
BLOCKS = 1024
SYSTEM_BLOCKS = 40
NOTE_BLOCKS = (40, 41, 42, 43, 44, 45, 46, 47, 8, 16, 24)
CASES = 4 + 4 * (SYSTEM_BLOCKS + len(NOTE_BLOCKS) * 2 + BLOCKS * 3)


def address_crc(offset):
    crc = 0
    for bit in range(15, 4, -1):
        feedback = ((offset >> bit) & 1) ^ ((crc >> 4) & 1)
        crc = (crc << 1) & 31
        if feedback:
            crc ^= 0x15
    return crc


def payload(port, block):
    return bytes((block & 255, block >> 8, port) +
                 tuple((port * 71 + block * 17 + (block >> 8) * 43 + lane * 29) & 255
                       for lane in range(3, 32)))


def note_data(port, block):
    if 40 <= block < 48:
        return bytes((port * 23 + block * 7 + lane * 19) & 255 for lane in range(32))
    if block in (8, 16):
        inode = bytearray(32)
        inode[1] = (122 * 3 + 1) & 255
        for slot in range(5, 16):
            inode[slot * 2 + 1] = 1 if slot == 5 else 3
        return bytes(inode)
    if block == 24:
        note = bytearray(32)
        note[:10] = bytes((0x56, 0x4c, 0x4f, 0x45, 0x50, 0x4b, 0, 5, 2, 3))
        note[12] = 0x1a  # extension A
        note[16:20] = bytes((0x2d, 0x1e, 0x2c, 0x2d))  # TEST
        return bytes(note)
    raise ValueError(block)


def cases():
    for port in range(4):
        yield port, 0, 0, 'status'
        for block in range(SYSTEM_BLOCKS):
            yield port, block, 2, 'fresh'
        for block in NOTE_BLOCKS:
            yield port, block, 3, 'note'
        for block in NOTE_BLOCKS:
            yield port, block, 2, 'note'
        for block in range(BLOCKS):
            yield port, block, 3, 'bulk'
            yield port, block, 2, 'bulk'
        for block in range(BLOCKS):
            yield port, block, 2, 'bulk'


def packet(port, block, command, phase):
    data = bytearray([0xaa] * 64)
    data[:port] = bytes(port)
    tx, rx = (1, 3) if command == 0 else (3, 33) if command == 2 else (35, 1)
    data[port:port + 3] = bytes((tx, rx, command))
    if command:
        address = block * 32 | address_crc(block * 32)
        data[port + 3:port + 5] = address.to_bytes(2, 'big')
    if command == 3:
        data[port + 5:port + 37] = note_data(port, block) if phase == 'note' else payload(port, block)
    data[port + 2 + tx + rx] = 254
    data[63] = 1
    return data


def build(wrong_capture=False):
    p = asm.Program()
    p.data[:4] = bytes.fromhex('80371240')
    p.data[8:12] = bytes.fromhex('80000400')
    p.data[0x20:0x34] = b'VELOCE PAK INSERT   '
    p.data[0x34:0x3c] = b'V64HB001'
    p.i(15, 0, 30, 0xbff0)
    p.i(15, 0, 24, 0xa480)
    p.i(15, 0, 10, 0xa070)  # 7 MiB scratch, clear of the 768 KiB packet table
    p.address(16, 'table')
    p.puts('header')
    p.i(9, 0, 17, CASES)
    p.mark('sample')
    for offset in range(0, 64, 8):
        p.i(55, 16, 11, offset)
        p.i(63, 10, 11, offset)
    p.i(43, 24, 10, 0)
    p.i(43, 24, 0, 16)
    p.i(43, 24, 0, 24)
    p.i(43, 24, 0, 4)
    p.puts('row')
    for offset in range(0, 64, 8):
        p.i(55, 10, 5, offset)
        if wrong_capture and offset == 0:
            p.i(14, 5, 5, 1)
        p.jump(3, 'hex')
    p.i(9, 0, 8, 10)
    p.i(40, 30, 8, 0)
    p.i(9, 16, 16, 64)
    p.i(9, 17, 17, -1)
    p.branch(5, 17, 0, 'sample')
    p.puts('complete')
    p.i(43, 30, 0, 4)
    p.mark('halt')
    p.branch(4, 0, 0, 'halt')
    asm.output_routines(p)
    p.string('header', f'VELOCE 1 n64 {SUITE}\nBEGIN pak cases={CASES} words=8\n')
    p.string('row', 'REG r=')
    p.string('complete', f'LOG complete cases={CASES}\nEND observations_complete\n')
    while p.pc & 7:
        p.data.append(0)
    p.mark('table')
    for item in cases():
        p.data.extend(packet(*item))
    p.resolve()
    return bytes(p.data)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', nargs='?', type=Path, default=HERE / 'controller_pak.z64')
    args = parser.parse_args()
    args.output.write_bytes(build())
