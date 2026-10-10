#!/usr/bin/env python3
"""Original V810 immediate-instruction register cartridge."""
import argparse
import hashlib
from pathlib import Path
PATTERNS=(0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,
          0xfffffffe,0xffffffff,0x55555555,0x89abcdef)
WIDE=(('movea',0x28),('addi',0x29),('ori',0x2c),('andi',0x2d),('xori',0x2e),('movhi',0x2f))
SHORT=(('mov',0x10),('add',0x11),('cmp',0x13),('shl',0x14),('shr',0x15),('sar',0x17))
BOUNDARIES=(0,1,2,15,16,31,32,127,128,255,256,0x7ffe,0x7fff,0x8000,0xfffe,0xffff)
def groups(full=True):
    return tuple((name,name,op,PATTERNS,tuple(range(65536)) if full else BOUNDARIES)
                 for name,op in WIDE)+tuple((name,name,op,PATTERNS,tuple(range(32))) for name,op in SHORT)
def cases(group): return len(group[3])*len(group[4])*16

class Program:
    def __init__(self): self.data=bytearray(); self.labels={}; self.fixups=[]
    @property
    def pc(self): return 0x07000000+len(self.data)
    def mark(self,name):
        if name in self.labels: raise ValueError("duplicate label")
        self.labels[name]=self.pc
    def half(self,value): self.data.extend((value&65535).to_bytes(2,'little'))
    def reg(self,op,r1=0,r2=0): self.half(op<<10|r2<<5|r1)
    def wide(self,op,r1,r2,imm): self.reg(op,r1,r2); self.half(imm)
    def constant(self,rt,value):
        self.wide(0x2f,0,rt,value>>16); self.wide(0x2c,rt,rt,value&65535)
    def address(self,rt,label):
        self.fixups.append((len(self.data),label,'address')); self.constant(rt,0)
    def branch(self,cond,label):
        self.fixups.append((len(self.data),label,'branch')); self.half(0x8000|cond<<9)
    def jump(self,op,label):
        self.fixups.append((len(self.data),label,'jump')); self.half(op<<10); self.half(0)
    def puts(self,label): self.address(10,label); self.jump(0x2b,'puts')
    def string(self,label,text): self.mark(label); self.data.extend(text.encode('ascii')+b'\0')
    def resolve(self):
        for offset,label,kind in self.fixups:
            target=self.labels[label]; pc=0x07000000+offset
            if kind=='address':
                self.data[offset+2:offset+4]=(target>>16).to_bytes(2,'little')
                self.data[offset+6:offset+8]=(target&65535).to_bytes(2,'little')
            elif kind=='branch':
                delta=target-pc
                if delta&1 or not -256<=delta<256: raise ValueError("branch range")
                word=int.from_bytes(self.data[offset:offset+2],'little')|delta&0x1fe
                self.data[offset:offset+2]=word.to_bytes(2,'little')
            else:
                delta=target-pc
                if delta&1 or not -(1<<25)<=delta<(1<<25): raise ValueError("jump range")
                word=int.from_bytes(self.data[offset:offset+2],'little')|((delta>>16)&0x3ff)
                self.data[offset:offset+2]=word.to_bytes(2,'little')
                self.data[offset+2:offset+4]=(delta&65535).to_bytes(2,'little')

def build(full=True,wrong_addi=False,wrong_mov=False):
    p=Program(); domain=groups(full)
    p.constant(28,0x04000000); p.constant(27,16); p.constant(26,0x05001000); p.constant(5,0x8000)
    p.constant(1,(6<<10)|31); p.wide(0x37,26,1,4); p.puts('header')
    for name,operation,opcode,values,immediates in domain:
        p.puts(name+'_header'); p.address(16,'patterns'); p.constant(17,len(values))
        p.mark(name+'_left'); p.wide(0x33,16,23,0)
        p.address(18,name+'_instructions'); p.constant(19,len(immediates)); p.mark(name+'_immediate')
        p.wide(0x33,18,24,0); p.wide(0x37,26,24,0)
        p.reg(0x10,0,22); p.mark(name+'_flags')
        p.address(31,name+'_return'); p.reg(0,23,20); p.reg(0,23,25); p.reg(0,22,1); p.reg(0x0c,5,1); p.reg(0x1c,5,1)
        p.reg(6,26); p.mark(name+'_return'); p.reg(0x1d,5,21)
        p.puts('row')
        for rt in (20,21,25): p.reg(0,rt,10); p.jump(0x2b,'hex')
        p.reg(0x10,10,6); p.wide(0x34,28,6,0)
        p.reg(0x11,1,22); p.reg(3,27,22); p.branch(2,name+'_flags_done'); p.jump(0x2a,name+'_flags'); p.mark(name+'_flags_done')
        p.wide(0x28,18,18,4); p.reg(0x11,31,19); p.branch(2,name+'_immediate_done'); p.jump(0x2a,name+'_immediate'); p.mark(name+'_immediate_done')
        p.wide(0x28,16,16,4); p.reg(0x11,31,17); p.branch(2,name+'_left_done'); p.jump(0x2a,name+'_left'); p.mark(name+'_left_done')
    p.puts('complete'); p.wide(0x37,28,0,4); p.reg(0x1a)
    p.mark('puts'); p.wide(0x30,10,6,0); p.reg(0x13,0,6); p.branch(2,'puts_return')
    p.wide(0x34,28,6,0); p.reg(0x11,1,10); p.jump(0x2a,'puts'); p.mark('puts_return'); p.reg(6,31)
    p.mark('hex'); p.constant(7,28); p.mark('hex_loop')
    p.reg(0,10,6); p.reg(5,7,6); p.wide(0x2d,6,6,15); p.reg(0x13,10,6)
    p.branch(1,'digit'); p.wide(0x29,6,6,39); p.mark('digit'); p.wide(0x28,6,6,48); p.wide(0x34,28,6,0)
    p.reg(0x11,28,7); p.branch(14,'hex_loop'); p.reg(6,31)
    while p.pc&3: p.data.append(0)
    p.mark('patterns')
    for value in PATTERNS: p.data.extend(value.to_bytes(4,'little'))
    for name,operation,opcode,_,immediates in domain:
        p.mark(name+'_instructions')
        if wrong_addi and operation=='addi': opcode=0x2c
        if wrong_mov and operation=='mov': opcode=0x14
        for imm in immediates:
            word=(opcode<<10)|(20<<5)|(25 if opcode>=0x28 else imm)
            word|=(imm<<16) if opcode>=0x28 else (((6<<10)|31)<<16)
            p.data.extend(word.to_bytes(4,'little'))
    total=sum(cases(g) for g in domain)
    p.string('header',f'VELOCE 1 vb immediates\nINFO full={int(full)} flags=16 groups={len(domain)}\n')
    for g in domain: p.string(g[0]+'_header',f'BEGIN {g[0]} cases={cases(g)} words=3\n')
    p.string('row','REG r='); p.string('complete',f'LOG complete cases={total}\nEND observations_complete\n')
    p.resolve(); size=1024
    while size<len(p.data)+0x200: size*=2
    p.data.extend(bytes(size-len(p.data))); p.data[size-0x200:size-0x1f8]=b'VVBHB001'
    p.data[-16:-10]=((0x2f<<10)|(10<<5)).to_bytes(2,'little')+bytes.fromhex('0007')+((6<<10)|10).to_bytes(2,'little')
    return bytes(p.data)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('immediates.vb'))
    args=parser.parse_args(); rom=build(); args.output.write_bytes(rom)
    print(f'{hashlib.sha256(rom).hexdigest()}  {args.output}')
