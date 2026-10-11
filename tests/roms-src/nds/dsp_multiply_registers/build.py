#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original ARM9 ARMv5TE DSP multiplication register cartridge."""
import argparse,hashlib
from pathlib import Path
OPS=tuple((kind,x,y) for kind in ('smul','smla','smlal') for x in range(2) for y in range(2))+tuple((kind,0,y) for kind in ('smulw','smlaw') for y in range(2))
PATTERNS=(0,1,0xffffffff,0x7fffffff,0x80000000,0x80007fff,0x7fff8000,0x5555aaaa)
SEEDS=(0,0x7fffffffffffffff,0x8000000080000000,0xffffffffffffffff)
ALIASES=('distinct','destination_rm','destination_rs','same_sources','accumulator_rm')
def op_name(op):return op[0]+('t' if op[1] else 'b')+('t' if op[2] else 'b') if op[0] in ('smul','smla','smlal') else op[0]+('t' if op[2] else 'b')
def lane(value,top,guard):return (value<<16)|guard if top else (guard<<16)|value
def groups(full=True):
    result=[];halves=range(65536) if full else (0,1,32767,32768,65535)
    boundary=PATTERNS if full else (0,0xffffffff,0x80007fff)
    for op in OPS:
        kind,x,y=op;name=op_name(op)
        if kind.endswith('w'):
            left=(0x7fffffff,0x80000000,0xffffffff,0x5555aaaa)
            right=tuple(lane(h,y,0xa55a) for h in halves)
        else:
            left=tuple(lane(h,x,0x5aa5) for h in halves)
            right=tuple(lane(h,y,0xa55a) for h in (0,1,32767,32768))
        result.append((name+'_sweep',op,left,right,0x7fffffffffffffff,14,'distinct',(0,1)))
        seeds=SEEDS if kind in ('smla','smlal','smlaw') else (0,)
        for seed_index,seed in enumerate(seeds):
            for cond in range(15):
                for alias in ALIASES:
                    result.append((f'{name}_boundary_{seed_index}_{cond}_{alias}',op,boundary,boundary,seed,cond,alias,tuple(range(32))))
    return tuple(result)
def cases(g):return len(g[2])*len(g[3])*len(g[7])
def registers(op,alias):
    kind,_,_=op
    rm=0;rs=0 if alias=='same_sources' else 1
    low=0 if alias=='destination_rm' else 1 if alias=='destination_rs' else 4
    high=1 if alias=='accumulator_rm' and kind=='smlal' else 5
    if alias=='accumulator_rm' and kind=='smlal':low=0
    rn=0 if alias=='accumulator_rm' else 2
    return low,high,rm,rs,rn
# Encoding comes from ARM DDI0100I A4.1.74/.77/.79/.86/.88.
def instruction(op,alias,cond=14,mutant=False):
    kind,x,y=op;low,high,rm,rs,rn=registers(op,alias)
    if mutant:y^=1
    common=(cond<<28)|(rs<<8)|(y<<6)|rm
    if kind=='smul':return common|0x01600080|(low<<16)|(x<<5)
    if kind=='smla':return common|0x01000080|(low<<16)|(rn<<12)|(x<<5)
    if kind=='smlal':return common|0x01400080|(high<<16)|(low<<12)|(x<<5)
    if kind=='smulw':return common|0x012000a0|(low<<16)
    return common|0x01200080|(low<<16)|(rn<<12)
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
    for name,op,left,right,seed,cond,alias,flags in domain:
        p.puts(name+'_header');p.address(8,name+'_left_values');p.constant(9,len(left));p.mark(name+'_left')
        p.address(10,name+'_right_values');p.constant(11,len(right));p.mark(name+'_right')
        p.constant(6,0);p.memory(6,13,load=False);p.mark(name+'_flags')
        p.memory(0,8);p.memory(1,10);p.constant(2,seed&0xffffffff);p.constant(3,0xfacefeed)
        p.constant(4,seed&0xffffffff if op[0]=='smlal' else 0xdeadbeef);p.constant(5,seed>>32)
        p.memory(6,13);p.emit(0xe1a06006|(27<<7));p.emit(0xe128f006)
        p.emit(instruction(op,alias,cond,mutant));p.emit(0xe10f6000);p.memory(6,13,4,load=False)
        p.puts('row')
        for reg in range(6):p.emit(0xe1a06000|reg);p.branch('word',link=True)
        p.memory(6,13,4);p.branch('word',link=True)
        p.constant(6,10);p.memory(6,12,load=False,byte=True)
        p.memory(6,13);p.emit(0xe2866001);p.memory(6,13,load=False);p.emit(0xe3560000|len(flags));p.branch(name+'_flags',cond=1)
        p.emit(0xe28aa004);p.emit(0xe25bb001);p.branch(name+'_right',cond=1)
        p.emit(0xe2888004);p.emit(0xe2599001);p.branch(name+'_left',cond=1)
    p.puts('complete');p.constant(6,0);p.memory(6,12,4,load=False);p.mark('idle');p.branch('idle')
    p.mark('puts');p.memory(7,6,load=True,byte=True);p.emit(0xe3570000);p.emit(0x012fff1e)
    p.memory(7,12,load=False,byte=True);p.emit(0xe2866001);p.branch('puts')
    p.mark('word');p.memory(6,12,8,load=False);p.emit(0xe12fff1e)
    # Deduplicate input tables so exhaustive lane sweeps fit the 4 MiB RAM profile.
    tables={}
    for g in domain:
        for label,values in ((g[0]+'_left_values',g[2]),(g[0]+'_right_values',g[3])):
            key=values
            if key in tables:p.labels[label]=tables[key]
            else:
                p.mark(label);tables[key]=p.pc
                for value in values:p.emit(value)
    p.string('header',f'VELOCE 1 ds dsp_multiply_registers cpu=9\nINFO full={int(full)} groups={len(domain)} words=7\n')
    for g in domain:p.string(g[0]+'_header',f'BEGIN {g[0]} cases={cases(g)} words=7\n')
    p.string('row','REG r=');p.string('complete',f'LOG complete cases={sum(cases(g) for g in domain)}\nEND observations_complete\n')
    while len(p.data)%4:p.data.append(0)
    p.resolve()
    if p.pc>0x022f0000:raise ValueError('ARM9 code/data overlaps idle ARM7 placement')
    return bytes(p.data)
def build(full=True,mutant=False):
    nine=program(full,mutant);seven=bytes.fromhex('feffffea');offset7=(512+len(nine)+511)&~511
    rom=bytearray(offset7+len(seven));rom[:12]=b'VELOCE DSP  ';rom[12:16]=b'VDSP';rom[0x180:0x188]=b'VDSHB001'
    for offset,fields in ((0x20,(512,0x02004000,0x02004000,len(nine))),(0x30,(offset7,0x02300000,0x02300000,len(seven)))):
        for n,field in enumerate(fields):rom[offset+n*4:offset+n*4+4]=field.to_bytes(4,'little')
    rom[0x80:0x84]=len(rom).to_bytes(4,'little');rom[0x84:0x88]=(512).to_bytes(4,'little')
    rom[512:512+len(nine)]=nine;rom[offset7:]=seven;return bytes(rom)
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('dsp_multiply_registers.nds'))
    args=ap.parse_args();rom=build();args.output.write_bytes(rom);print(hashlib.sha256(rom).hexdigest(),args.output)
