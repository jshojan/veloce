#!/usr/bin/env python3
"""Original word rotate/mask stimuli and guest capture, without expected answers."""
import argparse
from pathlib import Path
CONSOLE='wii'
EXT='vwi'
SOURCES=(0,1,0x7fffffff,0x80000000,0xffffffff,0x55555555,0xaaaaaaaa,0x89abcdef)
DESTINATIONS=(0,0xffffffff,0x12345678,0xa5a5a5a5)
HIGH=(0,0x20,0x80000000,0xffffffe0)
SEEDS=(0,0xffffffff,0xa5a5a5a5,0x12345678)
def groups(short=False):
    sources=(0x80000001,0x89abcdef) if short else SOURCES
    for op in (20,21):
        for ai,a in enumerate(sources):
            for di,d in enumerate((DESTINATIONS[:2] if short else DESTINATIONS) if op==20 else DESTINATIONS[:1]):
                for so in range(2):
                    for rc in range(2):
                        yield (op<<26|3<<21|11<<16|(31<<11 if short else 0)|rc,a,0x2468ace0,d,SEEDS[(ai+di+rc+so)%4],so<<31|0x6000007f,64 if short else 32768,2)
    for ai,a in enumerate(sources):
        for shift in ((0,31) if short else range(32)):
            for hi in (HIGH[:2] if short else HIGH):
                for so in range(2):
                    for rc in range(2):
                        yield (23<<26|3<<21|11<<16|10<<11|rc,a,hi+shift,0,SEEDS[(ai+rc+so)%4],so<<31|0x6000007f,64 if short else 1024,2)

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

def build(short=False,wrong_result=False,wrong_mask=False,wrong_insert=False):
    records=list(groups(short)); cases=sum(record[6] for record in records)
    p=Program(); p.const(12,0xfff00000); p.const(6,0x12345678)
    p.text(f'VELOCE 1 {CONSOLE} rotate_registers\nBEGIN rotates cases={cases} words=10\n')
    p.address(14,'records'); p.address(15,'instruction'); p.const(19,len(records)); p.mark('group')
    for index,reg in enumerate((4,3,10,16,5,9,17,18)): p.emit(32<<26|reg<<21|14<<16|index*4)
    p.spr(17,9,True); p.mark('case'); p.emit(31<<26|5<<21|255<<12|144<<1); p.spr(9,1,True)
    p.emit(14<<26|11<<21|16<<16)
    if wrong_insert: p.emit(14<<26|11<<21)
    if wrong_mask:
        p.emit(26<<26|4<<21|21<<16|2); p.emit(36<<26|21<<21|15<<16)
    else: p.emit(36<<26|4<<21|15<<16)
    p.mark('instruction'); p.emit(0)
    p.emit(31<<26|7<<21|19<<1); p.spr(8,1)
    if wrong_result: p.emit(14<<26|11<<21|11<<16|1)
    p.text('REG r=')
    for reg in (11,7,8,3,10,16,4,5,9,6): p.emit(36<<26|reg<<21|12<<16|8)
    p.text('\n'); p.emit(31<<26|4<<21|4<<16|18<<11|266<<1); p.branch('case',16)
    p.emit(14<<26|14<<21|14<<16|32); p.emit(14<<26|19<<21|19<<16|65535)
    p.emit(11<<26|7<<23|19<<16); p.branch('group',4,30)
    p.text(f'LOG complete cases={cases}\nEND observations_complete\n'); p.emit(36<<26|12<<16|4); p.emit(18<<26)
    p.mark('records')
    for record in records:
        for word in record: p.emit(word)
    p.resolve(); header=bytearray(b'VWIP0001')
    for value in (0x80000000,0x80000000,len(p.data),0,0,0): header.extend(value.to_bytes(4,'big'))
    return bytes(header+p.data)
if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('rotate_registers.'+EXT))
    args=parser.parse_args(); args.output.parent.mkdir(parents=True,exist_ok=True); args.output.write_bytes(build())
