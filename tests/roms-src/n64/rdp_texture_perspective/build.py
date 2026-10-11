#!/usr/bin/env python3
"""Original MIT perspective texture triangle RAM/register observations, without SDK data."""
import argparse,importlib.util,itertools
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('dp_assembler',HERE.parent/'dp_registers'/'build.py')
asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
SUITE='rdp_texture_perspective'
COORDINATES=((7,23),(31,64),(32767,32768),(65535,0))
WEIGHTS=(0,1,2,255,256,16384,32767,32768)
VARIANTS=[(kind,index,source) for kind,count in ((0,4),(1,8)) for index in range(count) for source in range(2)]
INDEX={v:i for i,v in enumerate(VARIANTS)}
def cases(short=False):
    for raw in ((0,1,2,255,256,16383,16384,17920,32767,32768,65534,65535) if short else range(65536)):
        for v in VARIANTS:yield raw,v

def texture_bytes():
    return b''.join((((x*4)<<11)|((y*4)<<6)|(((x*3+y*5)%32)<<1)|1).to_bytes(2,'big') for y in range(8) for x in range(8))

def commands(v):
    kind,index,source=v
    image=(0xfd<<56)|(2<<51)|(7<<32)|0x7f0000
    tile=(0xf5<<56)|(2<<51)|(2<<41)|(3<<14)|(3<<4)
    load=(0xf4<<56)|(28<<12)|28
    bounds=(0xf2<<56)|(28<<12)|28
    color=(0xff<<56)|(3<<51)|(3<<32)|0x700000
    mode=(0xef<<56)|(15<<36)|(3<<42)|(1<<51)
    scissor=(0xed<<56)|(16<<12)|16
    combine=(0xfc<<56)|(15<<52)|(15<<28)|(31<<47)|(3<<15)|(7<<44)|(7<<12)|(7<<41)|(3<<9)
    combine|=(15<<37)|(15<<24)|(31<<32)|(1<<6)|(7<<21)|(7<<3)|(7<<18)|1
    ss,tt=COORDINATES[index] if kind==0 else (0,0)
    w=WEIGHTS[index] if kind==1 else 0
    tri=[0xca80001000080000,(2<<48)|0xffff0000,0,65536,(ss<<48)|(tt<<32)|(w<<16),0]+[0]*6
    return [image,tile,load,bounds,color,mode,scissor,combine]+tri+[0xe900000000000000]

def build(short=False,wrong_capture=False):
    rows=list(cases(short));p=asm.Program();asm.header(p,len(rows),13)
    p.i(15,0,27,0xa070);p.i(15,0,28,0xa430);p.address(21,'templates');p.puts('header')
    p.address(8,'texture');p.i(15,0,9,0xa07f);p.i(9,0,10,32)
    p.mark('texture_copy');p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,'texture_copy')
    p.mark('sample');p.i(39,16,12,0);p.r(2,rt=12,rd=8,sa=16);p.r(0,rt=8,rd=9,sa=9);p.r(33,21,9,22)
    p.i(39,22,23,0);p.i(9,0,8,0x800);p.i(43,28,8,0)
    p.i(15,0,8,0x5a5a);p.i(13,8,8,0x5a5a);p.r(0x3c,rt=8,rd=9);p.r(37,8,9,8)
    p.i(15,0,9,0xa06f);p.i(13,9,9,0xfff8);p.i(9,0,10,10)
    p.mark('clear');p.i(63,9,8,0);p.i(9,9,9,8);p.i(9,10,10,-1);p.branch(5,10,0,'clear')
    p.i(9,22,8,8);p.i(13,27,9,256);p.r(2,rt=23,rd=10,sa=2)
    p.mark('copy');p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,'copy')
    p.i(39,22,13,4);p.i(12,13,8,256);p.branch(5,8,0,'st_parameters')
    p.i(41,27,12,356);p.branch(4,0,0,'parameters_done')
    p.mark('st_parameters');p.i(12,12,8,65535);p.r(0,rt=8,rd=9,sa=16);p.i(14,8,8,65535);p.r(37,8,9,8);p.i(43,27,8,352)
    p.mark('parameters_done')
    p.i(39,22,8,4);p.i(12,8,8,1);p.i(15,0,18,0x70);p.i(13,18,18,256);p.r(33,18,23,19);p.i(9,0,20,0x15);p.branch(4,8,0,'source_done')
    p.i(13,27,8,256);p.i(9,0,9,0xff0);p.r(2,rt=23,rd=10,sa=2)
    p.mark('xbus_copy');p.r(37,25,9,11);p.i(39,8,13,0);p.i(43,11,13,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(12,9,9,4095);p.i(9,10,10,-1);p.branch(5,10,0,'xbus_copy')
    p.i(9,0,18,0xff0);p.r(33,18,23,19);p.i(9,0,20,0x16)
    p.mark('source_done');p.i(43,24,20,12);p.i(43,24,18,0);p.i(43,24,19,4)
    p.mark('wait');p.i(39,24,8,8);p.branch(5,8,19,'wait')
    p.puts('row');p.r(37,27,0,18);p.i(9,0,20,8)
    p.mark('capture');p.i(55,18,5,0)
    if wrong_capture:p.i(14,5,5,1)
    p.jump(3,'hex');p.i(9,18,18,8);p.i(9,20,20,-1);p.branch(5,20,0,'capture')
    p.i(15,0,8,0xa06f);p.i(13,8,8,0xfff8);p.i(55,8,5,0);p.jump(3,'hex');p.i(55,27,5,64);p.jump(3,'hex')
    for addr,offset in ((24,8),(24,12),(28,8)):p.i(39,addr,5,offset);p.jump(3,'hex')
    p.i(9,0,8,10);p.i(40,30,8,0);p.i(9,16,16,4);p.i(9,17,17,-1);p.branch(5,17,0,'sample')
    p.puts('complete');p.i(43,30,0,4);p.mark('halt');p.branch(4,0,0,'halt');asm.output_routines(p)
    p.string('header',f'VELOCE 1 n64 {SUITE}\nBEGIN data cases={len(rows)} words=13\n');p.string('row','REG r=');p.string('complete',f'LOG complete cases={len(rows)}\nEND observations_complete\n')
    while p.pc&7:p.data.append(0)
    p.mark('templates')
    for v in VARIANTS:
        words=commands(v);record=(len(words)*8).to_bytes(4,'big')+(v[2]|(v[0]<<8)).to_bytes(4,'big')+b''.join(w.to_bytes(8,'big') for w in words)
        assert len(record)<=512;p.data.extend(record+b'\0'*(512-len(record)))
    p.mark('texture');p.data.extend(texture_bytes())
    p.mark('table')
    for raw,v in rows:p.word(raw|(INDEX[v]<<16))
    p.resolve();return bytes(p.data)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=HERE/(SUITE+'.z64'));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
