#!/usr/bin/env python3
"""Original carry arithmetic stimuli and guest observations, without expected answers."""
import argparse
from pathlib import Path
CONSOLE='wii'
EXT='vwi'
VALUES=(0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,0xfffffffe,0xffffffff,0x55555555,0x89abcdef)
SEEDS=(0,0xffffffff,0xa5a5a5a5,0x12345678)
def groups(short=False):
    values=(0xffffffff,0x80000000) if short else VALUES
    flags=(0,7) if short else range(8)
    for xo in (104,202,234,200,232):
        for ai,a in enumerate(values):
            for flag in flags:
                for oe in range(2):
                    for rc in range(2):
                        yield (31<<26|11<<21|3<<16|oe<<10|xo<<1|rc,a,0x2468ace0,SEEDS[(ai+flag+oe+rc)%4],flag<<29|127,1,0)
    for op in (12,13,8):
        for ai,a in enumerate(values):
            for flag in flags:
                yield (op<<26|11<<21|3<<16|(0xfff0 if short else 0),a,0x2468ace0,SEEDS[(ai+flag)%4],flag<<29|127,16 if short else 65536,1)

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

def build(short=False,wrong_result=False,wrong_carry=False,wrong_so=False):
    records=list(groups(short)); cases=sum(record[5] for record in records)
    p=Program(); p.const(12,0xfff00000); p.const(6,0x12345678)
    if wrong_carry or wrong_so: p.const(20,0x20000000 if wrong_carry else 0x80000000)
    p.text(f'VELOCE 1 {CONSOLE} carry_registers\nBEGIN carry cases={cases} words=9\n')
    p.address(14,'records'); p.address(15,'instruction'); p.const(19,len(records)); p.mark('group')
    for index,reg in enumerate((4,3,10,5,9,17,18)): p.emit(32<<26|reg<<21|14<<16|index*4)
    p.spr(17,9,True); p.mark('case'); p.emit(31<<26|5<<21|255<<12|144<<1); p.spr(9,1,True)
    p.emit(36<<26|4<<21|15<<16); p.mark('instruction'); p.emit(0)
    if wrong_carry or wrong_so:
        p.spr(8,1); p.emit(31<<26|8<<21|8<<16|20<<11|60<<1)
        if wrong_carry:
            p.emit(31<<26|9<<21|21<<16|20<<11|28<<1); p.emit(31<<26|8<<21|8<<16|21<<11|444<<1)
        p.spr(8,1,True)
    p.emit(31<<26|7<<21|19<<1); p.spr(8,1)
    if wrong_result: p.emit(14<<26|11<<21|11<<16|1)
    p.text('REG r=')
    for reg in (11,7,8,3,10,4,5,9,6): p.emit(36<<26|reg<<21|12<<16|8)
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
    parser=argparse.ArgumentParser(); parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('carry_registers.'+EXT))
    args=parser.parse_args(); args.output.parent.mkdir(parents=True,exist_ok=True); args.output.write_bytes(build())
