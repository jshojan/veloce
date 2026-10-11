#!/usr/bin/env python3
"""Original MIT CPU/EBUS RAM and register observations; no expected results."""
import argparse,importlib.util
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('dp_assembler',HERE.parent/'dp_registers'/'build.py')
asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
SUITE='hidden_rdram'
def cases(short=False):
    return range(256 if short else 65536)
def build(short=False,wrong_capture=False):
    rows=list(cases(short));p=asm.Program();asm.SUITE=SUITE;asm.header(p,len(rows),9)
    p.i(15,0,18,0xa070);p.i(15,0,28,0xa430)
    p.address(8,'helper');p.i(13,25,9,0x800);p.i(9,0,10,18)
    p.mark('copy');p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,'copy')
    p.puts('header');p.mark('sample');p.i(55,16,19,0);p.i(9,0,8,-1)
    for offset in (0,8,16,24):p.i(63,18,8,offset)
    for op,offset in ((40,0),(41,2),(43,4),(63,8)):p.i(op,18,19,offset)
    p.i(13,25,8,0x800);p.r(9,rs=8,rd=31);p.word(0)
    if wrong_capture:p.i(14,20,20,1)
    p.puts('row')
    for offset in (0,8):p.i(55,18,5,offset);p.jump(3,'hex')
    for reg in (20,21,22,23):p.r(37,reg,0,5);p.jump(3,'hex')
    for offset in (16,24):p.i(55,18,5,offset);p.jump(3,'hex')
    p.i(39,28,5,0);p.jump(3,'hex');p.i(9,0,8,10);p.i(40,30,8,0)
    p.i(9,16,16,8);p.i(9,17,17,-1);p.branch(5,17,0,'sample')
    asm.finish(p,len(rows),9);p.mark('helper')
    start=len(p.data)
    # Run entirely from uncached SP DMEM while MI EBUS mode is enabled.
    p.i(9,0,8,0x400);p.i(43,28,8,0)
    for offset,reg in ((0,20),(8,21)):p.i(55,18,reg,offset)
    for op,offset in ((40,16),(41,18),(43,20),(63,24)):p.i(op,18,19,offset)
    for offset,reg in ((16,22),(24,23)):p.i(55,18,reg,offset)
    p.i(9,0,8,0x200);p.i(43,28,8,0);p.r(8,rs=31);p.word(0)
    while (len(p.data)-start)//4<18:p.word(0)
    assert (len(p.data)-start)//4==18
    p.mark('table')
    for raw in rows:
        value=raw|(raw//2<<16)|(raw//4<<32)|(raw//8<<48)
        p.data.extend(value.to_bytes(8,'big'))
    p.resolve();return bytes(p.data)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=HERE/(SUITE+'.z64'));args=parser.parse_args();args.output.write_bytes(build())
