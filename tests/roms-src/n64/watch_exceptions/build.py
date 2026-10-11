#!/usr/bin/env python3
"""Original CP0 Watch cartridge: actual handler results, no guest verdict."""
import argparse,importlib.util
from pathlib import Path
_spec=importlib.util.spec_from_file_location('original_assembler',Path(__file__).resolve().parents[1]/'integer_traps/build.py')
_assembler=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(_assembler)
Program=_assembler.Program
def build(short=False,wrong_exception=False):
    p=Program();p.data[:4]=bytes.fromhex('80371240');p.data[8:12]=bytes.fromhex('80000400');p.data[0x20:0x34]=b'VELOCE WATCH CORPUS ';p.data[0x34:0x3c]=b'V64HB001';assert len(p.data)==0x1000
    p.constant(30,0xbff00000);p.constant(2,0xa0002000);p.i(9,0,28,0);p.word(0x40809000);p.word(0x40809800)
    p.address(8,'handler');p.constant(9,0xa0000180);p.i(9,0,10,6)
    p.mark('copy');p.i(35,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,'copy')
    p.i(9,0,8,128);p.word(0x40886000);p.i(9,0,18,0);p.i(9,0,16,0)
    p.mark('sweep');p.i(13,16,15,3);p.i(9,0,14,128)
    for op in (32,43):p.constant(17,(op<<26)|(4<<21)|(19<<16));p.call('pair')
    p.i(9,16,16,8);p.constant(8,2048 if short else 0x800000);p.branch(5,16,8,'sweep')
    p.i(9,0,18,1)
    for op in (36,40):
        for flags in range(4):
            for match in (True,False):
                for exl in (False,True):
                    label=f'c{op}_{flags}_{int(match)}_{int(exl)}';p.constant(15,(0x600000 if match else 0x600008)|flags);p.i(9,0,14,128|(2 if exl else 0));p.constant(16,0x600000);p.i(9,0,13,8)
                    p.mark(label);p.constant(17,(op<<26)|(4<<21)|(19<<16));p.call('pair');p.i(9,16,16,1);p.i(9,13,13,-1);p.branch(5,13,0,label)
    p.i(43,30,0,4);p.mark('halt');p.branch(4,0,0,'halt')
    p.mark('pair');p.r(0x25,31,0,27);p.i(9,0,20,0);p.call('execute');p.i(9,0,20,1);p.call('execute');p.r(8,27);p.word(0)
    p.mark('execute');p.constant(4,0xa0000000);p.r(0x25,4,16,4)
    p.address(8,'values');p.i(55,8,19,0);p.i(55,8,9,8)
    p.branch(4,18,0,'initialized');p.constant(8,0xa0600000);p.i(63,8,9,0);p.mark('initialized')
    p.constant(10,0xa0030000);p.address(26,'resume')
    for reg in (21,22,23,24):p.r(0x25,0,0,reg)
    p.r(0x25,17,0,9)
    if wrong_exception:p.constant(9,(49<<26)|(4<<21)|(19<<16)) # Execute disabled COP1 before Watch.
    p.branch(4,20,0,'direct');p.constant(8,0x10000003);p.i(43,10,8,0);p.i(43,10,9,4);p.constant(8,(26<<21)|8);p.i(43,10,8,8);p.i(43,10,0,12);p.i(43,10,8,16);p.i(43,10,0,20);p.branch(4,0,0,'enter')
    p.mark('direct');p.i(43,10,9,0);p.constant(8,(26<<21)|8);p.i(43,10,8,4);p.i(43,10,0,8)
    p.mark('enter');p.word(0x408e6000);p.word(0x408f9000);p.branch(5,28,0,'armed');p.i(43,30,0,12);p.mark('armed');p.word(15);p.r(8,10);p.word(0) # Status/WatchLo before actual data op.
    p.mark('resume');p.word(0x40196000);p.word(0x401a9000);p.word(0x40809000);p.i(9,0,8,128);p.word(0x40886000);p.i(9,0,7,0)
    p.branch(4,18,0,'read_done');p.i(36,4,7,0);p.mark('read_done');p.i(9,28,28,1)
    for offset,reg in enumerate((17,16,15,14,20,24,22,21,23,25,19,26,7,28)):p.i(63,2,reg,offset*8)
    p.i(43,30,0,12);p.r(8,31);p.word(0)
    p.mark('handler');p.word(0x40157000);p.word(0x40166800);p.word(0x40176000);p.i(9,24,24,1);p.word(0x409a7000);p.word(0x42000018)
    while len(p.data)%8:p.data.append(0)
    p.mark('values')
    for value in (0x1122334455667788,0x5a5a5a5a5a5a5a5a):p.data.extend(value.to_bytes(8,'big'))
    p.resolve();return bytes(p.data)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('watch_exceptions.z64'));a=p.parse_args();a.output.write_bytes(build())
