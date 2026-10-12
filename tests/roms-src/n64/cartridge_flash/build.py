#!/usr/bin/env python3
"""Original MIPS N64 FlashRAM command and PI DMA cartridge."""
import argparse
import importlib.util
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('n64_assembler', HERE.parent / 'dp_registers/build.py')
asm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(asm)
SUITE = 'cartridge_flash'
CASES = 1024


def payload(page):
    return bytes((page * 17 + lane * 37 + (page >> 8)) & 255 for lane in range(128))


def build(wrong_capture=False):
    p = asm.Program()
    p.data[:4] = bytes.fromhex('80371240')
    p.data[8:12] = bytes.fromhex('80000400')
    p.data[0x20:0x34] = b'VELOCE CART FLASH   '
    p.data[0x34:0x3c] = b'V64HB001'
    p.data[0x3c:0x40] = b'F128'
    p.i(15, 0, 30, 0xbff0)
    p.i(15, 0, 24, 0xa460)  # PI registers
    p.i(15, 0, 25, 0xa801)  # FlashRAM command register
    p.i(15, 0, 10, 0xa070)  # source scratch
    p.i(15, 0, 11, 0xa070)
    p.i(13, 11, 11, 0x100)  # readback scratch
    p.i(15, 0, 18, 0x0800)  # array DMA address
    p.i(15, 0, 19, 0x0070)  # source physical DRAM
    p.i(15, 0, 20, 0x0070)
    p.i(13, 20, 20, 0x100)
    p.i(9, 0, 21, 127)
    p.i(9, 0, 9, 2)
    p.i(9, 0, 17, CASES)
    p.i(9, 0, 22, 0)       # page number
    p.address(16, 'table')
    p.puts('header')
    p.i(15, 0, 8, 0xe100)
    p.i(43, 25, 8, 0)
    p.i(43, 24, 20, 0)
    p.i(15, 0, 8, 0x0800)
    p.i(43, 24, 8, 4)
    p.i(9, 0, 8, 7)
    p.i(43, 24, 8, 12)
    p.puts('idrow')
    p.i(55, 11, 5, 0)
    p.jump(3, 'hex')
    p.i(9, 0, 8, 10)
    p.i(40, 30, 8, 0)
    p.i(43, 24, 9, 16)
    p.i(15, 0, 8, 0xf000)
    p.i(43, 25, 8, 0)
    p.mark('sample')
    for offset in range(0, 128, 8):
        p.i(55, 16, 8, offset)
        p.i(63, 10, 8, offset)
    p.i(15, 0, 8, 0xb400)
    p.i(43, 25, 8, 0)
    p.i(43, 24, 19, 0)
    p.i(15, 0, 8, 0x0800)
    p.i(43, 24, 8, 4)
    p.i(43, 24, 21, 8)    # load 128-byte Flash page buffer
    p.i(43, 24, 9, 16)
    p.i(15, 0, 8, 0xa500)
    p.r(0x25, 8, 22, 8)
    p.i(43, 25, 8, 0)    # program page
    p.i(15, 0, 8, 0xf000)
    p.i(43, 25, 8, 0)    # array mode
    p.i(43, 24, 20, 0)
    p.i(43, 24, 18, 4)
    p.i(43, 24, 21, 12)  # read 128 bytes
    p.puts('row')
    for offset in range(0, 128, 8):
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
    p.i(9, 16, 16, 128)
    p.i(9, 18, 18, 128)
    p.i(9, 22, 22, 1)
    p.i(9, 17, 17, -1)
    p.branch(5, 17, 0, 'sample')
    p.puts('complete')
    p.i(43, 30, 0, 4)
    p.mark('halt')
    p.branch(4, 0, 0, 'halt')
    asm.output_routines(p)
    p.string('header', f'VELOCE 1 n64 {SUITE}\nBEGIN flash pages={CASES} words=20\n')
    p.string('idrow', 'REG id=')
    p.string('row', 'REG r=')
    p.string('complete', f'LOG complete pages={CASES}\nEND observations_complete\n')
    while p.pc & 7:
        p.data.append(0)
    p.mark('table')
    for page in range(CASES):
        p.data.extend(payload(page))
    p.resolve()
    return bytes(p.data)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', nargs='?', type=Path, default=HERE / 'cartridge_flash.z64')
    args = parser.parse_args()
    args.output.write_bytes(build())
