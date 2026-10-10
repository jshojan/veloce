#!/usr/bin/env python3
"""Original paired Thumb BL/BLX register cartridge, no SDK or expected answers."""
import argparse
import hashlib
from dataclasses import dataclass
from pathlib import Path
PATTERNS=(0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,0xfffffffe,0xffffffff,0x55555555,0x89abcdef)
SYNC=0x02001000
LINK_WORD=0x02001800
BL_SITE=0x02802200
BLX_SITE=0x02802400
LOW_BASE=0x02802ffc
LINK_SEED=0xfeedface
@dataclass(frozen=True)
class Group:
    kind:int
    align:int
    @property
    def name(self): return ('bl_high','bl_low','blx_high','blx_low')[self.kind]+f'_a{self.align}'
    @property
    def exchange(self): return bool(self.kind&2)
    @property
    def high(self): return not (self.kind&1)
    @property
    def base(self): return (BLX_SITE if self.exchange else BL_SITE) if self.high else LOW_BASE
def groups(cpu=9): return tuple(Group(kind,align) for kind in range(4 if cpu==9 else 2) for align in (0,2))
def fields(g,full=True):
    if full: return tuple(range(0,2048,2 if g.kind==3 else 1))
    return (0,2,1022,1024,2046) if g.kind==3 else (0,1,2,1023,1024,1025,2046,2047)
def seeds(full=True): return PATTERNS if full else (0,0xffffffff,0x80000000,0x89abcdef)
def cases(g,cpu,full=True): return len(fields(g,full))*len(seeds(full))*(32 if cpu==9 else 16)
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

def program(cpu,full=True,wrong_sign=False):
    p=Program(0x02000800 if cpu==9 else 0x03800000); domain=groups(cpu)
    p.constant(12,0x04ff0000); p.constant(13,SYNC)
    if cpu==7:
        p.mark('wait'); p.memory(6,13); p.emit(0xe3560001); p.branch('wait',cond=1)
    p.puts('header'); p.address(11,'descriptors')
    p.mark('group'); p.memory(6,11); p.branch('puts',link=True); p.constant(9,0)
    p.mark('field'); p.address(10,'values')
    p.mark('seed'); p.memory(0,10); p.constant(8,0)
    p.mark('flags'); p.memory(6,11,4)
    if full:
        p.emit(0xe1a07009); p.emit(0xe3560003); p.emit(0x01a07089) # BLX low fields are even
    else:
        p.memory(7,11,20); p.emit(0xe7977109) # LDR r7,[r7,r9,LSL #2]
    p.emit(0xe3160001); p.branch('low',cond=1)
    p.emit(0xe1a01007); p.constant(2,0); p.emit(0xe3560002); p.emit(0x03a02002); p.branch('inputs')
    p.mark('low'); p.emit(0xe1a02007); p.constant(1,0); p.emit(0xe3560003); p.emit(0x03a01001)
    p.mark('inputs'); p.memory(3,11,12); p.memory(7,11,8); p.emit(0xe0833007)
    p.emit(0xe3160001); p.emit(0x10433082) # low groups position the paired call before a fixed target
    p.emit(0xe1d360b0); p.memory(6,13,0x84,load=False)
    p.emit(0xe1d360b2); p.memory(6,13,0x88,load=False)
    p.constant(6,0xf000); p.emit(0xe1866001)
    if wrong_sign: p.emit(0xe2266b01) # EOR r6,r6,#0x400: wrong signed high field, same RAM mirror
    p.emit(0xe1c360b0)
    p.memory(7,11,4); p.emit(0xe3170002); p.constant(6,0xf800); p.branch('suffix',cond=0)
    p.constant(6,0xe800); p.mark('suffix'); p.emit(0xe1866002); p.emit(0xe1c360b2)
    p.constant(6,LINK_SEED); p.memory(6,13,0x800,load=False)
    p.emit(0xe1a0e000); p.emit(0xe1a06008|((27 if cpu==9 else 28)<<7)); p.emit(0xe128f006)
    p.emit(0xe3836001); p.address(7,'returned'); p.emit(0xe12fff16)
    p.mark('returned'); p.emit(0xe10f5000); p.memory(6,13,0x80,load=False)
    # Restore the two patched halfwords before the next call or any output.
    p.memory(6,13,0x84); p.emit(0xe1c360b0); p.memory(6,13,0x88); p.emit(0xe1c360b2)
    p.puts('row')
    for reg in (4,5): p.emit(0xe1a06000|reg); p.branch('word',link=True)
    p.memory(6,13,0x80); p.branch('word',link=True)
    for reg in (0,1,2,3): p.emit(0xe1a06000|reg); p.branch('word',link=True)
    p.memory(6,13,0x800); p.branch('word',link=True)
    p.constant(6,10); p.memory(6,12,load=False,byte=True)
    p.emit(0xe2888001); p.emit(0xe3580000|(32 if cpu==9 else 16)); p.branch('flags',cond=1)
    p.emit(0xe28aa004); p.address(6,'values_end'); p.emit(0xe15a0006); p.branch('seed',cond=1)
    p.emit(0xe2899001); p.memory(6,11,16); p.emit(0xe1590006); p.branch('field',cond=1)
    p.emit(0xe28bb018); p.address(6,'descriptors_end'); p.emit(0xe15b0006); p.branch('group',cond=1)
    p.puts('complete')
    if cpu==9: p.constant(6,1); p.memory(6,13,load=False)
    p.constant(6,0); p.memory(6,12,4,load=False); p.mark('idle'); p.branch('idle')
    p.mark('puts'); p.memory(7,6,load=True,byte=True); p.emit(0xe3570000); p.emit(0x012fff1e)
    p.memory(7,12,load=False,byte=True); p.emit(0xe2866001); p.branch('puts')
    p.mark('word'); p.memory(6,12,8,load=False); p.emit(0xe12fff1e)
    p.mark('values')
    for value in seeds(full): p.emit(value)
    p.mark('values_end')
    if not full:
        for kind in (0,3):
            p.mark(f'fields_{kind}')
            for value in fields(Group(kind,0),False): p.emit(value)
    total=sum(cases(g,cpu,full) for g in domain)
    p.string('header',f'VELOCE 1 ds long_calls cpu={cpu}\nINFO full={int(full)} groups={len(domain)} flags={32 if cpu==9 else 16}\n')
    for g in domain: p.string(g.name+'_header',f'BEGIN cpu{cpu}_{g.name} cases={cases(g,cpu,full)} words=8\n')
    p.string('row','REG r='); p.string('complete',f'LOG cpu={cpu} complete cases={total}\n'+('END observations_complete\n' if cpu==7 else ''))
    while len(p.data)%4: p.data.append(0)
    p.mark('descriptors')
    for g in domain:
        for value in (p.labels[g.name+'_header'],g.kind,g.align,g.base,len(fields(g,full)),0 if full else p.labels[f'fields_{3 if g.kind==3 else 0}']): p.emit(value)
    p.mark('descriptors_end'); p.resolve(); return bytes(p.data)
def build(full=True,wrong_sign=False,wrong_link=False):
    main=bytearray(0x400000)
    thumb=(0x4674,0x4674,0x467e,0x4738) # two entry halfwords, capture LR/PC, return through r7
    arm=(0xe58de800,0xe28e4002 if wrong_link else 0xe1a0400e,0xe1a0600f,0xe12fff17)
    for page in range(0,0x400000,4096):
        for n,word in enumerate(thumb): main[page+0x204+n*2:page+0x206+n*2]=word.to_bytes(2,'little')
        for n,word in enumerate(arm): main[page+0x408+n*4:page+0x40c+n*4]=word.to_bytes(4,'little')
    for n,word in enumerate(thumb): main[0x3000+n*2:0x3002+n*2]=word.to_bytes(2,'little')
    for n,word in enumerate(arm): main[0x4000+n*4:0x4004+n*4]=word.to_bytes(4,'little')
    nine=program(9,full,wrong_sign); seven=program(7,full,wrong_sign)
    if len(nine)>0x800 or len(seven)>0x10000: raise ValueError('driver exceeds reserved code area')
    main[0x800:0x800+len(nine)]=nine
    offset7=512+len(main); rom=bytearray(offset7+len(seven)); rom[:12]=b'VELOCE CALL '; rom[12:16]=b'VCAL'; rom[0x180:0x188]=b'VDSHB001'
    for offset,items in ((0x20,(512,0x02000800,0x02000000,len(main))),(0x30,(offset7,0x03800000,0x03800000,len(seven)))):
        for n,value in enumerate(items): rom[offset+n*4:offset+n*4+4]=value.to_bytes(4,'little')
    rom[0x80:0x84]=len(rom).to_bytes(4,'little'); rom[0x84:0x88]=(512).to_bytes(4,'little')
    rom[512:offset7]=main; rom[offset7:]=seven; return bytes(rom)
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('long_calls.nds'))
    args=parser.parse_args(); rom=build(); args.output.write_bytes(rom); print(hashlib.sha256(rom).hexdigest(),args.output)
