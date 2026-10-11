#!/usr/bin/env python3
"""Original MIT AArch64 logical-immediate NSO0; no SDK or firmware."""
from pathlib import Path
import argparse,importlib.util,struct
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('original_assembler',HERE.parent/'arithmetic_registers'/'build.py')
assembler=importlib.util.module_from_spec(spec);spec.loader.exec_module(assembler)
class Program(assembler.Program):
    def adr(self,reg,label):
        self.fixups.append((self.pc,'page',label));self.emit(0x90000000|reg)
        self.emit(0x91000000|(reg<<5)|reg)
    def resolve(self):
        pages=[x for x in self.fixups if x[1]=='page']
        self.fixups=[x for x in self.fixups if x[1]!='page'];super().resolve()
        for at,_,label in pages:
            target=self.labels[label];offset=(target//4096)-(at//4096)
            assert -(1<<20)<=offset<(1<<20)
            w=struct.unpack_from('<I',self.code,at)[0];value=offset&0x1fffff
            struct.pack_into('<I',self.code,at,w|((value&3)<<29)|((value>>2)<<5))
            w=struct.unpack_from('<I',self.code,at+4)[0]
            struct.pack_into('<I',self.code,at+4,w|((target&4095)<<10))
def cases(short=False):
    for width in (32,64):
        patterns=((0,0,60),(0,63,60),(0,0,0),(int(width==64),width-1,width-2)) if short else (
            (n,r,end) for n in range(2) for r in range(64) for end in range(64))
        for n,r,end in patterns:
            length=((n<<6)|(~end&63)).bit_length()-1
            if length<1 or (width==32 and (n or length>5)):continue
            levels=(1<<length)-1
            if end&levels==levels:continue
            for op in range(4):
                yield 0x12000002|(0x80000000 if width==64 else 0)|(op<<29)|(n<<22)|(r<<16)|(end<<10)
VALUES=(0,1,2,0x7fffffff,0x80000000,0xffffffff,0x100000000,
        0x7fffffffffffffff,0x8000000000000000,0xffffffffffffffff,
        0xffffffff00000000,0xffffffff80000000,0x8000000000000001,
        0x0123456789abcdef,0xfedcba9876543210,0xfffffffffffffffe)
def build(short=False,wrong_capture=False):
    p=Program();domain=2 if short else 16;words=tuple(cases(short));count=domain*2*len(words)
    assert len(words)==(32 if short else 45312)
    p.mov(28,0xffff,16);p.put('header');p.adr(21,'table');p.mov(19,domain)
    p.mark('a_loop');p.emit(0xf94002a0);p.mov(6,0x9000,16);p.mov(23,2)
    p.mark('seed_loop')
    for index,word in enumerate(words):
        p.emit(0xd51b4206);p.emit(word);p.emit(0xd53b4203)
        if wrong_capture:p.mov(7,1);p.emit(0xca070042)
        p.emit(0xb900139f)
        for reg in (0,2,3,6):p.emit(0xf9000780|reg)
        p.mov(5,index);p.emit(0xf9000785);p.mov(5,10);p.emit(0x39000385)
    p.mov(7,0x6000,16);p.emit(0x8b0700c6);p.emit(0xf10006f7)
    p.branch('seed_done',cond=0);p.branch('seed_loop');p.mark('seed_done')
    p.emit(0x910022b5);p.emit(0xf1000673);p.branch('inputs_done',cond=0)
    p.branch('a_loop');p.mark('inputs_done')
    p.put('complete');p.emit(0xb900079f);p.mark('halt');p.branch('halt')
    p.mark('puts');p.emit(0x39400085);p.cbz(5,'return');p.emit(0x39000385);p.emit(0x91000484);p.branch('puts')
    p.mark('return');p.emit(0xd65f03c0)
    ro=bytearray()
    for label,text in (('header',f'VELOCE 1 switch logical_imm_registers\nBEGIN logical_imm cases={count} words=5\n'),('complete',f'LOG complete cases={count}\nEND observations_complete\n')):
        p.labels[label]=0x300000+len(ro);ro.extend(text.encode()+b'\0')
    while len(ro)%8:ro.append(0)
    p.labels['table']=0x300000+len(ro)
    for value in VALUES:ro.extend(value.to_bytes(8,'little'))
    p.resolve();header=bytearray(256);header[:4]=b'NSO0'
    for n,(offset,address,data) in enumerate(((256,0,p.code),(256+len(p.code),0x300000,ro),(256+len(p.code)+len(ro),0x380000,b'\0'*16))):
        struct.pack_into('<III',header,16+n*16,offset,address,len(data))
    struct.pack_into('<I',header,60,4096)
    return bytes(header+p.code+ro+b'\0'*16)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=HERE/'logical_imm_registers.nso');args=parser.parse_args();args.output.write_bytes(build())
