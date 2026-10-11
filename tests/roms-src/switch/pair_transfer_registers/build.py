#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original AArch64 integer paired-transfer NSO0; no SDK or firmware."""
from pathlib import Path
import argparse,importlib.util,struct
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('original_assembler',HERE.parent/'ccmp_registers/build.py');asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
Program=asm.Program
VALUES=tuple(n*0x0101010101010101 for n in range(256))
def groups(short=False):
    for kind in range(5):
        for mode in range(4):
            if kind==4 and mode==0:continue
            for immediate in ((0,1,63,64,127) if short else range(128)):
                for base in (8,31):
                    for flags in (0,15):yield kind,mode,immediate,base,flags
def encoding(kind,mode,immediate,base):
    opc=1 if kind==4 else 2 if kind%2 else 0
    return 0x28000000|(opc<<30)|((kind>=2)<<22)|(mode<<23)|(immediate<<15)|(1<<10)|(base<<5)
def constant(p,r,value):
    p.mov(r,value)
    for shift in (16,32,48):
        if value>>shift&65535:p.emit(0xf2800000|(shift//16)<<21|((value>>shift)&65535)<<5|r)
def build(short=False,wrong_result=False,wrong_flags=False,wrong_offset=False,wrong_sign=False,wrong_store=False):
    p=Program();values=(0,127,128,255) if short else tuple(range(256));count=len(values)*len(tuple(groups(short)))
    p.mov(28,0xffff,16);p.put('header');p.adr(21,'table');p.mov(19,len(values));p.mark('value_loop')
    p.emit(0xf94002aa);p.emit(0xaa2a03eb) # X10=seed, X11=~seed.
    for kind,mode,immediate,base,flags in groups(short):
        size=8 if kind in (1,3) else 4;displacement=(immediate if immediate<64 else immediate-128)*size
        constant(p,12,0x300800);constant(p,9,0x300800+(0 if mode==1 else displacement))
        p.emit(0xaa0c03e8)
        if base==31:p.emit(0x9100011f) # ADD SP,X8,#0.
        # Ordinary stores seed RAM before the tested transfer.
        seed0,seed1=(10,11) if kind>=2 else (11,10)
        p.emit((0xf9000000 if size==8 else 0xb9000000)|(9<<5)|seed0)
        p.emit((0xf9000400 if size==8 else 0xb9000400)|(9<<5)|seed1)
        if kind>=2:constant(p,0,0x123456789abcdef0);constant(p,1,0xfedcba9876543210)
        else:p.emit(0xaa0a03e0);p.emit(0xaa0b03e1)
        word=encoding(kind,mode,immediate,base);actual=word
        if wrong_offset:actual^=1<<15
        if wrong_sign and kind==4:actual^=1<<30
        constant(p,13,word);constant(p,7,flags<<28);p.emit(0xd51b4207)
        p.emit(0xd503201f if wrong_store and kind<2 else actual)
        p.emit(0xd53b4206);p.emit(0x910003e2 if base==31 else 0xaa0803e2)
        p.emit((0xf9400000 if size==8 else 0xb9400000)|(9<<5)|3)
        p.emit((0xf9400400 if size==8 else 0xb9400400)|(9<<5)|4)
        if wrong_result:p.emit(0xd2800025);p.emit(0xca050000)
        if wrong_flags:p.emit(0xd2a20005);p.emit(0xca0500c6) # Flip actual NZCV N bit in capture.
        p.emit(0xb900139f)
        for reg in (0,1,2,3,4,6,10,11,13,12):p.emit(0xf9000780|reg)
        p.mov(5,10);p.emit(0x39000385)
    p.emit(0x910022b5);p.emit(0xf1000673);p.branch('values_done',cond=0);p.branch('value_loop');p.mark('values_done')
    p.put('complete');p.emit(0xb900079f);p.mark('halt');p.branch('halt')
    p.mark('puts');p.emit(0x39400085);p.cbz(5,'return');p.emit(0x39000385);p.emit(0x91000484);p.branch('puts');p.mark('return');p.emit(0xd65f03c0)
    ro=bytearray()
    for label,text in (('header',f'VELOCE 1 switch pair_transfer_registers\nBEGIN pairs cases={count} words=10\n'),('complete',f'LOG complete cases={count}\nEND observations_complete\n')):
        p.labels[label]=0x200000+len(ro);ro.extend(text.encode()+b'\0')
    while len(ro)%8:ro.append(0)
    p.labels['table']=0x200000+len(ro)
    for n in values:ro.extend(VALUES[n].to_bytes(8,'little'))
    p.resolve();assert len(p.code)<0x200000
    header=bytearray(256);header[:4]=b'NSO0'
    for n,(offset,address,data) in enumerate(((256,0,p.code),(256+len(p.code),0x200000,ro),(256+len(p.code)+len(ro),0x300000,b'\0'*16))):struct.pack_into('<III',header,16+n*16,offset,address,len(data))
    struct.pack_into('<I',header,60,8192)
    return bytes(header+p.code+ro+b'\0'*16)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=HERE/'pair_transfer_registers.nso');args=parser.parse_args();args.output.write_bytes(build())
