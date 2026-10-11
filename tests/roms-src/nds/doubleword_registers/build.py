#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original DS ARM9 doubleword register/RAM cartridge."""
import argparse,hashlib
from dataclasses import dataclass
from pathlib import Path
TARGET=0x02001808
PATTERNS=(0,1,0xffffffff,0x7fffffff,0x80000000,0x55555555,0xaaaaaaaa,0x89abcdef)
REG_OFFSETS=(0x100,0x7fffffff,0x80000000,0xffffffff,0xffffff00,0x55555555,0xaaaaaaaa)
@dataclass(frozen=True)
class Group:
    op:str
    form:str='imm'
    index:str='offset'
    up:bool=True
    offset:int=0
    alias:str='distinct'
    domain:str='pattern'
    cond:int=14
def groups(full=True):
    out=[]
    for op in ('ldrd','strd'):
        for form in ('imm','reg'):
            for index in ('offset','pre','post'):
                for up in (False,True):
                    out.append(Group(op,form,index,up,8,domain='byte'))
                    for offset in (range(256) if full else (0,1,127,255)):out.append(Group(op,form,index,up,offset))
                    if form=='reg':
                        for offset in REG_OFFSETS:out.append(Group(op,form,index,up,offset))
                    for cond in range(14):out.append(Group(op,form,index,up,255,cond=cond))
                    if index=='offset':
                        for alias in ('base_low','base_high'):out.append(Group(op,form,index,up,255,alias))
                    if op=='strd' and form=='reg':
                        for alias in ('index_low','index_high'):out.append(Group(op,form,index,up,255,alias))
    return tuple(out)
def values(g,full=True):return tuple(n*0x01010101 for n in (range(256) if full else (0,127,128,255))) if g.domain=='byte' else PATTERNS
def flags(g):return (0,1) if g.domain=='byte' else tuple(range(32))
def cases(g,full=True):return len(values(g,full))**2*len(flags(g))
def name(g):return f'{g.op}_{g.form}_{g.index}_{int(g.up)}_{g.offset:08x}_{g.alias}_{g.domain}_{g.cond}'
def fields(g):return (2 if g.alias=='base_low' else 0,1 if g.alias=='base_high' else 2,0 if g.alias=='index_low' else 1 if g.alias=='index_high' else 0 if g.alias=='base_low' and g.op=='ldrd' and g.form=='reg' else 3)
def base(g):return TARGET if g.index=='post' else (TARGET+(-g.offset if g.up else g.offset))&0xffffffff
def instruction(g,mutant=False):
    rd,rn,rm=fields(g);field=((g.offset&240)<<4)|(g.offset&15) if g.form=='imm' else rm
    # Executed mutant substitutes a store for a load with the same addressing.
    store=g.op=='strd' or (mutant and g.op=='ldrd')
    return (g.cond<<28)|0x000000d0|(int(store)<<5)|(int(g.index!='post')<<24)|(int(g.up)<<23)|(int(g.form=='imm')<<22)|(int(g.index=='pre')<<21)|(rn<<16)|(rd<<12)|field
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
    p=Program(0x02004000);domain=groups(full);p.constant(12,0x04ff0000);p.constant(13,0x02001000);p.puts('header')
    for g in domain:
        label=name(g);vals=values(g,full)
        p.puts(label+'_header');p.address(8,label+'_values');p.constant(9,len(vals));p.mark(label+'_left')
        p.address(10,label+'_values');p.constant(11,len(vals));p.mark(label+'_right')
        p.constant(5,0);p.mark(label+'_flags');p.constant(4,TARGET-4)
        p.constant(6,0xfacefeed);p.memory(6,4,load=False);p.constant(6,0xcafebabe);p.memory(6,4,12,load=False)
        p.memory(0,8);p.memory(1,10);p.memory(0,4,4,load=False);p.memory(1,4,8,load=False)
        p.emit(0xe1e00000);p.constant(6,0xa5a5a5a5);p.emit(0xe0211006)
        p.constant(2,base(g));p.constant(3,g.offset)
        if g.alias=='base_high':p.constant(1,base(g))
        if g.alias=='index_low' or (g.alias=='base_low' and g.op=='ldrd' and g.form=='reg'):p.constant(0,g.offset)
        if g.alias=='index_high':p.constant(1,g.offset)
        p.emit(0xe1a06005|(27<<7));p.emit(0xe128f006);p.emit(instruction(g,mutant))
        p.emit(0xe10f6000);p.memory(6,13,4,load=False);p.puts('row')
        for reg in range(4):p.emit(0xe1a06000|reg);p.branch('word',link=True)
        p.memory(6,13,4);p.branch('word',link=True)
        for offset in (0,4,8,12):p.memory(6,4,offset);p.branch('word',link=True)
        p.constant(6,10);p.memory(6,12,load=False,byte=True)
        p.emit(0xe2855001);p.emit(0xe3550000|len(flags(g)));p.branch(label+'_flags',cond=1)
        p.emit(0xe28aa004);p.emit(0xe25bb001);p.branch(label+'_right',cond=1)
        p.emit(0xe2888004);p.emit(0xe2599001);p.branch(label+'_left',cond=1)
    p.puts('complete');p.constant(6,0);p.memory(6,12,4,load=False);p.mark('idle');p.branch('idle')
    p.mark('puts');p.memory(7,6,load=True,byte=True);p.emit(0xe3570000);p.emit(0x012fff1e)
    p.memory(7,12,load=False,byte=True);p.emit(0xe2866001);p.branch('puts')
    p.mark('word');p.memory(6,12,8,load=False);p.emit(0xe12fff1e)
    tables={}
    for g in domain:
        vals=values(g,full);label=name(g)+'_values'
        if vals in tables:p.labels[label]=tables[vals]
        else:
            p.mark(label);tables[vals]=p.pc
            for value in vals:p.emit(value)
    p.string('header',f'VELOCE 1 ds doubleword_registers cpu=9\nINFO full={int(full)} groups={len(domain)} words=9\n')
    for g in domain:p.string(name(g)+'_header',f'BEGIN {name(g)} cases={cases(g,full)} words=9\n')
    p.string('row','REG r=');p.string('complete',f'LOG complete cases={sum(cases(g,full) for g in domain)}\nEND observations_complete\n')
    while len(p.data)%4:p.data.append(0)
    p.resolve()
    if p.pc>0x023f0000:raise ValueError('driver exceeds mapped RAM/idle placement')
    return bytes(p.data)
def build(full=True,mutant=False):
    nine=program(full,mutant);seven=bytes.fromhex('feffffea');offset7=(512+len(nine)+511)&~511
    rom=bytearray(offset7+len(seven));rom[:12]=b'VELOCE DBLW ';rom[12:16]=b'VDBL';rom[0x180:0x188]=b'VDSHB001'
    for offset,fields in ((0x20,(512,0x02004000,0x02004000,len(nine))),(0x30,(offset7,0x023f0000,0x023f0000,len(seven)))):
        for n,field in enumerate(fields):rom[offset+n*4:offset+n*4+4]=field.to_bytes(4,'little')
    rom[0x80:0x84]=len(rom).to_bytes(4,'little');rom[0x84:0x88]=(512).to_bytes(4,'little');rom[512:512+len(nine)]=nine;rom[offset7:]=seven
    return bytes(rom)
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('doubleword_registers.nds'));args=ap.parse_args();rom=build();args.output.write_bytes(rom);print(hashlib.sha256(rom).hexdigest(),args.output)
