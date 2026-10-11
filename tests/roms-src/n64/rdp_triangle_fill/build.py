#!/usr/bin/env python3
"""Original MIT triangle framebuffer observations; no SDK or expected output."""
import argparse,importlib.util,itertools
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('dp_assembler',HERE.parent/'dp_registers'/'build.py')
asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
SUITE='rdp_triangle_fill'
VARIANTS=list(itertools.product(range(8),range(2),range(32),range(3),range(2),(1,2,3)))
INDEX={v:i for i,v in enumerate(VARIANTS)}
def cases(short=False):
    for raw in ((0,1,0x7fff,0x8000,0xfffe,0xffff) if short else range(65536)):
        for size,source,flip in itertools.product((1,2,3),range(2),range(2)):
            yield raw,(0,flip,7,0,source,size)
    for v in VARIANTS:yield 0x1357,v
def geometry(g,flip):
    yh,ym,yl=0,8,16;xh,xm,xl=0,0,2*65536;dh,dm,dl=0,65536,-65536;clip=(0,0,16,16)
    if g==1:yh,ym,yl=1,9,15
    if g==2:yh=-4;xm=-65536
    if g==3:clip=(5,2,13,14)
    if g==4:ym=7;xl=5*32768;dl=-49152
    if g==5:yh,ym,yl=1,5,11;xh=81920;xm=xl=114688;dm=dl=0
    if g==6:yl=ym=0
    if g==7:xh=3*65536
    if not flip:xh,xm,xl=(2*65536-x for x in (xh,xm,xl));dh,dm,dl=-dh,-dm,-dl
    return yh,ym,yl,xh,xm,xl,dh,dm,dl,clip
def commands(v):
    g,flip,encoded,field,source,size=v;op=8+(encoded&7);alias=encoded>>3
    yh,ym,yl,xh,xm,xl,dh,dm,dl,clip=geometry(g,flip);sl,st,sr,sb=clip
    image=(0xff<<56)|((2 if size==1 else 0)<<53)|(size<<51)|(3<<32)|0x700000
    scissors=(0xed<<56)|(sl<<44)|(st<<32)|(sr<<12)|sb
    if field:scissors|=(1<<25)|((field==2)<<24)
    head=((op+alias*64)<<56)|(flip<<55)|((yl&16383)<<32)|((ym&16383)<<16)|(yh&16383)
    tri=[head,((xl&0xffffffff)<<32)|(dl&0xffffffff),((xh&0xffffffff)<<32)|(dh&0xffffffff),((xm&0xffffffff)<<32)|(dm&0xffffffff)]
    length=(4,6,12,14,12,14,20,22)[op-8]
    tri.extend(0x2900000000000000+n for n in range(length-4))
    return [image,0xef30000000000000,scissors,0xf700000000000000]+tri+[0xe900000000000000]
def build(short=False,wrong_capture=False):
    rows=list(cases(short));p=asm.Program();asm.header(p,len(rows),13)
    p.i(15,0,27,0xa070);p.i(15,0,28,0xa430);p.address(21,'templates');p.puts('header')
    p.mark('sample');p.i(39,16,12,0);p.r(2,rt=12,rd=8,sa=16)
    p.r(0,rt=8,rd=9,sa=5);p.r(0,rt=8,rd=10,sa=6);p.r(0,rt=8,rd=11,sa=7);p.r(33,9,10,9);p.r(33,9,11,9);p.r(33,21,9,22)
    p.i(39,22,23,0);p.i(9,0,8,0x800);p.i(43,28,8,0)
    p.i(15,0,8,0x5a5a);p.i(13,8,8,0x5a5a);p.r(0x3c,rt=8,rd=9);p.r(37,8,9,8)
    p.i(15,0,9,0xa06f);p.i(13,9,9,0xfff8);p.i(9,0,10,10)
    p.mark('clear');p.i(63,9,8,0);p.i(9,9,9,8);p.i(9,10,10,-1);p.branch(5,10,0,'clear')
    p.i(9,22,8,8);p.i(13,27,9,256);p.r(2,rt=23,rd=10,sa=2)
    p.mark('copy');p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,'copy')
    p.i(12,12,8,65535);p.r(0,rt=8,rd=9,sa=16);p.i(14,8,8,65535);p.r(37,8,9,8);p.i(43,27,8,284)
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
        assert len(record)<=224;p.data.extend(record+b'\0'*(224-len(record)))
    p.mark('table')
    for raw,v in rows:p.word(raw|(INDEX[v]<<16))
    p.resolve();return bytes(p.data)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=HERE/(SUITE+'.z64'));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
