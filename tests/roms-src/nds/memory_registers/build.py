#!/usr/bin/env python3
"""Original table-driven DS ARM/Thumb memory register cartridge, no SDK."""
import argparse
import hashlib
from dataclasses import dataclass
from pathlib import Path
PATTERNS=(0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,0xfffffffe,0xffffffff,0x55555555,0x89abcdef)
HALF_BOUNDARIES=(0,1,2,15,16,127,128,255,0x7fff,0x8000,0xff00,0xff7f,0xff80,0xfffe,0xffff,0x5555)
THUMB_REG=('str','strh','strb','ldrsb','ldr','ldrh','ldrb','ldrsh')
DATA_WORD=0x02001808
GUARDS=(0xdecafbad,0x13579bdf)
@dataclass(frozen=True)
class Group:
    state:str
    op:str
    form:str='reg'
    index:str='offset'
    up:bool=True
    offset:int=0
    lane:int=0
    domain:str='pattern'
def width(g): return 1 if g.op in ('strb','ldrb','ldrsb') else 2 if g.op in ('strh','ldrh','ldrsh') else 4
def values(domain,full=True):
    if domain=='half': return tuple(n|((~n&65535)<<16) for n in (range(65536) if full else HALF_BOUNDARIES))
    if domain=='byte': return tuple(n*0x01010101 for n in (range(256) if full else (0,127,128,255)))
    return PATTERNS
def groups(full=True):
    result=[]
    for state in ('arm','thumb'):
        for op in ('strh','ldrh','ldrsh'):
            for lane in (0,2): result.append(Group(state,op,lane=lane,domain='half'))
    for state,ops in (('arm',('ldrsb',)),('thumb',('strb','ldrb','ldrsb'))):
        for op in ops:
            for lane in range(4): result.append(Group(state,op,lane=lane,domain='byte'))
    for op in ('str','ldr'): result.append(Group('thumb',op))
    offsets=range(256) if full else (0,1,127,255)
    for op in ('strh','ldrh','ldrsb','ldrsh'):
        for form in ('imm','reg'):
            for index in ('offset','pre','post'):
                for up in (False,True):
                    for offset in offsets: result.append(Group('arm',op,form,index,up,offset))
    for op in THUMB_REG:
        for offset in offsets: result.append(Group('thumb',op,offset=offset))
    for op in ('str','ldr','strb','ldrb','strh','ldrh'):
        for offset in (range(32) if full else (0,1,15,31)): result.append(Group('thumb',op,'imm',offset=offset))
    for op in ('str','ldr'):
        for offset in offsets: result.append(Group('thumb',op,'sp',offset=offset))
    for offset in offsets: result.append(Group('thumb','ldr','pc',offset=offset))
    return tuple(result)
def cases(g,cpu=7,full=True): return len(values(g.domain,full))*(32 if cpu==9 else 16)
def encoding(g,wrong_signed=False,wrong_width=False,wrong_store=False):
    if wrong_store and (g.state,g.form,g.op)==('thumb','reg','strh'): return 0x46c0 # MOV r8,r8
    op='ldrh' if wrong_signed and g.op=='ldrsh' else g.op
    if wrong_width and (g.state,g.form,g.op)==('thumb','reg','strh'): op='strb'
    if g.state=='arm':
        kind=2 if op=='ldrsb' else 3 if op=='ldrsh' else 1
        immediate=g.form=='imm'
        field=((g.offset&240)<<4)|(g.offset&15) if immediate else 2
        return 0xe0014090|(int(g.index!='post')<<24)|(int(g.up)<<23)|(int(immediate)<<22)|(int(g.index=='pre')<<21)|(int(op.startswith('ldr'))<<20)|(kind<<5)|field
    if g.form=='reg': return 0x508c|(THUMB_REG.index(op)<<9)
    if g.form=='sp': return 0x9400|(int(op=='ldr')<<11)|g.offset
    if g.form=='pc': return 0x4c00|g.offset
    base=dict(str=0x600c,ldr=0x680c,strb=0x700c,ldrb=0x780c,strh=0x800c,ldrh=0x880c)[op]
    return base|(g.offset<<6)
def thunk_size(g): return 8 if g.state=='arm' else 1040 if g.form=='pc' else 4

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

def program(cpu,full=True,wrong_signed=False,wrong_width=False,wrong_store=False):
    p=Program(0x02004000 if cpu==9 else 0x02200000); domain=groups(full)
    p.constant(12,0x04ff0000); p.constant(13,0x02001000); p.constant(10,0x02001080)
    if cpu==7:
        p.mark('wait'); p.memory(6,13); p.emit(0xe3560001); p.branch('wait',cond=1)
    p.puts('header'); p.address(6,'descriptors'); p.memory(6,10,load=False)
    p.mark('group'); p.memory(7,10); p.memory(6,7); p.branch('puts',link=True)
    p.memory(7,10); p.memory(8,7,8); p.memory(9,7,12)
    p.mark('value'); p.memory(0,8); p.constant(11,0)
    p.mark('flags'); p.memory(7,10); p.memory(1,7,16); p.memory(2,7,20); p.memory(3,7,24); p.memory(6,7,28)
    p.memory(3,10,4,load=False); p.emit(0xe3560000); p.emit(0xe1a06000)
    # Stores start with the complement of the source's low byte in every
    # lane. A missing store must therefore change the observed word.
    p.emit(0x120060ff); p.emit(0x11866406); p.emit(0x11866806); p.emit(0x11e06006)
    p.memory(6,3,load=False)
    for offset,guard in zip((4,8),GUARDS): p.constant(6,guard); p.memory(6,3,offset,load=False)
    p.emit(0xe1a0d001); p.emit(0xe1a04000); p.emit(0xe1a0300b|((27 if cpu==9 else 28)<<7)); p.emit(0xe128f003)
    p.memory(7,10); p.memory(6,7,4); p.address(7,'returned'); p.emit(0xe12fff16)
    p.mark('returned'); p.emit(0xe10f5000); p.memory(13,10,8,load=False); p.constant(13,0x02001000)
    p.puts('row')
    for reg in (4,5,1,2,0): p.emit(0xe1a06000|reg); p.branch('hex',link=True)
    for offset in (0,4,8): p.memory(7,10,4); p.memory(6,7,offset); p.branch('hex',link=True)
    p.memory(6,10,8); p.branch('hex',link=True)
    p.constant(6,10); p.memory(6,12,load=False,byte=True)
    p.emit(0xe28bb001); p.emit(0xe35b0000|(32 if cpu==9 else 16)); p.branch('flags',cond=1)
    p.emit(0xe2888004); p.emit(0xe2599001); p.branch('value',cond=1)
    p.memory(6,10); p.emit(0xe2866020); p.memory(6,10,load=False); p.address(3,'descriptors_end'); p.emit(0xe1560003); p.branch('group',cond=1)
    p.puts('complete')
    if cpu==9: p.constant(6,1); p.memory(6,13,load=False)
    p.constant(6,0); p.memory(6,12,4,load=False); p.branch('idle'); p.mark('idle'); p.branch('idle')
    p.mark('puts'); p.memory(7,6,load=True,byte=True); p.emit(0xe3570000); p.emit(0x012fff1e)
    p.memory(7,12,load=False,byte=True); p.emit(0xe2866001); p.branch('puts')
    p.mark('hex'); p.constant(3,28); p.mark('hex_loop')
    p.emit(0xe1a07336); p.emit(0xe207700f); p.emit(0xe357000a); p.emit(0x32877030); p.emit(0x22877057)
    p.memory(7,12,load=False,byte=True); p.emit(0xe2533004); p.branch('hex_loop',cond=10); p.emit(0xe12fff1e)
    p.data.extend(b'VMEM0001')
    for index,g in enumerate(domain):
        p.mark(f'g{index}_stub'); word=encoding(g,wrong_signed,wrong_width,wrong_store)
        if g.state=='arm': p.emit(word); p.emit(0xe12fff17)
        else:
            p.emit(word|(0x4738<<16))
            if g.form=='pc': p.data.extend(bytes(thunk_size(g)-4))
    for kind in ('half','byte','pattern'):
        p.mark(kind+'_values')
        for value in values(kind,full): p.emit(value)
    total=sum(cases(g,cpu,full) for g in domain)
    p.string('header',f'VELOCE 1 ds memory_registers cpu={cpu}\nINFO full={int(full)} groups={len(domain)} flags={32 if cpu==9 else 16}\n')
    for index,g in enumerate(domain): p.string(f'g{index}_header',f'BEGIN cpu{cpu}_g{index:05d} cases={cases(g,cpu,full)} words=9\n')
    p.string('row','REG r='); p.string('complete',f'LOG cpu={cpu} complete cases={total}\n'+('END observations_complete\n' if cpu==7 else ''))
    while len(p.data)%4: p.data.append(0)
    p.mark('descriptors')
    for index,g in enumerate(domain):
        stub=p.labels[f'g{index}_stub']
        target=((stub+4)&~3)+g.offset*4 if g.form=='pc' else DATA_WORD+g.lane
        amount=g.offset*(width(g) if g.state=='thumb' and g.form=='imm' else 4 if g.form in ('sp','pc') else 1)
        base=target if g.state=='arm' and g.index=='post' else (target-amount if g.up else target+amount)&0xffffffff
        fields=(p.labels[f'g{index}_header'],stub|int(g.state=='thumb'),p.labels[g.domain+'_values'],len(values(g.domain,full)),base,g.offset,target&~3,int(g.op.startswith('str')))
        for field in fields: p.emit(field)
    p.mark('descriptors_end'); p.resolve(); return bytes(p.data)
def build(full=True,wrong_signed=False,wrong_width=False,wrong_store=False):
    nine=program(9,full,wrong_signed,wrong_width,wrong_store); seven=program(7,full,wrong_signed,wrong_width,wrong_store)
    require_size=len(nine)<=0x1fc000 and len(seven)<=0x200000
    if not require_size: raise ValueError('dual programs exceed disjoint main RAM regions')
    offset7=(512+len(nine)+511)&~511
    rom=bytearray(offset7+len(seven)); rom[:12]=b'VELOCE MEM  '; rom[12:16]=b'VMEM'; rom[0x180:0x188]=b'VDSHB001'
    for offset,fields in ((0x20,(512,0x02004000,0x02004000,len(nine))),(0x30,(offset7,0x02200000,0x02200000,len(seven)))):
        for n,field in enumerate(fields): rom[offset+n*4:offset+n*4+4]=field.to_bytes(4,'little')
    rom[0x80:0x84]=len(rom).to_bytes(4,'little'); rom[0x84:0x88]=(512).to_bytes(4,'little')
    rom[512:512+len(nine)]=nine; rom[offset7:]=seven; return bytes(rom)
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('memory_registers.nds'))
    args=parser.parse_args(); rom=build(); args.output.write_bytes(rom); print(hashlib.sha256(rom).hexdigest(),args.output)
