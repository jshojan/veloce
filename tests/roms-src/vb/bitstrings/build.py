#!/usr/bin/env python3
"""Original V810 arithmetic bit-string register cartridge."""
import argparse
import hashlib
from pathlib import Path
OPS=(('or',8),('and',9),('xor',10),('mov',11),('orn',12),('andn',13),('xorn',14),('not',15))
LENGTHS=(0,1,7,8,15,16,31,32,33,63,64,65)
def groups(full=True):
    byte=tuple(n*0x01010101 for n in range(256 if full else 16))
    offsets=range(32) if full else (0,1,31)
    parameters=tuple((s,d,n) for s in offsets for d in offsets for n in LENGTHS)
    return tuple((name+'_'+kind,name,op,values,params)
        for kind,values,params in (('byte',byte,((0,0,8),)),('offset',(0,0xffffffff),parameters))
        for name,op in OPS)
def cases(group): return len(group[3])**2*len(group[4])*16

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
def build(full=True,wrong_mov=False):
    p=Program(); domain=groups(full)
    p.constant(24,0x04000000); p.constant(13,0x05002000); p.constant(14,0x05002100); p.constant(15,0x050020fc)
    p.constant(5,0x8000); p.constant(23,16); p.constant(1,0xdeadbeef); p.wide(0x37,15,1,0); p.puts('header')
    for name,operation,op,values,params in domain:
        p.puts(name+'_header'); p.address(16,name+'_values'); p.constant(17,len(values))
        p.mark(name+'_source_loop'); p.wide(0x33,16,11,0)
        p.address(18,name+'_values'); p.constant(19,len(values)); p.mark(name+'_destination_loop'); p.wide(0x33,18,12,0)
        p.address(20,name+'_params'); p.constant(21,len(params)); p.mark(name+'_params_loop')
        p.reg(0x10,0,22); p.mark(name+'_flags_loop')
        p.wide(0x37,13,11,0); p.reg(0x0f,11,1); p.wide(0x37,13,1,4)
        p.constant(1,0x55555555); p.reg(0x0e,11,1); p.wide(0x37,13,1,8)
        p.wide(0x37,14,12,0); p.reg(0x0f,12,1); p.wide(0x37,14,1,4)
        for offset,pattern in ((8,0xa5a5a5a5),(12,0x89abcdef)):
            p.constant(1,pattern); p.reg(0x0e,12,1); p.wide(0x37,14,1,offset)
        p.wide(0x33,20,27,0); p.wide(0x33,20,26,4); p.wide(0x33,20,28,8)
        p.reg(0,14,29); p.reg(0,13,30); p.reg(0,22,1); p.reg(0x0c,5,1); p.reg(0x1c,5,1)
        p.reg(0x1f,10 if wrong_mov and operation=='mov' else op); p.reg(0x1d,5,4)
        p.puts('row')
        for base,count in ((14,4),(13,3)):
            for index in range(count): p.wide(0x33,base,10,index*4); p.jump(0x2b,'hex')
        for rt in (26,27,28,29,30,4): p.reg(0,rt,10); p.jump(0x2b,'hex')
        p.wide(0x33,15,10,0); p.jump(0x2b,'hex'); p.reg(0x10,10,6); p.wide(0x34,24,6,0)
        p.reg(0x11,1,22); p.reg(3,23,22); p.branch(2,name+'_flags_done'); p.jump(0x2a,name+'_flags_loop'); p.mark(name+'_flags_done')
        p.wide(0x28,20,20,12); p.reg(0x11,31,21)
        p.branch(2,name+'_params_done'); p.jump(0x2a,name+'_params_loop'); p.mark(name+'_params_done')
        p.wide(0x28,18,18,4); p.reg(0x11,31,19)
        p.branch(2,name+'_destination_done'); p.jump(0x2a,name+'_destination_loop'); p.mark(name+'_destination_done')
        p.wide(0x28,16,16,4); p.reg(0x11,31,17)
        p.branch(2,name+'_source_done'); p.jump(0x2a,name+'_source_loop'); p.mark(name+'_source_done')
    p.puts('complete'); p.wide(0x37,24,0,4); p.reg(0x1a)
    p.mark('puts'); p.wide(0x30,10,6,0); p.reg(0x13,0,6); p.branch(2,'puts_return')
    p.wide(0x34,24,6,0); p.reg(0x11,1,10); p.jump(0x2a,'puts'); p.mark('puts_return'); p.reg(6,31)
    p.mark('hex'); p.wide(0x28,0,7,28); p.mark('hex_loop')
    p.reg(0,10,6); p.reg(5,7,6); p.wide(0x2d,6,6,15); p.reg(0x13,10,6)
    p.branch(1,'digit'); p.wide(0x29,6,6,39); p.mark('digit'); p.wide(0x28,6,6,48); p.wide(0x34,24,6,0)
    p.reg(0x11,28,7); p.branch(14,'hex_loop'); p.reg(6,31)
    while p.pc&3: p.data.append(0)
    for name,_,_,values,params in domain:
        p.mark(name+'_values')
        for value in values: p.data.extend(value.to_bytes(4,'little'))
        p.mark(name+'_params')
        for triple in params:
            for value in triple: p.data.extend(value.to_bytes(4,'little'))
    total=sum(cases(g) for g in domain)
    p.string('header',f'VELOCE 1 vb bitstrings\nINFO full={int(full)} groups={len(domain)}\n')
    for g in domain: p.string(g[0]+'_header',f'BEGIN {g[0]} cases={cases(g)} words=14\n')
    p.string('row','REG r='); p.string('complete',f'LOG complete cases={total}\nEND observations_complete\n')
    p.resolve(); size=1024
    while size<len(p.data)+0x200: size*=2
    p.data.extend(bytes(size-len(p.data))); p.data[size-0x200:size-0x1f8]=b'VVBHB001'
    p.data[-16:-10]=((0x2f<<10)|(10<<5)).to_bytes(2,'little')+bytes.fromhex('0007')+((6<<10)|10).to_bytes(2,'little')
    return bytes(p.data)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('bitstrings.vb'))
    args=parser.parse_args(); rom=build(); args.output.write_bytes(rom)
    print(f'{hashlib.sha256(rom).hexdigest()}  {args.output}')
