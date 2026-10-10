#!/usr/bin/env python3
"""Original three-context Espresso ADDI/ADDIS stimuli and register captures."""
import argparse
from pathlib import Path
VALUES=(0,1,0x7fffffff,0x80000000)
SEEDS=(0,0xffffffff,0xa5a5a5a5,0x12345678)
class Program:
    def __init__(self,base):self.base=base;self.data=bytearray();self.labels={};self.fixups=[]
    @property
    def pc(self):return self.base+len(self.data)
    def emit(self,word):self.data.extend(word.to_bytes(4,'big'))
    def mark(self,name):
        if name in self.labels:raise ValueError('duplicate label')
        self.labels[name]=self.pc
    def const(self,reg,value):
        self.emit(15<<26|reg<<21|((value>>16)&65535));self.emit(24<<26|reg<<21|reg<<16|(value&65535))
    def address(self,reg,label):self.fixups.append((len(self.data),label,'address',reg));self.const(reg,0)
    def branch(self,label,bo=None,bi=0):self.fixups.append((len(self.data),label,'branch',bo,bi));self.emit(0)
    def spr(self,reg,number,write=False):self.emit(31<<26|reg<<21|(number&31)<<16|(number&992)<<6|(467 if write else 339)<<1)
    def text(self,value):
        for char in value.encode('ascii'):self.emit(14<<26|char);self.emit(38<<26|12<<16)
    def resolve(self):
        for offset,label,kind,*args in self.fixups:
            target=self.labels[label]
            if kind=='address':
                reg=args[0];words=(15<<26|reg<<21|((target>>16)&65535),24<<26|reg<<21|reg<<16|(target&65535))
            else:
                bo,bi=args;delta=target-(self.base+offset)
                if delta%4 or not -(32768 if bo is not None else 1<<25)<=delta<(32768 if bo is not None else 1<<25):raise ValueError('branch range')
                words=((16<<26|bo<<21|bi<<16|(delta&0xfffc)) if bo is not None else (18<<26|(delta&0x03fffffc)),)
            for index,word in enumerate(words):self.data[offset+4*index:offset+4*index+4]=word.to_bytes(4,'big')
def program(core,short=False,wrong_result=False,wrong_width=False,wrong_flags=False):
    p=Program(0x10000000+core*0x10000);p.const(12,0xffe00000);p.const(6,0x12345678);p.const(10,core)
    p.const(22,0x10008000)
    if core:
        p.mark('wait');p.emit(32<<26|7<<21|22<<16);p.emit(11<<26|7<<23|7<<16|core);p.branch('wait',4,30)
    else:p.text('VELOCE 1 wiiu immediate_registers\n')
    count=2*4*2*8*(8 if short else 65536)
    p.text(f'BEGIN cpu{core} cases={count} words=9\n');p.address(14,'records');p.address(15,'instruction');p.const(19,128);p.mark('group')
    for index,reg in enumerate((4,3,5,9)):p.emit(32<<26|reg<<21|14<<16|index*4)
    p.const(17,8 if short else 65536);p.spr(17,9,True);p.const(18,0);p.mark('case')
    # The dynamic instruction is the actual captured encoding; short values are table driven.
    if short:
        p.address(20,'values');p.emit(21<<26|18<<21|21<<16|2<<11|0<<6|29<<1);p.emit(31<<26|21<<21|20<<16|21<<11|23<<1)
        p.emit(31<<26|4<<21|16<<16|21<<11|444<<1)
    else:p.emit(31<<26|4<<21|16<<16|18<<11|444<<1)
    p.emit(31<<26|5<<21|255<<12|144<<1);p.spr(9,1,True);p.emit(36<<26|16<<21|15<<16)
    p.mark('instruction');p.emit(0)
    if wrong_result:p.emit(14<<26|11<<21|11<<16|1)
    if wrong_flags:p.const(7,0);p.emit(31<<26|7<<21|255<<12|144<<1)
    p.emit(31<<26|7<<21|19<<1);p.spr(8,1)
    p.text('REG r=')
    for reg in (11,7,8,3,16,5,9,6,10):p.emit(36<<26|reg<<21|12<<16|8)
    p.text('\n');p.emit(14<<26|18<<21|18<<16|1);p.branch('case',16)
    p.emit(14<<26|14<<21|14<<16|16);p.emit(14<<26|19<<21|19<<16|65535);p.emit(11<<26|7<<23|19<<16);p.branch('group',4,30)
    p.text(f'BLOCK complete cpu{core}\n')
    if core<2:p.const(7,core+1);p.emit(36<<26|7<<21|22<<16)
    else:p.text(f'LOG complete cases={count*3}\nEND observations_complete\n')
    p.emit(36<<26|12<<16|4);p.emit(18<<26)
    p.mark('values')
    for value in (0,1,127,128,255,32767,32768,65535):p.emit(value)
    p.mark('records')
    for op in (14,15):
        for ai,a in enumerate(VALUES):
            for ra in (0,3):
                for flags in range(8):
                    actual_op=14 if wrong_width and op==15 else op
                    for word in (actual_op<<26|11<<21|ra<<16,a,SEEDS[(ai+ra+flags)%4],flags<<29|127):p.emit(word)
    p.resolve()
    if len(p.data)>=0x8000:raise ValueError('guest overlaps synchronization')
    return bytes(p.data)
def build(short=False,wrong_result=False,wrong_width=False,wrong_flags=False):
    rom=bytearray(64);rom[:8]=b'VWUP0001'
    for core in range(3):
        code=program(core,short,wrong_result,wrong_width,wrong_flags);address=0x10000000+core*0x10000
        for n,value in enumerate((len(rom),address,address,len(code))):rom[16+core*16+n*4:20+core*16+n*4]=value.to_bytes(4,'big')
        rom.extend(code)
    return bytes(rom)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('immediate_registers.vwiiu'))
    args=parser.parse_args();args.output.write_bytes(build())
