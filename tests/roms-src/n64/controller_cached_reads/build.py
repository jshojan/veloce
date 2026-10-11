#!/usr/bin/env python3
"""Original MIT SI repeated-read cartridge; no SDK or expected guest verdicts."""
import argparse,importlib.util
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('dp_assembler',HERE.parent/'dp_registers'/'build.py');asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
SUITE='controller_cached_reads'
def build(cases=16384,wrong_capture=False):
    if not 1<=cases<=16384:raise ValueError('invalid case count')
    p=asm.Program();asm.header(p,cases,16)
    p.i(15,0,24,0xa480);p.i(15,0,25,0xa430);p.i(15,0,27,0xa002)
    p.address(16,'packet')
    for offset in range(0,64,8):p.i(55,16,8,offset);p.i(63,27,8,offset)
    p.i(15,0,8,2);p.i(43,24,8,0);p.i(9,0,8,8);p.i(43,25,8,12)
    p.i(43,24,0,16);p.i(43,24,0,24) # Exactly one format DMA for the entire run.
    p.puts('header');p.i(9,0,17,cases);p.mark('sample');p.puts('row')
    for phase in range(2):
        if phase:p.puts('row')
        p.i(43,24,0,4) # Read refreshes current input without another format write.
        for offset in (0,8,16,24,56):
            p.i(55,27,5,offset)
            if wrong_capture and offset==0:p.i(14,5,5,1)
            p.jump(3,'hex')
        for reg,offset in ((24,24),(25,8)):p.i(39,reg,5,offset);p.jump(3,'hex')
        if phase==0:p.i(9,0,8,10);p.i(40,30,8,0);p.i(43,30,0,12) # Host changes its actual input between the reads.
    p.i(43,24,0,24)
    for reg,offset in ((24,24),(25,8)):p.i(39,reg,5,offset);p.jump(3,'hex')
    p.i(9,0,8,10);p.i(40,30,8,0);p.i(43,30,0,12)
    p.i(9,17,17,-1);p.branch(5,17,0,'sample');p.puts('complete');p.i(43,30,0,4);p.mark('halt');p.branch(4,0,0,'halt');asm.output_routines(p)
    p.string('header',f'VELOCE 1 n64 {SUITE}\nBEGIN digital cases={cases} words=16 chunks=7,9\n');p.string('row','REG r=');p.string('complete',f'LOG complete cases={cases}\nEND observations_complete\n')
    while p.pc&7:p.data.append(0)
    p.mark('packet');packet=bytearray([0xaa]*64)
    for port in range(4):packet[port*8:port*8+8]=bytes([255,1,4,1,0xaa,0xaa,0xaa,0xaa])
    packet[32]=254;packet[63]=1;p.data.extend(packet);p.mark('table');p.word(0);p.resolve();return bytes(p.data)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=HERE/(SUITE+'.z64'));args=parser.parse_args();args.output.write_bytes(build())
