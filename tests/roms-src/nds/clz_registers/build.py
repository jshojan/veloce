#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original ARM9 CLZ register cartridge, standalone emitter without SDK."""
import argparse,hashlib
from pathlib import Path
BOUNDARIES=tuple(sorted({0,0xffffffff,0x55555555,0xaaaaaaaa}|{1<<bit for bit in range(32)}|{(1<<bit)-1 for bit in range(1,33)}))
def groups(full=True):
    values=tuple(range(65536)) if full else (0,1,127,128,255,32767,32768,65535)
    result=[]
    for shift in (0,8,16):
        for alias in (False,True):result.append((f'window_{shift}_{int(alias)}',tuple(n<<shift for n in values),14,alias))
    for cond in range(15):
        for alias in (False,True):result.append((f'boundary_{cond}_{int(alias)}',BOUNDARIES,cond,alias))
    return tuple(result)
def cases(g):return len(g[1])*32
def immediate(value):
    for rotation in range(16):
        count=rotation*2; candidate=((value<<count)|(value>>(32-count if count else 32)))&0xffffffff
        if candidate<256: return (rotation<<8)|candidate
    raise ValueError('unencodable immediate')
class Program:
    def __init__(self,base): self.base=base; self.data=bytearray(); self.labels={}; self.fixups=[]
    @property
    def pc(self): return self.base+len(self.data)
    def emit(self,word): self.data.extend(word.to_bytes(4,'little'))
    def mark(self,name):
        if name in self.labels: raise ValueError('duplicate label')
        self.labels[name]=self.pc
    def branch(self,label,cond=14,link=False):
        self.fixups.append((len(self.data),label,'branch',cond,link)); self.emit(0)
    def constant(self,reg,value):
        self.emit(0xe3a00000|(reg<<12)|immediate(value&255))
        for shift in (8,16,24):
            chunk=value&(255<<shift)
            if chunk: self.emit(0xe3800000|(reg<<16)|(reg<<12)|immediate(chunk))
    def address(self,reg,label):
        self.fixups.append((len(self.data),label,'address',reg,False))
        for _ in range(4): self.emit(0)
    def puts(self,label): self.address(6,label); self.branch('puts',link=True)
    def memory(self,reg,base,offset=0,load=True,byte=False):
        self.emit(0xe5800000|(int(load)<<20)|(int(byte)<<22)|(base<<16)|(reg<<12)|offset)
    def string(self,label,value): self.mark(label); self.data.extend(value.encode('ascii')+b'\0')
    def resolve(self):
        for offset,label,kind,reg,link in self.fixups:
            target=self.labels[label]
            if kind=='branch':
                delta=target-(self.base+offset+8)
                if delta%4 or not -(1<<25)<=delta<(1<<25): raise ValueError('branch range')
                words=[(reg<<28)|0x0a000000|(int(link)<<24)|((delta//4)&0xffffff)]
            else:
                words=[0xe3a00000|(reg<<12)|immediate(target&255)]
                for count in (8,16,24): words.append(0xe3800000|(reg<<16)|(reg<<12)|immediate(target&(255<<count)))
            for n,word in enumerate(words): self.data[offset+n*4:offset+n*4+4]=word.to_bytes(4,'little')

def program(full=True,mutant=False):
    p=Program(0x02004000);domain=groups(full)
    p.constant(12,0x04ff0000);p.constant(13,0x02001000);p.puts('header')
    for name,values,cond,alias in domain:
        p.puts(name+'_header');p.address(8,name+'_values');p.constant(9,len(values));p.mark(name+'_operand')
        p.constant(2,0);p.mark(name+'_flags');p.memory(0,8);p.constant(4,0xdeadbeef)
        p.emit(0xe1a06002|(27<<7));p.emit(0xe128f006)
        rd=0 if alias else 4
        p.emit((cond<<28)|0x016f0f10|(rd<<12))
        p.emit(0xe10f6000);p.memory(6,13,4,load=False);p.emit(0xe1a04000|rd)
        if mutant:p.emit((cond<<28)|0x02844001)
        p.puts('row');p.emit(0xe1a06004);p.branch('word',link=True)
        p.memory(6,13,4);p.branch('word',link=True)
        p.emit(0xe1a06000);p.branch('word',link=True)
        p.constant(6,10);p.memory(6,12,load=False,byte=True)
        p.emit(0xe2822001);p.emit(0xe3520020);p.branch(name+'_flags',cond=1)
        p.emit(0xe2888004);p.emit(0xe2599001);p.branch(name+'_operand',cond=1)
    p.puts('complete');p.constant(6,0);p.memory(6,12,4,load=False);p.mark('idle');p.branch('idle')
    p.mark('puts');p.memory(7,6,load=True,byte=True);p.emit(0xe3570000);p.emit(0x012fff1e)
    p.memory(7,12,load=False,byte=True);p.emit(0xe2866001);p.branch('puts')
    p.mark('word');p.memory(6,12,8,load=False);p.emit(0xe12fff1e)
    for name,values,_,_ in domain:
        p.mark(name+'_values')
        for value in values:p.emit(value)
    p.string('header',f'VELOCE 1 ds clz_registers cpu=9\nINFO full={int(full)} groups={len(domain)} flags=32\n')
    for g in domain:p.string(g[0]+'_header',f'BEGIN {g[0]} cases={cases(g)} words=3\n')
    p.string('row','REG r=');p.string('complete',f'LOG complete cases={sum(cases(g) for g in domain)}\nEND observations_complete\n')
    while len(p.data)%4:p.data.append(0)
    p.resolve();return bytes(p.data)
def build(full=True,mutant=False):
    nine=program(full,mutant);seven=bytes.fromhex('feffffea');offset7=(512+len(nine)+511)&~511
    rom=bytearray(offset7+len(seven));rom[:12]=b'VELOCE CLZ  ';rom[12:16]=b'VCLZ';rom[0x180:0x188]=b'VDSHB001'
    for offset,fields in ((0x20,(512,0x02004000,0x02004000,len(nine))),(0x30,(offset7,0x02200000,0x02200000,len(seven)))):
        for n,field in enumerate(fields):rom[offset+n*4:offset+n*4+4]=field.to_bytes(4,'little')
    rom[0x80:0x84]=len(rom).to_bytes(4,'little');rom[0x84:0x88]=(512).to_bytes(4,'little')
    rom[512:512+len(nine)]=nine;rom[offset7:]=seven;return bytes(rom)
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('clz_registers.nds'))
    a=ap.parse_args();rom=build();a.output.write_bytes(rom);print(hashlib.sha256(rom).hexdigest(),a.output)
