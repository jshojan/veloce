#!/usr/bin/env python3
"""Original MIT one-cycle triangle RAM observations; no SDK or expected output."""
import argparse,importlib.util,itertools
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('dp_assembler',HERE.parent/'dp_registers'/'build.py')
asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
spec=importlib.util.spec_from_file_location('fill_geometry',HERE.parent/'rdp_triangle_fill'/'build.py')
fill=importlib.util.module_from_spec(spec);spec.loader.exec_module(fill)
SUITE='rdp_triangle_shade'
VARIANTS=list(itertools.product(range(8),range(2),range(8),range(3),range(2),range(4),range(2)))
INDEX={v:i for i,v in enumerate(VARIANTS)}
PROGRAMS=((15,15,31,4,7,7,7,4),(15,15,31,3,7,7,7,3),(15,15,31,5,7,7,7,5),(4,8,10,7,4,7,3,7),(3,5,4,5,3,5,4,5),(15,15,31,6,7,7,7,6),(15,15,31,7,7,7,7,7),(4,3,14,5,4,3,6,5))
def cases(short=False):
    for raw in ((0,1,0x7fff,0x8000,0xfffe,0xffff) if short else range(65536)):
        for program,source,flip in itertools.product((0,3,4),range(2),range(2)):
            yield raw,(0,flip,program,0,source,0,0)
    for v in VARIANTS:yield 0x1357,v

def commands(v):
    g,flip,program,field,source,destination,read=v
    yh,ym,yl,xh,xm,xl,dh,dm,dl,clip=fill.geometry(g,flip);sl,st,sr,sb=clip
    image=(0xff<<56)|(3<<51)|(3<<32)|0x700000
    mode=(0xef<<56)|(15<<36)|(destination<<8)|(read<<6)
    scissors=(0xed<<56)|(sl<<44)|(st<<32)|(sr<<12)|sb
    if field:scissors|=(1<<25)|((field==2)<<24)
    head=(0xcc<<56)|(flip<<55)|((yl&16383)<<32)|((ym&16383)<<16)|(yh&16383)
    tri=[head,((xl&0xffffffff)<<32)|(dl&0xffffffff),((xh&0xffffffff)<<32)|(dh&0xffffffff),((xm&0xffffffff)<<32)|(dm&0xffffffff)]
    colors=((0,0,0,0),(65536,-65536,32768,16384),(32768,32768,-32768,0),(16384,65536,0,-16384))
    coefficients=[]
    for pair in (0,2):
        for fractional in (False,True):
            for group in (pair,pair+1):
                word=0
                for channel in colors[group]:word=(word<<16)|((channel&65535) if fractional else ((channel>>16)&65535))
                coefficients.append(word)
    tri+=coefficients
    # Cycle zero has a different pass-primitive program. One-cycle execution
    # must use cycle one; the guest does not contain expected framebuffer data.
    combine=0xfc00000000000000|(15<<52)|(15<<28)|(31<<47)|(3<<15)|(7<<44)|(7<<12)|(7<<41)|(3<<9)
    for value,shift in zip(PROGRAMS[program],(37,24,32,6,21,3,18,0)):combine|=value<<shift
    return [image,mode,scissors,(0xfa<<56)|(73<<32),0xfb00000000000000,combine]+tri+[0xe900000000000000]
def build(short=False,wrong_capture=False):
    rows=list(cases(short));p=asm.Program();asm.header(p,len(rows),13)
    p.i(15,0,27,0xa070);p.i(15,0,28,0xa430);p.address(21,'templates');p.puts('header')
    p.mark('sample');p.i(39,16,12,0);p.r(2,rt=12,rd=8,sa=16)
    p.r(0,rt=8,rd=9,sa=8);p.r(33,21,9,22)
    p.i(39,22,23,0);p.i(9,0,8,0x800);p.i(43,28,8,0)
    p.i(15,0,8,0x5a5a);p.i(13,8,8,0x5a5a);p.r(0x3c,rt=8,rd=9);p.r(37,8,9,8)
    p.i(15,0,9,0xa06f);p.i(13,9,9,0xfff8);p.i(9,0,10,10)
    p.mark('clear');p.i(63,9,8,0);p.i(9,9,9,8);p.i(9,10,10,-1);p.branch(5,10,0,'clear')
    p.i(9,22,8,8);p.i(13,27,9,256);p.r(2,rt=23,rd=10,sa=2)
    p.mark('copy');p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,'copy')
    p.i(12,12,8,65535);p.r(0,rt=8,rd=9,sa=16);p.i(14,8,8,65535);p.r(37,8,9,8);p.i(43,27,8,284);p.i(15,0,10,0xffff);p.i(13,10,10,65535);p.r(38,8,10,11);p.i(43,27,11,292)
    p.i(12,12,8,65535);p.r(2,rt=8,rd=9,sa=8);p.i(12,8,10,255);p.r(0,rt=9,rd=11,sa=16);p.r(37,11,10,11);p.i(43,27,11,336)
    p.i(14,9,9,255);p.i(14,10,10,255);p.r(0,rt=9,rd=11,sa=16);p.r(37,11,10,11);p.i(43,27,11,340)
    p.i(39,22,8,4);p.i(15,0,18,0x70);p.i(13,18,18,256);p.r(33,18,23,19);p.i(9,0,20,0x15);p.branch(4,8,0,'source_done')
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
        words=commands(v);record=(len(words)*8).to_bytes(4,'big')+v[4].to_bytes(4,'big')+b''.join(w.to_bytes(8,'big') for w in words)
        assert len(record)<=256;p.data.extend(record+b'\0'*(256-len(record)))
    p.mark('table')
    for raw,v in rows:p.word(raw|(INDEX[v]<<16))
    p.resolve();return bytes(p.data)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=HERE/(SUITE+'.z64'));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
