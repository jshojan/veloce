#!/usr/bin/env python3
"""Original integer memory instructions, stimuli and register capture only."""
import argparse
from pathlib import Path
CONSOLE='wii'
EXT='vwi'
VALUES=(0,1,0x7fff8000,0x8000ffff,0xffffffff,0x89abcdef,0x5555aaaa,0x12345678)
FORMS=((32, 4, False), (33, 4, False), (34, 1, False), (35, 1, False), (36, 4, False), (37, 4, False), (38, 1, False), (39, 1, False), (40, 2, False), (41, 2, False), (42, 2, False), (43, 2, False), (44, 2, False), (45, 2, False), (23, 4, True), (55, 4, True), (87, 1, True), (119, 1, True), (151, 4, True), (183, 4, True), (215, 1, True), (247, 1, True), (279, 2, True), (311, 2, True), (343, 2, True), (375, 2, True), (407, 2, True), (439, 2, True), (534, 4, True), (790, 2, True), (662, 4, True), (918, 2, True))
def groups(short=False):
    values=VALUES[:2]+VALUES[4:6] if short else VALUES
    for fi,(code,width,indexed) in enumerate(FORMS):
        for pi,source in enumerate(values):
            for bank,alias in enumerate((0,0x80000000,0xc0000000)):
                indexes=(0,-4,4,-32768,32764,-2147483648,-width,2147483644) if indexed else (-32768,)
                if short: indexes=(0,4) if indexed else (-8,)
                for ii,index in enumerate(indexes):
                    instruction=(31<<26|11<<21|3<<16|10<<11|code<<1) if indexed else (code<<26|11<<21|3<<16)
                    memory=((source<<8)|(source>>24))&0xffffffff; memory^=0x89abcdef
                    count=1 if indexed else 16//width if short else 65536//width
                    cr=(0,0xffffffff,0xa5a5a5a5,0x12345678)[(fi+pi+bank+ii)%4]
                    xer=((fi+pi+bank+ii)%8)<<29|127
                    yield (instruction,source,memory,cr,xer,alias+0x100000,index&0xffffffff,count,width,int(not indexed))

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

def build(short=False,wrong_result=False,wrong_sign=False,wrong_update=False):
    records=list(groups(short)); cases=sum(r[7] for r in records)
    p=Program(); p.const(12,0xfff00000); p.const(14,0x13579bdf); p.const(15,0x2468ace0)
    p.text(f'VELOCE 1 {CONSOLE} memory_registers\nBEGIN transfers cases={cases} words=10\n')
    p.address(28,'records'); p.address(30,'instruction'); p.const(29,len(records)); p.mark('group')
    for n,r in enumerate((4,26,13,5,9,22,10,21,23,24)): p.emit(32<<26|r<<21|28<<16|n*4)
    p.spr(21,9,True); p.mark('case')
    p.emit(36<<26|13<<21|22<<16); p.emit(36<<26|14<<21|22<<16|0xfffc); p.emit(36<<26|15<<21|22<<16|4)
    p.emit(24<<26|26<<21|11<<16) # Restore old RT for stores and load preservation.
    p.emit(31<<26|3<<21|10<<16|22<<11|40<<1) # RA = target - signed displacement/index.
    p.emit(24<<26|3<<21|27<<16)
    p.emit(11<<26|24<<16|0); p.branch('indexed',12,2)
    p.emit(21<<26|10<<21|25<<16|16<<6|31<<1); p.emit(31<<26|4<<21|25<<16|25<<11|444<<1); p.branch('patch')
    p.mark('indexed'); p.emit(24<<26|4<<21|25<<16); p.mark('patch')
    # The sign mutant replaces only algebraic halfword loads, preserving captured encoding.
    p.emit(24<<26|25<<21|31<<16)
    if wrong_sign:
        p.emit(21<<26|25<<21|20<<16|6<<11|26<<6|31<<1) # primary opcode
        p.emit(11<<26|20<<16|42); p.branch('sign-d',12,2)
        p.emit(11<<26|20<<16|43); p.branch('sign-d',12,2)
        p.emit(11<<26|20<<16|31); p.branch('sign-done',4,2)
        p.emit(21<<26|25<<21|20<<16|31<<11|22<<6|31<<1) # XO bits
        p.emit(11<<26|20<<16|343); p.branch('sign-x',12,2)
        p.emit(11<<26|20<<16|375); p.branch('sign-done',4,2)
        p.mark('sign-x'); p.const(20,0x80); p.branch('sign-flip')
        p.mark('sign-d'); p.const(20,2<<26)
        p.mark('sign-flip'); p.emit(31<<26|31<<21|31<<16|20<<11|316<<1)
        p.mark('sign-done')
    p.emit(36<<26|31<<21|30<<16)
    p.emit(31<<26|5<<21|255<<12|144<<1); p.spr(9,1,True)
    p.mark('instruction'); p.emit(0)
    if wrong_update: p.emit(24<<26|27<<21|3<<16)
    p.emit(31<<26|7<<21|19<<1); p.spr(8,1)
    if wrong_result: p.emit(14<<26|11<<21|11<<16|1)
    p.emit(32<<26|16<<21|22<<16); p.emit(32<<26|17<<21|22<<16|0xfffc); p.emit(32<<26|18<<21|22<<16|4)
    p.text('REG r=')
    for r in (11,3,10,7,8,16,17,18,25,26): p.emit(36<<26|r<<21|12<<16|8)
    p.text('\n'); p.emit(31<<26|10<<21|10<<16|23<<11|266<<1); p.branch('case',16)
    p.emit(14<<26|28<<21|28<<16|40); p.emit(14<<26|29<<21|29<<16|0xffff)
    p.emit(11<<26|29<<16); p.branch('group',4,2)
    p.text(f'LOG complete cases={cases}\nEND observations_complete\n'); p.emit(36<<26|12<<16|4); p.emit(18<<26)
    p.mark('records')
    for r in records:
        for word in r: p.emit(word)
    p.resolve(); header=bytearray(b'VWIP0001')
    for value in (0x80000000,0x80000000,len(p.data),0,0,0): header.extend(value.to_bytes(4,'big'))
    return bytes(header+p.data)
if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('memory_registers.'+EXT))
    args=parser.parse_args(); args.output.parent.mkdir(parents=True,exist_ok=True); args.output.write_bytes(build())
