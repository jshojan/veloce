#!/usr/bin/env python3
"""Original DS Thumb PUSH/POP/STMIA/LDMIA cartridge, no SDK or answers."""
import argparse
import hashlib
from dataclasses import dataclass
from pathlib import Path
PATTERNS=(0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,0xfffffffe,0xffffffff,0x55555555,0x89abcdef)
SYNC=0x020e8000
BUFFER=0x020e8400
DATA=0x020e9004
SENTINEL=0xdeadbeef
@dataclass(frozen=True)
class Group:
    kind:int
    base:int
    mask:int
    target:int=0
    @property
    def count(self): return self.mask.bit_count()
    @property
    def instruction(self): return (0xb400,0xbc00,0xc000,0xc800)[self.kind]|self.mask|(self.base<<8 if self.kind>=2 else 0)
def groups(full=True):
    masks=range(1,512) if full else (1,2,0x55,0x80,0xff,0x100,0x101,0x155,0x180,0x1ff)
    result=[]
    for kind in range(4):
        for base in range(1 if kind<2 else 8):
            for mask in masks:
                if kind>=2 and mask>255: continue
                if kind==2 and mask&(1<<base) and mask&((1<<base)-1): continue
                for target in ((0,1,3) if kind==1 and mask&256 else (0,)):
                    result.append(Group(kind,base,mask,target))
    return tuple(result)
def seeds(full=True): return PATTERNS if full else (0,0xffffffff,0x80000000,0x89abcdef)
def cases(cpu,full=True): return len(groups(full))*len(seeds(full))*(32 if cpu==9 else 16)
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

def xor_tag(p,reg,tag):
    for shift in (0,8,16,24):
        chunk=tag&(255<<shift)
        if chunk: p.emit(0xe2200000|(reg<<16)|(reg<<12)|immediate(chunk))
def program(cpu,full=True,skip_store=False,skip_load=False):
    p=Program(0x02000000 if cpu==9 else 0x02100000 if full else 0x03800000); domain=groups(full)
    p.constant(10,BUFFER); p.constant(12,0x04ff0000)
    if cpu==7:
        p.constant(6,SYNC); p.mark('wait'); p.memory(7,6); p.emit(0xe3570001); p.branch('wait',cond=1)
    p.puts('header'); p.address(11,'descriptors')
    p.mark('group'); p.memory(6,11,24); p.branch('puts',link=True)
    p.address(6,'values'); p.memory(6,10,104,load=False)
    p.mark('seed'); p.memory(6,10,104); p.memory(0,6); p.memory(0,10,100,load=False); p.constant(8,0)
    p.mark('flags'); p.memory(0,10,100); p.constant(6,DATA-4)
    for n in range(11):
        p.emit(0xe1e01000); xor_tag(p,1,(n+17)*0x01010101); p.memory(1,6,n*4,load=False)
    p.memory(2,11,12); p.memory(7,11,16); p.constant(6,DATA); p.emit(0xe3520000); p.emit(0x17862007)
    for reg in range(8):
        p.memory(reg,10,100); xor_tag(p,reg,(reg+1)*0x01010101)
    p.memory(14,10,100); xor_tag(p,14,15*0x01010101)
    p.emit(0xe1a09008|((27 if cpu==9 else 28)<<7)); p.emit(0xe128f009)
    p.memory(9,11); p.address(12,'returned'); p.emit(0xe12fff19)
    p.mark('returned'); p.memory(9,10,44,load=False); p.emit(0xe10f9000); p.memory(9,10,40,load=False)
    for reg in range(8): p.memory(reg,10,reg*4,load=False)
    p.memory(13,10,32,load=False); p.memory(14,10,36,load=False)
    p.memory(6,10,100); p.memory(6,10,48,load=False)
    p.memory(6,11,20); p.memory(6,10,52,load=False)
    p.constant(6,DATA-4)
    for n in range(11): p.memory(7,6,n*4); p.memory(7,10,56+n*4,load=False)
    p.constant(12,0x04ff0000); p.puts('row')
    for n in range(25): p.memory(6,10,n*4); p.branch('word',link=True)
    p.constant(6,10); p.memory(6,12,load=False,byte=True)
    p.emit(0xe2888001); p.emit(0xe3580000|(32 if cpu==9 else 16)); p.branch('flags',cond=1)
    p.memory(6,10,104); p.emit(0xe2866004); p.memory(6,10,104,load=False); p.address(7,'values_end'); p.emit(0xe1560007); p.branch('seed',cond=1)
    p.emit(0xe28bb01c); p.address(6,'descriptors_end'); p.emit(0xe15b0006); p.branch('group',cond=1)
    p.puts('complete')
    if cpu==9: p.constant(6,SYNC); p.constant(7,1); p.memory(7,6,load=False)
    p.constant(6,0); p.memory(6,12,4,load=False); p.mark('idle'); p.branch('idle')
    p.mark('puts'); p.memory(7,6,load=True,byte=True); p.emit(0xe3570000); p.emit(0x012fff1e)
    p.memory(7,12,load=False,byte=True); p.emit(0xe2866001); p.branch('puts')
    p.mark('word'); p.memory(6,12,8,load=False); p.emit(0xe12fff1e)
    p.mark('values')
    for value in seeds(full): p.emit(value)
    p.mark('values_end')
    p.string('header',f'VELOCE 1 ds multiple_registers cpu={cpu}\nINFO full={int(full)} groups={len(domain)} flags={32 if cpu==9 else 16}\n')
    for index,g in enumerate(domain): p.string(f'g{index}',f'BEGIN cpu{cpu}_g{index:05d} cases={len(seeds(full))*(32 if cpu==9 else 16)} words=25\n')
    p.string('row','REG r='); p.string('complete',f'LOG cpu={cpu} complete cases={cases(cpu,full)}\n'+('END observations_complete\n' if cpu==7 else ''))
    while len(p.data)%4: p.data.append(0)
    p.data.extend(b'VTGT0001')
    for form in (0,1,3):
        p.mark(f'target{form}')
        if form==0 and cpu==9:
            for word in (0xe1a0900f,0xe12fff1c,0,0): p.emit(word)
        else:
            code=(0x46c0,0x46f9,0x4760) if form==3 else (0x46f9,0x4760)
            for half in code: p.data.extend(half.to_bytes(2,'little'))
            p.data.extend(bytes(16-len(code)*2))
    p.data.extend(b'VSTK0001')
    for index,g in enumerate(domain):
        p.mark(f'thunk{index}'); p.memory(13,11,4)
        if g.kind>=2: p.memory(g.base,11,8)
        else: p.emit(0xe1a08008)
        p.emit(0xe28f9001); p.emit(0xe12fff19)
        half=0x46c0 if (skip_store and g.kind in (0,2)) or (skip_load and g.kind in (1,3)) else g.instruction
        p.data.extend(half.to_bytes(2,'little')); p.data.extend((0x4760).to_bytes(2,'little'))
    p.mark('descriptors')
    for index,g in enumerate(domain):
        target=p.labels[f'target{g.target}']+(2 if g.target==3 else 0)|int(g.target!=0)
        items=(p.labels[f'thunk{index}'],DATA+g.count*4 if g.kind==0 else DATA,DATA,target if g.kind==1 and g.mask&256 else 0,(g.count-1)*4,g.instruction,p.labels[f'g{index}'])
        for value in items: p.emit(value)
    p.mark('descriptors_end'); p.resolve(); return bytes(p.data)
def build(full=True,skip_store=False,skip_load=False):
    nine=program(9,full,skip_store,skip_load); seven=program(7,full,skip_store,skip_load)
    offset7=512+len(nine); rom=bytearray(offset7+len(seven)); rom[:12]=b'VELOCE MULT '; rom[12:16]=b'VMUL'; rom[0x180:0x188]=b'VDSHB001'
    for offset,items in ((0x20,(512,0x02000000,0x02000000,len(nine))),(0x30,(offset7,0x02100000 if full else 0x03800000,0x02100000 if full else 0x03800000,len(seven)))):
        for n,value in enumerate(items): rom[offset+n*4:offset+n*4+4]=value.to_bytes(4,'little')
    if len(nine)>0xe8000 or len(seven)>(0x80000 if full else 0x10000): raise ValueError('driver exceeds reserved segment')
    rom[512:offset7]=nine; rom[offset7:]=seven; return bytes(rom)
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('multiple_registers.nds'))
    args=parser.parse_args(); rom=build(); args.output.write_bytes(rom); print(hashlib.sha256(rom).hexdigest(),args.output)
