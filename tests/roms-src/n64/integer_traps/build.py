#!/usr/bin/env python3
"""Original VR4300 trap cartridge: uncached execution, raw GPR/CP0 RAM records."""
import argparse
from pathlib import Path
VALUES=(0,1,2,0x7fff,0x8000,0x7fffffff,0x80000000,0xffffffff,0x100000000,0x7fffffffffffffff,0x8000000000000000,0xffffffffffffffff,0xfffffffffffffffe,0x5555555555555555,0xaaaaaaaaaaaaaaaa,0xfedcba9876543210)
FUNCTIONS=(0x30,0x31,0x32,0x33,0x34,0x36)
TYPES=(8,9,10,11,12,14)
class Program:
    def __init__(self):self.data=bytearray(0x1000);self.labels={};self.fixups=[]
    @property
    def pc(self):return 0x80000400+len(self.data)-0x1000
    def word(self,x):self.data.extend(x.to_bytes(4,'big'))
    def i(self,op,rs,rt,imm):self.word(op<<26|rs<<21|rt<<16|(imm&65535))
    def r(self,fn,rs=0,rt=0,rd=0,sa=0):self.word(rs<<21|rt<<16|rd<<11|sa<<6|fn)
    def mark(self,name):self.labels[name]=self.pc
    def address(self,rt,name):self.fixups.append((len(self.data),name,'address'));self.i(15,0,rt,0);self.i(13,rt,rt,0)
    def branch(self,op,rs,rt,name):self.fixups.append((len(self.data),name,'branch'));self.i(op,rs,rt,0);self.word(0)
    def call(self,name):self.fixups.append((len(self.data),name,'jump'));self.word(3<<26);self.word(0)
    def constant(self,rt,x):self.i(15,0,rt,x>>16);self.i(13,rt,rt,x&65535)
    def resolve(self):
        for at,name,kind in self.fixups:
            word=int.from_bytes(self.data[at:at+4],'big');target=self.labels[name]
            if kind=='address':
                word|=target>>16;low=int.from_bytes(self.data[at+4:at+8],'big')|(target&65535);self.data[at+4:at+8]=low.to_bytes(4,'big')
            elif kind=='branch':
                delta=(target-(0x80000400+at-0x1000)-4)//4
                if not -32768<=delta<32768:raise ValueError('branch range')
                word|=delta&65535
            else:word|=target>>2&0x3ffffff
            self.data[at:at+4]=word.to_bytes(4,'big')

def build(short=False,wrong_condition=False):
    p=Program();p.data[:4]=bytes.fromhex('80371240');p.data[8:12]=bytes.fromhex('80000400');p.data[0x20:0x34]=b'VELOCE INTEGER TRAP ';p.data[0x34:0x3c]=b'V64HB001'
    p.constant(30,0xbff00000);p.constant(2,0xa0002000);p.i(9,0,28,0)
    p.address(8,'handler');p.constant(9,0xa0000180);p.i(9,0,10,6)
    p.mark('copy_handler');p.i(35,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,'copy_handler')
    p.i(9,0,8,128);p.word(0x40886000) # MTC0 t0,Status: explicit kernel 64-bit profile.
    p.address(8,'values');p.i(55,8,9,14*8);p.r(0x11,9);p.i(55,8,9,15*8);p.r(0x13,9)
    for kind,fn in enumerate(FUNCTIONS):
        label=f'r{kind}';p.i(9,0,16,0)
        p.mark(label+'_code');p.address(12,'values');p.i(9,0,14,16)
        p.mark(label+'_a');p.i(55,12,18,0);p.address(13,'values');p.i(9,0,15,16)
        p.mark(label+'_b');p.i(55,13,19,0);p.i(9,0,20,0)
        p.mark(label+'_placement');p.r(0,rt=16,rd=8,sa=6);p.constant(17,(18<<21)|(19<<16)|fn);p.r(0x25,17,8,17);p.call('execute')
        p.i(9,20,20,1);p.i(9,0,8,2);p.branch(5,20,8,label+'_placement')
        p.i(9,13,13,8);p.i(9,15,15,-1);p.branch(5,15,0,label+'_b')
        p.i(9,12,12,8);p.i(9,14,14,-1);p.branch(5,14,0,label+'_a')
        p.i(9,16,16,1);p.i(9,0,8,4 if short else 1024);p.branch(5,16,8,label+'_code')
    for kind,rt in enumerate(TYPES):
        label=f'i{kind}';p.i(9,0,16,0)
        p.mark(label+'_imm');p.address(12,'values');p.i(9,0,14,16)
        p.mark(label+'_a');p.i(55,12,18,0);p.address(8,'values');p.i(55,8,19,15*8);p.i(9,0,20,0)
        p.mark(label+'_placement');p.constant(17,(1<<26)|(18<<21)|(rt<<16));p.r(0x25,17,16,17);p.call('execute')
        p.i(9,20,20,1);p.i(9,0,8,2);p.branch(5,20,8,label+'_placement')
        p.i(9,12,12,8);p.i(9,14,14,-1);p.branch(5,14,0,label+'_a')
        p.i(9,16,16,4369 if short else 1);p.constant(8,69904 if short else 65536);p.branch(5,16,8,label+'_imm')
    p.i(43,30,0,4);p.mark('halt');p.branch(4,0,0,'halt')
    p.mark('execute');p.constant(10,0xa0030000);p.address(26,'resume')
    for reg in (21,22,23,24):p.r(0x25,0,0,reg)
    p.r(0x25,17,0,9)
    if wrong_condition:
        # Execute the opposite signed/unsigned relation or equality. The input
        # tag remains the requested encoding; only actual execution is mutated.
        p.r(2,rt=9,rd=8,sa=26);p.branch(4,8,0,'mutant_special')
        p.i(15,0,8,2);p.r(0x26,9,8,9);p.branch(4,0,0,'mutant_done')
        p.mark('mutant_special');p.i(14,9,9,2);p.mark('mutant_done')
    p.branch(4,20,0,'direct')
    p.constant(8,0x10000003);p.i(43,10,8,0);p.i(43,10,9,4);p.constant(8,(26<<21)|8);p.i(43,10,8,8);p.i(43,10,0,12);p.i(43,10,8,16);p.i(43,10,0,20)
    p.branch(4,0,0,'enter')
    p.mark('direct');p.i(43,10,9,0);p.constant(8,(26<<21)|8);p.i(43,10,8,4);p.i(43,10,0,8)
    p.mark('enter');p.word(0x0000000f);p.r(8,10);p.word(0) # SYNC before uncached instruction fetch.
    p.mark('resume');p.word(0x40196000);p.r(0x10,rd=8);p.r(0x12,rd=9);p.i(9,28,28,1)
    for offset,reg in enumerate((17,18,19,20,24,22,21,23,25,8,9,28)):p.i(63,2,reg,offset*8)
    p.i(43,30,0,12);p.r(8,31);p.word(0)
    p.mark('handler');p.word(0x40157000);p.word(0x40166800);p.word(0x40176000);p.i(9,24,24,1);p.word(0x409a7000);p.word(0x42000018)
    while len(p.data)%8:p.data.append(0)
    p.mark('values')
    for value in VALUES:p.data.extend(value.to_bytes(8,'big'))
    p.resolve();return bytes(p.data)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('integer_traps.z64'));a=p.parse_args();a.output.write_bytes(build())
