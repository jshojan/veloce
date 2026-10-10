#!/usr/bin/env python3
"""Original shift/unary stimuli, guest loops and register capture."""
import argparse
from pathlib import Path
CONSOLE='gc'
EXT='vgc'
VALUES=(0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,0xfffffffe,0xffffffff,0x55555555,0x89abcdef)
HIGH=(0,0x40,0x80000000,0xffffffc0)
BYTE_HIGH=(0,0x12345600,0x80000000,0xffffff00)
HALF_HIGH=(0,0x12340000,0x80000000,0xffff0000)
SEEDS=(0,0xffffffff,0xa5a5a5a5,0x12345678)
def groups(short=False):
    values=(0x80000001,0xffffffff) if short else VALUES
    flags=(0,7) if short else range(8)
    for xo in (24,536,792):
        for ai,a in enumerate(values):
            for hi,h in enumerate(HIGH[:2] if short else HIGH):
                for flag in flags:
                    for rc in range(2):
                        yield (31<<26|3<<21|11<<16|10<<11|xo<<1|rc,a,h,SEEDS[(ai+hi+flag+rc)%4],flag<<29|127,64,0,0,1)
    for ai,a in enumerate(values):
        for flag in flags:
            for rc in range(2): yield (31<<26|3<<21|11<<16|824<<1|rc,a,0x2468ace0,SEEDS[(ai+flag+rc)%4],flag<<29|127,32,2048,0,0)
    for xo,uppers in ((954,BYTE_HIGH),(922,HALF_HIGH)):
        for hi,h in enumerate(uppers[:2] if short else uppers):
            for flag in flags:
                for rc in range(2):
                    start=(0x70 if xo==954 else 0x7ff0) if short else 0
                    yield (31<<26|3<<21|11<<16|xo<<1|rc,h+start,0x2468ace0,SEEDS[(hi+flag+rc)%4],flag<<29|127,32 if short else (256 if xo==954 else 65536),0,1,0)
    patterns=(0,1,0x80000000,0xffffffff) if short else (0,)+tuple(1<<bit for bit in range(32))+tuple((1<<(bit+1))-1 for bit in range(32))
    for index,a in enumerate(patterns):
        for flag in flags:
            for rc in range(2): yield (31<<26|3<<21|11<<16|26<<1|rc,a,0x2468ace0,SEEDS[(index+flag+rc)%4],flag<<29|127,1,0,0,0)

class Program:
    def __init__(self): self.data=bytearray(); self.labels={}; self.fixups=[]
    @property
    def pc(self): return 0x80000000+len(self.data)
    def emit(self,word): self.data.extend(word.to_bytes(4,'big'))
    def mark(self,name):
        if name in self.labels: raise ValueError('duplicate label')
        self.labels[name]=self.pc
    def const(self,reg,value):
        self.emit(15<<26|reg<<21|((value>>16)&65535)); self.emit(24<<26|reg<<21|reg<<16|(value&65535))
    def address(self,reg,label): self.fixups.append((len(self.data),label,'address',reg)); self.const(reg,0)
    def branch(self,label,bo=None,bi=0):
        self.fixups.append((len(self.data),label,'branch',bo,bi)); self.emit(0)
    def spr(self,reg,number,write=False): self.emit(31<<26|reg<<21|(number&31)<<16|(number&992)<<6|(467 if write else 339)<<1)
    def text(self,value):
        for char in value.encode('ascii'): self.emit(14<<26|char); self.emit(38<<26|12<<16)
    def resolve(self):
        for offset,label,kind,*args in self.fixups:
            target=self.labels[label]
            if kind=='address':
                reg=args[0]; words=(15<<26|reg<<21|((target>>16)&65535),24<<26|reg<<21|reg<<16|(target&65535))
            else:
                bo,bi=args; delta=target-(0x80000000+offset)
                if delta%4 or not -(32768 if bo is not None else 1<<25)<=delta<(32768 if bo is not None else 1<<25): raise ValueError('branch range')
                words=((16<<26|bo<<21|bi<<16|(delta&0xfffc)) if bo is not None else (18<<26|(delta&0x03fffffc)),)
            for index,word in enumerate(words): self.data[offset+4*index:offset+4*index+4]=word.to_bytes(4,'big')

def build(short=False,wrong_result=False,wrong_carry=False,wrong_count=False):
    records=list(groups(short));
    if wrong_count: records=[r for r in records if ((r[0]>>1)&1023) in (24,536,792)]
    cases=sum(record[5] for record in records)
    p=Program(); p.const(12,0xfff00000); p.const(6,0x12345678)
    p.text(f'VELOCE 1 {CONSOLE} shift_registers\nBEGIN shifts cases={cases} words=9\n')
    p.address(14,'records'); p.address(15,'instruction'); p.const(19,len(records)); p.mark('group')
    for index,reg in enumerate((4,3,10,5,9,17,18,23,22)): p.emit(32<<26|reg<<21|14<<16|index*4)
    p.spr(17,9,True); p.mark('case'); p.emit(31<<26|5<<21|255<<12|144<<1); p.spr(9,1,True)
    if wrong_count:
        # Mask the actual RB value to five bits; preserve the captured stimulus B.
        p.emit(21<<26|10<<21|21<<16|27<<6|31<<1)
        p.emit(26<<26|4<<21|20<<16|(31<<11)); p.emit(36<<26|20<<21|15<<16)
        # Only register shift instructions use RB=10, selected below before image generation.
    else: p.emit(36<<26|4<<21|15<<16)
    p.mark('instruction'); p.emit(0)
    if wrong_carry: p.spr(9,1,True)
    p.emit(31<<26|7<<21|19<<1); p.spr(8,1)
    if wrong_result: p.emit(14<<26|11<<21|11<<16|1)
    p.text('REG r=')
    for reg in (11,7,8,3,10,4,5,9,6): p.emit(36<<26|reg<<21|12<<16|8)
    p.text('\n'); p.emit(31<<26|4<<21|4<<16|18<<11|266<<1)
    p.emit(31<<26|3<<21|3<<16|23<<11|266<<1); p.emit(31<<26|10<<21|10<<16|22<<11|266<<1); p.branch('case',16)
    p.emit(14<<26|14<<21|14<<16|36); p.emit(14<<26|19<<21|19<<16|65535)
    p.emit(11<<26|7<<23|19<<16); p.branch('group',4,30)
    p.text(f'LOG complete cases={cases}\nEND observations_complete\n'); p.emit(36<<26|12<<16|4); p.emit(18<<26)
    p.mark('records')
    for record in records:
        for word in record: p.emit(word)
    p.resolve(); header=bytearray(b'VGCP0001')
    for value in (0x80000000,0x80000000,len(p.data),0,0,0): header.extend(value.to_bytes(4,'big'))
    return bytes(header+p.data)
if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('shift_registers.'+EXT))
    args=parser.parse_args(); args.output.parent.mkdir(parents=True,exist_ok=True); args.output.write_bytes(build())
