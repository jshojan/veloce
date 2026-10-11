#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original AArch64 single-register transfers, actual register/RAM captures."""
import argparse,importlib.util,struct
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('original_assembler',HERE.parent/'ccmp_registers/build.py');asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
Program=asm.Program
KINDS=tuple((size,op) for size in range(4) for op in range(4) if op<2 or (op==2 and size<3) or (op==3 and size<2))
def groups(short=False):
    for scale,op in KINDS:
        for mode in range(4):
            for imm in ((0,1,255,256,511) if short else range(512)):
                yield scale,op,mode,imm,31 if imm&1 else 8,15 if imm&2 else 0,0,0,False
        for imm in ((0,1,4095) if short else (0,1,2,7,255,256,2048,4095)):
            for base in (8,31):
                for flags in (0,15):yield scale,op,4,imm,base,flags,0,0,False
        for option in (2,3,6,7):
            for scaled in (False,True):
                for index in ((0,1) if short else (0,1,255,0xffffffff00000001,0xffffffffffffffff)):
                    for flags in (0,15):yield scale,op,5,0,8,flags,index,option,scaled
def encode(scale,op,mode,imm,base,option=0,scaled=False):
    word=0x38000000|(scale<<30)|(op<<22)|(base<<5)
    if mode==4:return word|0x01000000|(imm<<10)
    if mode==5:return word|0x00200800|(11<<16)|(option<<13)|(int(scaled)<<12)
    return word|(imm<<12)|(mode<<10)
def constant(p,r,value):
    value%=2**64;p.mov(r,value)
    for shift in (16,32,48):
        if value>>shift&65535:p.emit(0xf2800000|(shift//16)<<21|((value>>shift)&65535)<<5|r)
def displacement(scale,mode,imm,index,option,scaled):
    if mode==4:return imm*(1<<scale)
    if mode!=5:return imm if imm<256 else imm-512
    value=index if option&1 else index&0xffffffff
    if option==6 and value&0x80000000:value-=2**32
    return (value<<(scale if scaled else 0))%2**64
# Large unsigned register offsets use modulo address formation to a mapped target.
def build(short=False,wrong_result=False,wrong_flags=False,wrong_offset=False,wrong_sign=False,wrong_store=False):
    p=Program();values=(0,127,128,255) if short else tuple(range(256));count=len(values)*len(tuple(groups(short)))
    p.mov(28,0xffff,16);p.put('header');p.adr(21,'table');p.mov(19,len(values));p.mark('value_loop');p.emit(0xf94002aa)
    for scale,op,mode,imm,base,flags,index,option,scaled in groups(short):
        size=1<<scale;offset=displacement(scale,mode,imm,index,option,scaled)
        origin=(0x3c0800-offset)%2**64 if mode==5 else 0x3c0800
        target=0x3c0800 if mode==5 else origin+(0 if mode==1 else offset)
        constant(p,12,origin);constant(p,9,target);constant(p,11,index);p.emit(0xaa0c03e8)
        if base==31:p.emit(0x9100011f)
        # Complement guards make off-by-one addresses observable even for repeated-byte loads.
        p.emit(0xaa2a03e4);p.emit(0xd100212e);p.emit(0xf90001c4)
        p.emit(0x9100012e|(size<<10));p.emit(0xf90001c4)
        # Loads read real seed bytes; stores start from the complement.
        if op:p.emit((0x39000000|(scale<<30))|(9<<5)|10);p.mov(0,0xffff,48)
        else:p.emit(0xaa2a03e4);p.emit((0x39000000|(scale<<30))|(9<<5)|4);p.emit(0xaa0a03e0)
        word=encode(scale,op,mode,imm,base,option,scaled);actual=word
        if wrong_offset and mode!=5:actual^=1<<(10 if mode==4 else 12)
        if wrong_sign and op>=2:actual=(actual&~(3<<22))|(1<<22)
        constant(p,13,word);p.mov(7,flags<<12,16);p.emit(0xd51b4207)
        p.emit(0xd503201f if wrong_store and op==0 else actual)
        p.emit(0xd53b4206);p.emit(0x910003e2 if base==31 else 0xaa0803e2)
        p.emit((0x39400000|(scale<<30))|(9<<5)|3)
        if wrong_result:p.mov(5,1);p.emit(0xca050000)
        if wrong_flags:p.mov(5,0x1000,16);p.emit(0xca0500c6)
        p.emit(0xb900139f)
        for reg in (0,2,3,6,10,13,11,12):p.emit(0xf9000780|reg)
        p.mov(5,10);p.emit(0x39000385)
    p.emit(0x910022b5);p.emit(0xf1000673);p.branch('values_done',cond=0);p.branch('value_loop');p.mark('values_done')
    p.put('complete');p.emit(0xb900079f);p.mark('halt');p.branch('halt')
    p.mark('puts');p.emit(0x39400085);p.cbz(5,'return');p.emit(0x39000385);p.emit(0x91000484);p.branch('puts');p.mark('return');p.emit(0xd65f03c0)
    ro=bytearray()
    for label,text in (('header',f'VELOCE 1 switch single_transfer_registers\nBEGIN singles cases={count} words=8\n'),('complete',f'LOG complete cases={count}\nEND observations_complete\n')):
        p.labels[label]=0x3b0000+len(ro);ro.extend(text.encode()+b'\0')
    while len(ro)%8:ro.append(0)
    p.labels['table']=0x3b0000+len(ro)
    for n in values:ro.extend((n*0x0101010101010101).to_bytes(8,'little'))
    p.resolve();assert len(p.code)<0x3b0000,len(p.code)
    header=bytearray(256);header[:4]=b'NSO0'
    for n,(offset,address,data) in enumerate(((256,0,p.code),(256+len(p.code),0x3b0000,ro),(256+len(p.code)+len(ro),0x3c0000,b'\0'*16))):struct.pack_into('<III',header,16+n*16,offset,address,len(data))
    struct.pack_into('<I',header,60,65536)
    return bytes(header+p.code+ro+b'\0'*16)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=HERE/'single_transfer_registers.nso');args=parser.parse_args();args.output.write_bytes(build())
