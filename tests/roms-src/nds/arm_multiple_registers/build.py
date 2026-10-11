#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original dual-CPU DS ARM register-list cartridge, no SDK/expected answers."""
import argparse,hashlib
from dataclasses import dataclass
from pathlib import Path
TARGET=0x02001880;SYNC=0x02001000
MODES=('ia','ib','da','db')
BOUNDARY_LISTS=(1,2,8,0x80,0x100,0x1000,0x2000,0x4000,0x8000,0x8001,0x5555,0xaaaa,0x7fff,0xffff)
SEEDS=(0,1,0xffffffff,0x7fffffff,0x80000000,0x55555555,0xaaaaaaaa,0x89abcdef)
@dataclass(frozen=True)
class Group:
    load:bool
    mode:str
    wb:bool
    rn:int=3
    domain:str='lists'
    cond:int=14
def name(g):return f'{"ldm" if g.load else "stm"}_{g.mode}_{int(g.wb)}_{g.rn}_{g.domain}_{g.cond}'
def legal(g,mask):
    return mask!=0 and (not g.wb or not(mask&(1<<g.rn)) or (not g.load and not(mask&((1<<g.rn)-1))))
def masks(g,full=True):return tuple(m for m in (range(1,65536) if g.domain=='lists' and full else BOUNDARY_LISTS) if legal(g,m))
def groups(full=True):
    out=[]
    for load in (False,True):
        for mode in MODES:
            for wb in (False,True):
                out.append(Group(load,mode,wb))
                for rn in range(15):out.append(Group(load,mode,wb,rn,'boundary'))
                for cond in range(14):out.append(Group(load,mode,wb,3,'condition',cond))
    return tuple(out)
def seeds(g):return (None,) if g.domain=='lists' else SEEDS if g.domain=='boundary' else (0x89abcdef,)
def flag_values(g,cpu,mask):return (mask&((1<<(5 if cpu==9 else 4))-1),) if g.domain=='lists' else tuple(range(32 if cpu==9 else 16))
def cases(g,cpu,full=True):return sum(len(seeds(g))*len(flag_values(g,cpu,m)) for m in masks(g,full))
def start_base(g,count):return TARGET if g.mode=='ia' else TARGET-4 if g.mode=='ib' else TARGET+4*count-4 if g.mode=='da' else TARGET+4*count
def instruction(g,mask,mutant=False):
    # Executed mutant changes increment-before to increment-after only.
    mode='ia' if mutant and not g.load and g.mode=='ib' else g.mode
    return (g.cond<<28)|0x08000000|(int(mode in ('ib','db'))<<24)|(int(mode in ('ia','ib'))<<23)|(int(g.wb)<<21)|(int(g.load)<<20)|(g.rn<<16)|mask
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
    def literal(self,reg,label,load=True):
        self.fixups.append((len(self.data),label,'literal',reg,load));self.emit(0)
    def resolve(self):
        for offset,label,kind,reg,link in self.fixups:
            target=self.labels[label]
            if kind=='branch':
                delta=target-(self.base+offset+8)
                if delta%4 or not -(1<<25)<=delta<(1<<25): raise ValueError('branch range')
                words=[(reg<<28)|0x0a000000|(int(link)<<24)|((delta//4)&0xffffff)]
            elif kind=='literal':
                delta=target-(self.base+offset+8)
                if abs(delta)>4095:raise ValueError('literal range')
                words=[0xe50f0000|(int(delta>=0)<<23)|(int(link)<<20)|(reg<<12)|abs(delta)]
            else:
                words=[0xe3a00000|(reg<<12)|immediate(target&255)]
                for count in (8,16,24): words.append(0xe3800000|(reg<<16)|(reg<<12)|immediate(target&(255<<count)))
            for n,word in enumerate(words): self.data[offset+n*4:offset+n*4+4]=word.to_bytes(4,'little')

def program(cpu,full=True,mutant=False,layout=False):
    p=Program(0x02004000 if cpu==9 else 0x02200000);domain=groups(full)
    p.constant(12,0x04ff0000);p.constant(13,SYNC)
    if cpu==7:
        p.mark('wait');p.memory(6,13);p.emit(0xe3560001);p.branch('wait',cond=1)
    p.puts('header')
    for g in domain:
        label=name(g);ms=masks(g,full)
        p.puts(label+'_header');p.address(8,label+'_masks');p.constant(9,len(ms));p.mark(label+'_mask')
        p.memory(2,8);p.address(10,label+'_seeds');p.constant(11,len(seeds(g)));p.mark(label+'_seed')
        if g.domain=='lists':p.emit(0xe1a06002);p.emit(0xe1866802);p.memory(6,13,8,load=False)
        else:p.memory(6,10);p.memory(6,13,8,load=False)
        if g.domain=='lists':p.emit(0xe2025000|((1<<(5 if cpu==9 else 4))-1))
        else:p.constant(5,0)
        p.mark(label+'_flags')
        p.constant(4,TARGET-4);p.constant(6,0xfacefeed);p.memory(6,4,load=False)
        p.constant(6,0xcafebabe);p.memory(6,4,68,load=False)
        p.address(0,'stimuli');p.memory(3,13,8)
        for reg in range(15):
            p.constant(6,(reg+1)*0x01020304);p.emit(0xe0266003);p.memory(6,0,reg*4,load=False)
        p.constant(1,0);p.constant(7,1)
        for reg in range(16):
            p.emit(0xe1120007);p.emit(0x12811001);p.emit(0xe1a07087)
        # r1 is the number of transfer words. Adjust base for a fixed RAM span.
        p.constant(6,TARGET)
        if g.mode in ('da','db'):p.emit(0xe0866101)
        if g.mode in ('ib','da'):p.emit(0xe2466004)
        p.memory(6,0,g.rn*4,load=False)
        for n in range(16):
            p.constant(6,(n+1)*0x11111111&0xffffffff);p.emit(0xe0266003);p.memory(6,4,4+n*4,load=False)
        if g.load:
            # If PC is listed, its last memory word targets the capture block.
            p.address(6,'capture');p.constant(7,0x8000);p.emit(0xe1120007);p.branch(label+'_pc_ready',cond=0)
            p.emit(0xe0846101);p.memory(6,13,12,load=False)
            p.address(7,'capture');p.memory(6,13,12);p.memory(7,6,load=False)
            p.mark(label+'_pc_ready')
        p.constant(6,instruction(g,0,mutant));p.emit(0xe1866002);p.address(7,'transfer');p.memory(6,7,load=False)
        p.branch('invoke',link=True)
        p.puts('row');p.emit(0xe1a06002);p.branch('word',link=True)
        p.address(0,'captured')
        for offset in range(0,64,4):p.memory(6,0,offset);p.branch('word',link=True)
        p.constant(6,10);p.memory(6,12,load=False,byte=True);p.puts('memory_row')
        for offset in range(0,72,4):p.memory(6,4,offset);p.branch('word',link=True)
        p.constant(6,10);p.memory(6,12,load=False,byte=True)
        if g.domain!='lists':p.emit(0xe2855001);p.emit(0xe3550000|(32 if cpu==9 else 16));p.branch(label+'_flags',cond=1)
        p.emit(0xe28aa004);p.emit(0xe25bb001);p.branch(label+'_seed',cond=1)
        p.emit(0xe2888004);p.emit(0xe2599001);p.branch(label+'_mask',cond=1)
    p.puts('complete')
    if cpu==9:p.constant(6,1);p.memory(6,13,load=False)
    p.constant(6,0);p.memory(6,12,4,load=False);p.mark('idle');p.branch('idle')
    p.mark('invoke')
    for reg in range(15):p.literal(reg,'before_'+str(reg),False)
    p.emit(0xe1a06005|(28<<7 if cpu==7 else 27<<7));p.emit(0xe128f006)
    for reg in range(15):p.literal(reg,'stimuli_'+str(reg))
    p.mark('transfer');p.emit(0)
    p.mark('capture')
    for reg in range(15):p.literal(reg,'captured_'+str(reg),False)
    p.emit(0xe10f0000);p.literal(0,'captured_15',False)
    for reg in range(15):p.literal(reg,'before_'+str(reg))
    p.emit(0xe12fff1e)
    for table in ('before','stimuli','captured'):
        p.mark(table)
        for reg in range(16):p.mark(table+'_'+str(reg));p.emit(0)
    p.mark('puts');p.memory(7,6,load=True,byte=True);p.emit(0xe3570000);p.emit(0x012fff1e)
    p.memory(7,12,load=False,byte=True);p.emit(0xe2866001);p.branch('puts')
    p.mark('word');p.memory(6,12,8,load=False);p.emit(0xe12fff1e)
    tables={}
    for g in domain:
        label=name(g)
        for suffix,vals in (('_masks',masks(g,full)),('_seeds',tuple(0 if x is None else x for x in seeds(g)))):
            if vals in tables:p.labels[label+suffix]=tables[vals]
            else:
                p.mark(label+suffix);tables[vals]=p.pc
                for value in vals:p.emit(value)
    p.string('header',f'VELOCE 1 ds arm_multiple_registers cpu={cpu}\nINFO full={int(full)} groups={len(domain)} transfer={p.labels["transfer"]:08x} capture={p.labels["capture"]:08x} words=35\n')
    for g in domain:p.string(name(g)+'_header',f'BEGIN {name(g)} cases={cases(g,cpu,full)} words=35\n')
    p.string('row','REG r=');p.string('memory_row','MEM m=');p.string('complete',f'LOG complete cpu={cpu} cases={sum(cases(g,cpu,full) for g in domain)}\n'+('END observations_complete\n' if cpu==7 else ''))
    while len(p.data)%4:p.data.append(0)
    p.resolve()
    if len(p.data)>0x1fc000:raise ValueError('driver exceeds assigned 2 MiB segment')
    return (bytes(p.data),p.labels) if layout else bytes(p.data)
def build(full=True,mutant=False):
    nine=program(9,full,mutant);seven=program(7,full,mutant);offset7=(512+len(nine)+511)&~511
    rom=bytearray(offset7+len(seven));rom[:12]=b'VELOCE ARMB ';rom[12:16]=b'VAMB';rom[0x180:0x188]=b'VDSHB001'
    for offset,fields in ((0x20,(512,0x02004000,0x02004000,len(nine))),(0x30,(offset7,0x02200000,0x02200000,len(seven)))):
        for n,field in enumerate(fields):rom[offset+n*4:offset+n*4+4]=field.to_bytes(4,'little')
    rom[0x80:0x84]=len(rom).to_bytes(4,'little');rom[0x84:0x88]=(512).to_bytes(4,'little');rom[512:512+len(nine)]=nine;rom[offset7:]=seven
    return bytes(rom)
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('arm_multiple_registers.nds'));args=ap.parse_args();rom=build();args.output.write_bytes(rom);print(hashlib.sha256(rom).hexdigest(),args.output)
