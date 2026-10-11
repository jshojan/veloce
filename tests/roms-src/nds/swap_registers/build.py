#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original dual-CPU SWP/SWPB register and memory cartridge."""
import argparse,hashlib
from pathlib import Path
PATTERNS=(0,1,0xffffffff,0x80000000,0x7fffffff,0x000000ff,0x0000ff00,0x00ff0000,0xff000000,0x01020304,0xdeadbeef,0x55555555,0xaaaaaaaa,0xfffffffe,0x80000001,0x7ffffffe)
def groups(full=True):
    byte=tuple(n*0x01010101 for n in (range(256) if full else (0,127,128,255)))
    boundary=PATTERNS if full else (0,1,0xffffffff,0x80000000,0x01020304,0xdeadbeef)
    result=[]
    for width,lane in ((4,0),(1,0),(1,1),(1,2),(1,3)):
        for alias in (False,True):
            result.append((f'byte_{width}_{lane}_{int(alias)}',width,lane,alias,14,byte))
            for cond in range(15):result.append((f'boundary_{width}_{lane}_{int(alias)}_{cond}',width,lane,alias,cond,boundary))
    return tuple(result)
def cases(g,cpu):return len(g[5])**2*(32 if cpu==9 else 16)
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

def program(cpu,full=True,mutant=False):
    p=Program(0x02004000 if cpu==9 else 0x02200000);domain=groups(full)
    p.constant(12,0x04ff0000);p.constant(13,0x02001000)
    if cpu==7:
        p.mark('wait');p.memory(6,13);p.emit(0xe3560001);p.branch('wait',cond=1)
    p.puts('header')
    for name,width,lane,alias,cond,values in domain:
        p.puts(name+'_header');p.address(8,name+'_values');p.constant(9,len(values));p.mark(name+'_left')
        p.address(10,name+'_values');p.constant(11,len(values));p.mark(name+'_right');p.constant(2,0);p.mark(name+'_flags')
        p.memory(0,8);p.memory(1,10);p.memory(1,13,0x40,load=False)
        p.constant(5,0xfacefeed);p.memory(5,13,0x44,load=False);p.constant(4,0xdeadbeef);p.constant(3,0x02001040+lane)
        p.emit(0xe1a06002|((27 if cpu==9 else 28)<<7));p.emit(0xe128f006)
        rd=0 if alias else 4
        if mutant:
            # Execute an ordinary load in place of the exchange: memory stays old.
            p.emit((cond<<28)|0x05930000|(int(width==1)<<22)|(rd<<12))
        else:p.emit((cond<<28)|0x01030090|(int(width==1)<<22)|(rd<<12))
        p.emit(0xe10f6000);p.memory(6,13,4,load=False);p.emit(0xe1a04000|rd)
        p.puts('row');p.emit(0xe1a06004);p.branch('word',link=True);p.memory(6,13,4);p.branch('word',link=True)
        for reg in (0,3):p.emit(0xe1a06000|reg);p.branch('word',link=True)
        for offset in (0x40,0x44):p.memory(6,13,offset);p.branch('word',link=True)
        p.constant(6,10);p.memory(6,12,load=False,byte=True)
        p.emit(0xe2822001);p.emit(0xe3520000|(32 if cpu==9 else 16));p.branch(name+'_flags',cond=1)
        p.emit(0xe28aa004);p.emit(0xe25bb001);p.branch(name+'_right',cond=1)
        p.emit(0xe2888004);p.emit(0xe2599001);p.branch(name+'_left',cond=1)
    p.puts('complete')
    if cpu==9:p.constant(6,1);p.memory(6,13,load=False)
    p.constant(6,0);p.memory(6,12,4,load=False);p.mark('idle');p.branch('idle')
    p.mark('puts');p.memory(7,6,load=True,byte=True);p.emit(0xe3570000);p.emit(0x012fff1e)
    p.memory(7,12,load=False,byte=True);p.emit(0xe2866001);p.branch('puts')
    p.mark('word');p.memory(6,12,8,load=False);p.emit(0xe12fff1e)
    for name,_,_,_,_,values in domain:
        p.mark(name+'_values')
        for value in values:p.emit(value)
    p.string('header',f'VELOCE 1 ds swap_registers cpu={cpu}\nINFO full={int(full)} groups={len(domain)} flags={32 if cpu==9 else 16}\n')
    for g in domain:p.string(g[0]+'_header',f'BEGIN cpu{cpu}_{g[0]} cases={cases(g,cpu)} words=6\n')
    p.string('row','REG r=');p.string('complete',f'LOG cpu={cpu} complete cases={sum(cases(g,cpu) for g in domain)}\n'+('END observations_complete\n' if cpu==7 else ''))
    while len(p.data)%4:p.data.append(0)
    p.resolve();return bytes(p.data)
def build(full=True,mutant=False):
    nine=program(9,full,mutant);seven=program(7,full,mutant);offset7=(512+len(nine)+511)&~511
    rom=bytearray(offset7+len(seven));rom[:12]=b'VELOCE SWAP ';rom[12:16]=b'VSWP';rom[0x180:0x188]=b'VDSHB001'
    for offset,fields in ((0x20,(512,0x02004000,0x02004000,len(nine))),(0x30,(offset7,0x02200000,0x02200000,len(seven)))):
        for n,field in enumerate(fields):rom[offset+n*4:offset+n*4+4]=field.to_bytes(4,'little')
    rom[0x80:0x84]=len(rom).to_bytes(4,'little');rom[0x84:0x88]=(512).to_bytes(4,'little')
    rom[512:512+len(nine)]=nine;rom[offset7:]=seven;return bytes(rom)
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('swap_registers.nds'))
    a=ap.parse_args();rom=build();a.output.write_bytes(rom);print(hashlib.sha256(rom).hexdigest(),a.output)
