#!/usr/bin/env python3
"""Original disabled-COP2 exception cartridge; raw handler observations only."""
import argparse,importlib.util
from pathlib import Path
_spec=importlib.util.spec_from_file_location('original_assembler',Path(__file__).resolve().parents[1]/'integer_traps/build.py')
_assembler=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(_assembler)
Program=_assembler.Program
VALUES=(0,1,2,0x7fff,0x8000,0x7fffffff,0x80000000,0xffffffff,0x100000000,0x7fffffffffffffff,0x8000000000000000,0xffffffffffffffff,0xfffffffffffffffe,0x5555555555555555,0xaaaaaaaaaaaaaaaa,0xfedcba9876543210)
def build(short=False,wrong_coprocessor=False):
    p=Program();p.data[:4]=bytes.fromhex('80371240');p.data[8:12]=bytes.fromhex('80000400');p.data[0x20:0x34]=b'VELOCE COP2 UNUSABLE';p.data[0x34:0x3c]=b'V64HB001'
    p.constant(30,0xbff00000);p.constant(2,0xa0002000);p.i(9,0,28,0)
    p.address(8,'handler');p.constant(9,0xa0000180);p.i(9,0,10,6)
    p.mark('copy');p.i(35,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,'copy')
    p.i(9,0,8,128);p.word(0x40886000);p.address(8,'values');p.i(55,8,9,14*8);p.r(0x11,9);p.i(55,8,9,15*8);p.r(0x13,9)
    for op in (50,54,58,62):
        label=f'm{op}';p.i(9,0,16,0);p.mark(label+'_imm');p.address(12,'values');p.i(9,0,14,16)
        p.mark(label+'_value');p.i(55,12,18,0);p.address(8,'values');p.i(55,8,19,15*8);p.constant(17,(op<<26)|(18<<21)|(19<<16));p.r(0x25,17,16,17);p.call('pair')
        p.i(9,12,12,8);p.i(9,14,14,-1);p.branch(5,14,0,label+'_value');p.i(9,16,16,4369 if short else 1);p.constant(8,69904 if short else 65536);p.branch(5,16,8,label+'_imm')
    # Defined move/control encodings: every target and coprocessor register.
    for rs in (0,1,2,4,5,6):
        label=f'r{rs}';p.i(9,0,16,0);p.mark(label);p.constant(17,(18<<26)|(rs<<21));p.r(0,rt=16,rd=8,sa=11);p.r(0x25,17,8,17);p.call('pair');p.i(9,16,16,1);p.i(9,0,8,1024);p.branch(5,16,8,label)
    for rt in range(4):
        label=f'b{rt}';p.i(9,0,16,0);p.mark(label);p.constant(17,(18<<26)|(8<<21)|(rt<<16));p.r(0x25,17,16,17);p.call('pair');p.i(9,16,16,4369 if short else 1);p.constant(8,69904 if short else 65536);p.branch(5,16,8,label)
    p.i(9,0,16,0);p.mark('cofun');p.constant(17,(18<<26)|(1<<25));p.r(0x25,17,16,17);p.call('pair');p.i(9,16,16,4369 if short else 1);p.constant(8,69904 if short else 65536);p.branch(5,16,8,'cofun')
    p.i(43,30,0,4);p.mark('halt');p.branch(4,0,0,'halt')
    # Pair preserves the outer return address while executing both placements.
    p.mark('pair');p.r(0x25,31,0,27);p.i(9,0,20,0);p.call('execute');p.i(9,0,20,1);p.call('execute');p.r(8,27);p.word(0)
    p.mark('execute');p.constant(10,0xa0030000);p.address(26,'resume')
    for reg in (21,22,23,24):p.r(0x25,0,0,reg)
    p.r(0x25,17,0,9)
    if wrong_coprocessor:
        # Request tags remain COP2; execute the corresponding disabled COP1 encoding.
        p.constant(8,3<<26);p.r(0x26,9,8,9)
    p.branch(4,20,0,'direct');p.constant(8,0x10000003);p.i(43,10,8,0);p.i(43,10,9,4);p.constant(8,(26<<21)|8);p.i(43,10,8,8);p.i(43,10,0,12);p.i(43,10,8,16);p.i(43,10,0,20);p.branch(4,0,0,'enter')
    p.mark('direct');p.i(43,10,9,0);p.constant(8,(26<<21)|8);p.i(43,10,8,4);p.i(43,10,0,8)
    p.mark('enter');p.word(15);p.r(8,10);p.word(0)
    p.mark('resume');p.word(0x40196000);p.r(0x10,rd=8);p.r(0x12,rd=9);p.i(9,28,28,1)
    for offset,reg in enumerate((17,18,19,20,24,22,21,23,25,8,9,28)):p.i(63,2,reg,offset*8)
    p.i(43,30,0,12);p.r(8,31);p.word(0)
    p.mark('handler');p.word(0x40157000);p.word(0x40166800);p.word(0x40176000);p.i(9,24,24,1);p.word(0x409a7000);p.word(0x42000018)
    while len(p.data)%8:p.data.append(0)
    p.mark('values')
    for value in VALUES:p.data.extend(value.to_bytes(8,'big'))
    p.resolve();return bytes(p.data)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('cop2_unusable.z64'));a=p.parse_args();a.output.write_bytes(build())
