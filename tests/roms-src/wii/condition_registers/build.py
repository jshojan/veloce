#!/usr/bin/env python3
"""Original PowerPC stimulus groups, guest instruction loops and register capture."""
import argparse
from pathlib import Path
CONSOLE='wii'
EXT='vwi'
VALUES=(0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,0xfffffffe,0xffffffff,0x55555555,0x89abcdef)
SEEDS=(0,0xffffffff,0xa5a5a5a5,0x12345678)
def groups(short=False):
    values=(0,0xffffffff) if short else VALUES
    fields=(0,7) if short else range(8)
    flags=(0,7) if short else range(8)
    seeds=SEEDS[:2] if short else SEEDS
    for xo in (0,32):
        for a in values:
            for b in values:
                for field in fields:
                    for flag in flags:
                        for seed in seeds: yield (31<<26|field<<23|3<<16|10<<11|xo<<1,a,b,seed,flag<<29|127,1,0)
    for op in (11,10):
        for a in values:
            for field in fields:
                for so in range(2):
                    seed=SEEDS[(field+so)%4]
                    yield (op<<26|field<<23|3<<16|(0xfff0 if short else 0),a,0x2468ace0,seed,so<<31|0x6000007f,16 if short else 65536,1)
    for xo in (257,129,289,225,33,449,417,193):
        for seed in seeds:
            for flag in flags: yield (19<<26|xo<<1,0x13579bdf,0x2468ace0,seed,flag<<29|127,16 if short else 32768,2048)
    for dst in fields:
        for src in fields:
            for seed in seeds:
                for flag in flags: yield (19<<26|dst<<23|src<<18,0x13579bdf,0x2468ace0,seed,flag<<29|127,1,0)
    for dst in fields:
        for flag in flags:
            for count in ((0,127) if short else range(128)):
                for seed in seeds: yield (31<<26|dst<<23|512<<1,0x13579bdf,0x2468ace0,seed,flag<<29|count,1,0)

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

def build(short=False,wrong_result=False,wrong_signed=False,wrong_clear=False):
    records=list(groups(short)); cases=sum(record[5] for record in records)
    p=Program(); p.const(12,0xfff00000); p.const(6,0x12345678)
    p.text(f'VELOCE 1 {CONSOLE} condition_registers\nBEGIN conditions cases={cases} words=8\n')
    p.address(14,'records'); p.address(15,'instruction'); p.const(19,len(records)); p.mark('group')
    for index,reg in enumerate((4,3,10,5,9,17,18)): p.emit(32<<26|reg<<21|14<<16|index*4)
    p.spr(17,9,True); p.mark('case'); p.emit(31<<26|5<<21|255<<12|144<<1); p.spr(9,1,True)
    if wrong_signed:
        p.emit(25<<26|4<<21|11<<16|0x400); p.emit(36<<26|11<<21|15<<16)
    else: p.emit(36<<26|4<<21|15<<16)
    p.mark('instruction'); p.emit(0)
    if wrong_clear: p.spr(9,1,True)
    p.emit(31<<26|7<<21|19<<1); p.spr(8,1)
    if wrong_result: p.emit(14<<26|7<<21|7<<16|1)
    p.text('REG r=')
    for reg in (7,8,3,10,4,5,9,6): p.emit(36<<26|reg<<21|12<<16|8)
    p.text('\n'); p.emit(31<<26|4<<21|4<<16|18<<11|266<<1); p.branch('case',16)
    p.emit(14<<26|14<<21|14<<16|28); p.emit(14<<26|19<<21|19<<16|65535)
    p.emit(11<<26|7<<23|19<<16); p.branch('group',4,30)
    p.text(f'LOG complete cases={cases}\nEND observations_complete\n'); p.emit(36<<26|12<<16|4); p.emit(18<<26)
    p.mark('records')
    for record in records:
        for word in record: p.emit(word)
    p.resolve(); header=bytearray(b'VWIP0001')
    for value in (0x80000000,0x80000000,len(p.data),0,0,0): header.extend(value.to_bytes(4,'big'))
    return bytes(header+p.data)
if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('condition_registers.'+EXT))
    args=parser.parse_args(); args.output.parent.mkdir(parents=True,exist_ok=True); args.output.write_bytes(build())
