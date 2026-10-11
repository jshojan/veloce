#!/usr/bin/env python3
"""Original MIT affine texture triangle RAM/register observations, without SDK data."""
import argparse,importlib.util,itertools
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('dp_assembler',HERE.parent/'dp_registers'/'build.py')
asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
SUITE='rdp_color_convert'
VARIANTS=[(field,source,0) for field in range(4) for source in range(2)]+[(4,0,1)]+[(5,source,profile) for profile in range(2,6) for source in range(2)]
INDEX={v:i for i,v in enumerate(VARIANTS)}
BASE=(175,469,423,222,114,42)
def cases(short=False):
    coefficients=(0,1,255,256,383,384,511) if short else range(512)
    seeds=(0,1,127,128,254,255) if short else range(256)
    for k,seed,field,source in itertools.product(coefficients,seeds,range(4),range(2)):
        yield (k<<9)|seed,(field,source,0)
    for k4,k5 in itertools.product(coefficients,coefficients):yield (k4<<9)|k5,(4,0,1)
    coordinates=((0,0),(7,23),(16,16),(25,17),(31,31)) if short else itertools.product(range(32),range(32))
    for ss,tt in coordinates:
        for v in VARIANTS[9:]:yield ss|(tt<<5),v

def commands(v):
    field,source,profile=v
    f,size=((1,2),(1,3),(0,2),(3,2))[profile-2] if profile>=2 else ((0,2) if profile==1 else (1,2))
    width=16>>size
    image=(0xfd<<56)|(size<<51)|((width-1)<<32)|0x7f0000
    tile=(0xf5<<56)|(f<<53)|(size<<51)|(1<<41)
    load=(0xf4<<56)|(((width-1)*4)<<12)|4
    bounds=(0xf2<<56)|(((width-1)*4)<<12)|4
    color=(0xff<<56)|(3<<51)|(3<<32)|0x700000
    mode=(0xef<<56)|(15<<36)|((3<<42) if profile==1 else (1<<45))
    scissor=(0xed<<56)|(16<<12)|16
    combine=(0xfc<<56)|(15<<52)|(15<<28)|(31<<47)|(3<<15)|(7<<44)|(7<<12)|(7<<41)|(3<<9)
    a,b,c,d=(1,7,15,1) if profile==1 else (15,15,31,1)
    combine|=(a<<37)|(b<<24)|(c<<32)|(d<<6)|(7<<21)|(7<<3)|(7<<18)|1
    coefficients=list(BASE)
    if field<4:coefficients[field]=0
    if profile==1:coefficients[4]=coefficients[5]=0
    convert=0xec00000000000000
    for n,value in enumerate(coefficients):convert|=value<<(45-9*n)
    tri=[0xca80001000080000,(2<<48)|0xffff0000,0,65536,(7<<48)|(23<<32)]+[0]*7
    return [image,tile,load,bounds,color,mode,scissor,combine,convert]+tri+[0xe900000000000000]

def build(short=False,wrong_capture=False):
    rows=list(cases(short));p=asm.Program();asm.header(p,len(rows),13)
    p.i(15,0,27,0xa070);p.i(15,0,28,0xa430);p.address(21,'templates');p.puts('header')
    p.mark('sample');p.i(39,16,12,0);p.r(2,rt=12,rd=8,sa=18);p.r(0,rt=8,rd=9,sa=9);p.r(33,21,9,22)
    p.i(39,22,23,0);p.i(9,0,8,0x800);p.i(43,28,8,0)
    p.i(15,0,8,0x5a5a);p.i(13,8,8,0x5a5a);p.r(0x3c,rt=8,rd=9);p.r(37,8,9,8)
    p.i(15,0,9,0xa06f);p.i(13,9,9,0xfff8);p.i(9,0,10,10)
    p.mark('clear');p.i(63,9,8,0);p.i(9,9,9,8);p.i(9,10,10,-1);p.branch(5,10,0,'clear')
    p.i(9,22,8,8);p.i(13,27,9,256);p.r(2,rt=23,rd=10,sa=2)
    p.mark('copy');p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,'copy')
    # Source byte repeats into a 16-bit seed; rows retain the complemented pattern.
    p.i(12,12,8,255);p.r(0,rt=8,rd=9,sa=8);p.r(37,8,9,8)
    p.r(0,rt=8,rd=9,sa=16);p.i(14,8,8,65535);p.r(37,8,9,8)
    p.r(0x3c,rt=8,rd=8);p.r(0x3e,rt=8,rd=8);p.r(0x3c,rt=8,rd=9);p.r(37,8,9,8);p.i(15,0,9,0xa07f);p.i(63,9,8,0)
    p.r(39,8,0,10);p.i(63,9,10,8)
    p.i(39,22,13,4);p.r(2,rt=13,rd=9,sa=17);p.branch(5,9,0,'grid_coordinates')
    p.r(2,rt=13,rd=9,sa=16);p.branch(5,9,0,'pair_coefficients')
    p.r(2,rt=12,rd=8,sa=9);p.i(12,8,8,511);p.r(2,rt=13,rd=9,sa=8);p.i(12,9,9,63);p.r(0x14,9,8,8);p.branch(4,0,0,'update_convert')
    p.mark('pair_coefficients');p.i(15,0,9,3);p.i(13,9,9,65535);p.r(36,12,9,8)
    p.mark('update_convert');p.i(55,27,10,320);p.r(37,8,10,8);p.i(63,27,8,320);p.branch(4,0,0,'parameters_done')
    p.mark('grid_coordinates');p.i(12,12,8,31);p.r(0,rt=8,rd=8,sa=16);p.r(2,rt=12,rd=9,sa=5);p.i(12,9,9,31);p.r(37,8,9,8);p.i(43,27,8,360)
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
        words=commands(v);field,source,profile=v;metadata=source|((45-9*field if field<4 else 0)<<8)|((field>=4)<<16)|((field==5)<<17);record=(len(words)*8).to_bytes(4,'big')+metadata.to_bytes(4,'big')+b''.join(w.to_bytes(8,'big') for w in words)
        assert len(record)<=512;p.data.extend(record+b'\0'*(512-len(record)))
    p.mark('table')
    for parameters,v in rows:p.word(parameters|(INDEX[v]<<18))
    p.resolve();return bytes(p.data)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=HERE/(SUITE+'.z64'));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
