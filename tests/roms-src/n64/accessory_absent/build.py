#!/usr/bin/env python3
"""Original N64 no-accessory Joybus cartridge; no SDK or retail bytes."""
import argparse
import importlib.util
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('n64_assembler', HERE.parent / 'dp_registers/build.py')
asm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(asm)
SUITE = 'accessory_absent'
CASES = 4 * 256 * 2
WORDS = 8


def cases():
    for port in range(4):
        for seed in range(256):
            for command in (2, 3):
                yield port, seed, command


def packet(port, seed, command):
    data = bytearray([0xaa] * 64)
    data[:port] = bytes(port)
    tx, rx = (3, 33) if command == 2 else (35, 1)
    data[port:port + 5] = bytes((tx, rx, command, seed, seed * 73 & 255))
    if command == 3:
        for lane in range(32):
            data[port + 5 + lane] = (seed * 17 + lane * 53) & 255
    data[port + 2 + tx + rx] = 254
    data[63] = 1
    return bytes(data)


def build(wrong_capture=False):
    p = asm.Program()
    p.data[:4] = bytes.fromhex('80371240')
    p.data[8:12] = bytes.fromhex('80000400')
    p.data[0x20:0x34] = b'VELOCE PAK ABSENT   '
    p.data[0x34:0x3c] = b'V64HB001'
    p.i(15, 0, 30, 0xbff0)  # debug channel
    p.i(15, 0, 24, 0xa480)  # SI registers
    p.i(15, 0, 10, 0xa003)  # uncached SI scratch
    p.address(16, 'table')
    p.puts('header')
    p.i(9, 0, 17, CASES)
    p.mark('sample')
    for offset in range(0, 64, 8):
        p.i(55, 16, 11, offset)
        p.i(63, 10, 11, offset)
    p.i(43, 24, 10, 0)  # SI DRAM address
    p.i(43, 24, 0, 16)  # format guest packet
    p.i(43, 24, 0, 24)  # acknowledge SI completion
    p.i(43, 24, 0, 4)   # poll and transfer reply
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
    p.string('header', f'VELOCE 1 n64 {SUITE}\nBEGIN accessory cases={CASES} words={WORDS}\n')
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
    parser.add_argument('output', nargs='?', type=Path,
                        default=HERE / 'accessory_absent.z64')
    args = parser.parse_args()
    args.output.write_bytes(build())
