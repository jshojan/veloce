#!/usr/bin/env python3
"""Original MIT variable-length RDP packet/END-extension register cartridge."""
import argparse,importlib.util
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('dp_assembler',HERE.parent/'dp_registers'/'build.py')
asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
SUITE='rdp_packets'
def cases(short=False):
    for payload in ((0,255) if short else range(256)):
        for alias in ((0,) if short else range(4)):
            for op,length in ((8,4),(9,6),(10,12),(11,14),(12,12),(13,14),(14,20),(15,22),(36,2),(37,2)):
                for source in range(2):
                    for split in (sorted(set((1,length-1))) if short else range(1,length)):
                        yield payload,op+alias*64,source,split,length

def build(short=False,wrong_capture=False):
    samples=list(cases(short));p=asm.Program();asm.header(p,len(samples),7)
    p.i(15,0,27,0xa070);p.i(15,0,28,0xa430);p.puts('header');p.mark('sample')
    p.i(39,16,12,0);p.i(39,16,13,4);p.i(55,16,9,8);p.i(55,16,11,16)
    p.i(9,0,8,0x800);p.i(43,28,8,0)
    p.i(15,0,8,0x5a5a);p.i(13,8,8,0x5a5a);p.r(0x3c,rt=8,rd=10);p.r(37,8,10,8);p.i(63,27,8,64)
    p.r(2,rt=12,rd=15,sa=16);p.i(12,15,15,1)
    p.i(13,27,18,256);p.i(9,0,20,0x15)
    p.branch(4,15,0,'source_done');p.i(13,25,18,0xff8);p.i(9,0,20,0x16)
    p.mark('source_done');p.r(37,18,0,14);p.i(63,14,9,0);p.i(9,13,10,-1)
    p.mark('copy');p.i(9,14,14,8);p.branch(4,15,0,'copy_no_wrap');p.i(12,14,14,4095);p.r(37,25,14,14)
    p.mark('copy_no_wrap');p.i(63,14,11,0);p.i(9,10,10,-1);p.branch(5,10,0,'copy')
    p.i(43,24,20,12);p.i(12,18,18,65535);p.branch(5,15,0,'start_done');p.i(15,0,18,0x70);p.i(13,18,18,256)
    p.mark('start_done');p.r(0,rt=13,rd=22,sa=3);p.r(33,18,22,22)
    p.r(2,rt=12,rd=23,sa=24);p.r(0,rt=23,rd=23,sa=3);p.r(33,18,23,19)
    p.i(43,24,18,0);p.i(43,24,19,4)
    p.mark('wait_first');p.i(39,24,8,8);p.branch(5,8,19,'wait_first');p.puts('row')
    for addr,offset in ((24,8),(24,12),(28,8)):
        p.i(39,addr,5,offset)
        if wrong_capture and (addr,offset)==(24,8):p.i(14,5,5,1)
        p.jump(3,'hex')
    p.i(43,24,22,4);p.mark('wait_complete');p.i(39,24,8,8);p.branch(5,8,22,'wait_complete')
    for addr,offset in ((24,8),(24,12),(28,8)):p.i(39,addr,5,offset);p.jump(3,'hex')
    p.i(55,27,5,64);p.jump(3,'hex');p.i(9,0,8,10);p.i(40,30,8,0)
    p.i(9,16,16,24);p.i(9,17,17,-1);p.branch(5,17,0,'sample')
    p.puts('complete');p.i(43,30,0,4);p.mark('halt');p.branch(4,0,0,'halt');asm.output_routines(p)
    p.string('header',f'VELOCE 1 n64 {SUITE}\nBEGIN data cases={len(samples)} words=7\n');p.string('row','REG r=');p.string('complete',f'LOG complete cases={len(samples)}\nEND observations_complete\n')
    while p.pc&7:p.data.append(0)
    p.mark('table')
    for payload,byte,source,split,length in samples:
        p.word(payload|(byte<<8)|(source<<16)|(split<<24));p.word(length)
        repeated=payload*0x01010101010101
        for value in ((byte<<56)|repeated,0x2900000000000000|repeated):p.data.extend(value.to_bytes(8,'big'))
    p.resolve();return bytes(p.data)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=HERE/(SUITE+'.z64'));args=parser.parse_args();args.output.write_bytes(build())
