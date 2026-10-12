#!/usr/bin/env python3
"""Original MIPS PI cartridge SRAM DMA test; no retail or SDK bytes."""
import argparse
import importlib.util
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('n64_assembler', HERE.parent / 'dp_registers/build.py')
asm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(asm)
SUITE = 'cartridge_sram'


def payload(block):
    return bytes((block * 17 + lane * 37 + (block >> 8)) & 255 for lane in range(64))


def build(size=96 * 1024, wrong_capture=False):
    if size not in (32 * 1024, 96 * 1024):
        raise ValueError(size)
    cases = size // 64
    p = asm.Program()
    p.data[:4] = bytes.fromhex('80371240')
    p.data[8:12] = bytes.fromhex('80000400')
    p.data[0x20:0x34] = b'VELOCE CART SRAM    '
    p.data[0x34:0x3c] = b'V64HB001'
    p.data[0x3c:0x40] = b'S32K' if size == 32 * 1024 else b'S96K'
    p.i(15, 0, 30, 0xbff0)
    p.i(15, 0, 24, 0xa460)  # PI registers
    p.i(15, 0, 10, 0xa070)  # source scratch in RDRAM
    p.i(15, 0, 11, 0xa070)
    p.i(13, 11, 11, 0x100)  # readback scratch
    p.i(15, 0, 18, 0x0800)  # PI cartridge SRAM
    p.i(15, 0, 19, 0x0070)  # source DRAM address
    p.i(15, 0, 20, 0x0070)
    p.i(13, 20, 20, 0x100)
    p.i(9, 0, 21, 63)       # 64-byte PI length minus one
    p.i(9, 0, 9, 2)         # PI interrupt acknowledge
    p.i(9, 0, 17, cases)
    p.address(16, 'table')
    p.puts('header32' if size == 32 * 1024 else 'header96')
    p.mark('sample')
    for offset in range(0, 64, 8):
        p.i(55, 16, 8, offset)
        p.i(63, 10, 8, offset)
    p.i(43, 24, 19, 0)
    p.i(43, 24, 18, 4)
    p.i(43, 24, 21, 8)    # RDRAM -> SRAM
    p.i(43, 24, 9, 16)    # clear first PI interrupt
    p.i(43, 24, 20, 0)
    p.i(43, 24, 18, 4)
    p.i(43, 24, 21, 12)   # SRAM -> RDRAM
    p.puts('row')
    for offset in range(0, 64, 8):
        p.i(55, 11, 5, offset)
        if wrong_capture and offset == 0:
            p.i(14, 5, 5, 1)
        p.jump(3, 'hex')
    for offset in (0, 4, 12, 16):
        p.i(39, 24, 5, offset)
        p.jump(3, 'hex')
    p.i(9, 0, 8, 10)
    p.i(40, 30, 8, 0)
    p.i(43, 24, 9, 16)
    p.i(9, 16, 16, 64)
    p.i(9, 18, 18, 64)
    p.i(9, 17, 17, -1)
    p.branch(5, 17, 0, 'sample')
    p.puts('complete32' if size == 32 * 1024 else 'complete96')
    p.i(43, 30, 0, 4)
    p.mark('halt')
    p.branch(4, 0, 0, 'halt')
    asm.output_routines(p)
    for n in (32 * 1024, 96 * 1024):
        count = n // 64
        suffix = '32' if n == 32 * 1024 else '96'
        p.string('header' + suffix,
                 f'VELOCE 1 n64 {SUITE}\nBEGIN sram bytes={n} cases={count} words=12\n')
        p.string('complete' + suffix,
                 f'LOG complete cases={count}\nEND observations_complete\n')
    p.string('row', 'REG r=')
    while p.pc & 7:
        p.data.append(0)
    p.mark('table')
    for block in range(cases):
        p.data.extend(payload(block))
    p.resolve()
    return bytes(p.data)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', nargs='?', type=Path, default=HERE / 'cartridge_sram.z64')
    parser.add_argument('--size', type=int, choices=(32768, 98304), default=98304)
    args = parser.parse_args()
    args.output.write_bytes(build(args.size))
