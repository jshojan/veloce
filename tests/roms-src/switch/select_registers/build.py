#!/usr/bin/env python3
"""Original MIT AArch64 conditional select NSO0; no SDK or firmware."""
from pathlib import Path
import argparse,importlib.util,struct
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('original_assembler',HERE.parent/'arithmetic_registers'/'build.py')
assembler=importlib.util.module_from_spec(spec);spec.loader.exec_module(assembler)
WORDS=(0x1a810002,0x1a810402,0x5a810002,0x5a810402,
       0x9a810002,0x9a810402,0xda810002,0xda810402)
VALUES=(0,1,2,0x7fffffff,0x80000000,0xffffffff,0x100000000,
        0x7fffffffffffffff,0x8000000000000000,0xffffffffffffffff,
        0xffffffff00000000,0xffffffff80000000,0x8000000000000001,
        0x0123456789abcdef,0xfedcba9876543210,0xfffffffffffffffe)
def build(short=False,wrong_capture=False):
    p=assembler.Program();domain=2 if short else 16;count=domain*domain*2048
    values=VALUES
    assert len(values)==len(set(values))==16
    p.mov(28,0xffff,16);p.put('header');p.adr(21,'table');p.mov(19,domain)
    p.mark('a_loop');p.emit(0xf94002a0);p.adr(22,'table');p.mov(20,domain)
    p.mark('b_loop');p.emit(0xf94002c1)
    for flags in range(16):
        for condition in range(16):
            for form,word in enumerate(WORDS):
                p.mov(3,flags<<12,16);p.emit(0xd51b4203)
                p.emit(word|(condition<<12));p.emit(0xd53b4203)
                if wrong_capture:p.mov(6,1);p.emit(0xca060042)
                p.emit(0xb900139f)
                for reg in (0,1,2,3):p.emit(0xf9000780|reg)
                p.mov(5,flags*128+condition*8+form);p.emit(0xf9000785)
                p.mov(5,10);p.emit(0x39000385)
    p.emit(0x910022d6);p.emit(0xf1000694);p.branch('b_loop',cond=1)
    p.emit(0x910022b5);p.emit(0xf1000673);p.branch('a_loop',cond=1)
    p.put('complete');p.emit(0xb900079f);p.mark('halt');p.branch('halt')
    p.mark('puts');p.emit(0x39400085);p.cbz(5,'return');p.emit(0x39000385);p.emit(0x91000484);p.branch('puts')
    p.mark('return');p.emit(0xd65f03c0)
    ro=bytearray()
    for label,text in (('header',f'VELOCE 1 switch select_registers\nBEGIN select cases={count} words=5\n'),('complete',f'LOG complete cases={count}\nEND observations_complete\n')):
        p.labels[label]=0x80000+len(ro);ro.extend(text.encode()+b'\0')
    while len(ro)%8:ro.append(0)
    p.labels['table']=0x80000+len(ro)
    for value in values:ro.extend(value.to_bytes(8,'little'))
    p.resolve();header=bytearray(256);header[:4]=b'NSO0'
    for n,(offset,address,data) in enumerate(((256,0,p.code),(256+len(p.code),0x80000,ro),(256+len(p.code)+len(ro),0x100000,b'\0'*16))):
        struct.pack_into('<III',header,16+n*16,offset,address,len(data))
    struct.pack_into('<I',header,60,4096)
    return bytes(header+p.code+ro+b'\0'*16)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=HERE/'select_registers.nso');args=parser.parse_args();args.output.write_bytes(build())
